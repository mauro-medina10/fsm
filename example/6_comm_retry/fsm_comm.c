/**
 * @file fsm_comm.c
 * @author Mauro Medina
 * @brief Connection & handshake manager FSM showcasing guards, event payloads,
 *        exit cleanup, composite FSM_ST_END/FSM_END_EV, self-transitions, and fsm_terminate()
 * @version 2.0.0
 * @date 2026
 *
 * @copyright Copyright (c) 2026
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

#define MAX_HANDSHAKE_RETRIES 2U
#define HANDSHAKE_TIMEOUT_MS  100U
#define LINK_WATCHDOG_MS      500U
#define FATAL_FAULT_CODE      42

typedef struct {
    unsigned int retries;
    unsigned int messages_rx;
    bool session_open;
} comm_ctx_t;

typedef struct {
    unsigned int seq;
    const char *payload;
} comm_msg_t;

// Define states
enum {
    ST_ROOT = FSM_ST_FIRST,
    ST_DISCONNECTED,
    ST_HANDSHAKE,
    ST_AUTH,
    ST_SYNC,
    ST_CONNECTED,
    ST_ACTIVE_LINK,
    ST_FAULT,
};

// Define events
enum {
    EV_CONNECT = FSM_EV_FIRST,
    EV_AUTH_OK,
    EV_SYNC_OK,
    EV_RX_DATA,
    EV_DISCONNECT,
    EV_LAST,
};

// Prototypes
static void enter_disconnected(fsm_t *self, void *data);
static void enter_handshake(fsm_t *self, void *data);
static void enter_auth(fsm_t *self, void *data);
static void enter_sync(fsm_t *self, void *data);
static void enter_connected(fsm_t *self, void *data);
static void exit_connected(fsm_t *self, void *data);
static void enter_fault(fsm_t *self, void *data);

static bool guard_can_retry(fsm_t *self, void *data);
static bool guard_retries_exhausted(fsm_t *self, void *data);

static void work_inc_retry(fsm_t *self, void *data);
static void work_max_retries(fsm_t *self, void *data);
static void work_process_msg(fsm_t *self, void *data);

// Define FSM states
FSM_STATES_INIT(comm)
//               name  state id         parent        sub             entry               run   exit
FSM_CREATE_STATE(comm, ST_ROOT,         FSM_ST_NONE,  ST_DISCONNECTED, NULL,              NULL, NULL)
FSM_CREATE_STATE(comm, ST_DISCONNECTED, ST_ROOT,      FSM_ST_NONE,    enter_disconnected, NULL, NULL)
FSM_CREATE_STATE(comm, ST_HANDSHAKE,    ST_ROOT,      ST_AUTH,        enter_handshake,    NULL, NULL)
FSM_CREATE_STATE(comm, ST_AUTH,         ST_HANDSHAKE, FSM_ST_NONE,    enter_auth,         NULL, NULL)
FSM_CREATE_STATE(comm, ST_SYNC,         ST_HANDSHAKE, FSM_ST_NONE,    enter_sync,         NULL, NULL)
FSM_CREATE_STATE(comm, ST_CONNECTED,    ST_ROOT,      ST_ACTIVE_LINK, enter_connected,    NULL, exit_connected)
FSM_CREATE_STATE(comm, ST_ACTIVE_LINK,  ST_CONNECTED, FSM_ST_NONE,    NULL,               NULL, NULL)
FSM_CREATE_STATE(comm, ST_FAULT,        ST_ROOT,      FSM_ST_NONE,    enter_fault,        NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(comm)
//                          fsm   source           event           target          guard                    work
FSM_TRANSITION_CREATE(      comm, ST_DISCONNECTED, EV_CONNECT,     ST_HANDSHAKE)
FSM_TRANSITION_CREATE(      comm, ST_AUTH,         EV_AUTH_OK,     ST_SYNC)
FSM_TRANSITION_CREATE(      comm, ST_SYNC,         EV_SYNC_OK,     FSM_ST_END)
FSM_TRANSITION_CREATE(      comm, ST_HANDSHAKE,    FSM_END_EV,     ST_CONNECTED)
FSM_TRANSITION_GUARD_CREATE(comm, ST_HANDSHAKE,    FSM_TIMEOUT_EV, ST_HANDSHAKE,   guard_can_retry,         work_inc_retry)
FSM_TRANSITION_GUARD_CREATE(comm, ST_HANDSHAKE,    FSM_TIMEOUT_EV, ST_FAULT,       guard_retries_exhausted, work_max_retries)
FSM_TRANSITION_WORK_CREATE( comm, ST_ACTIVE_LINK,  EV_RX_DATA,     ST_ACTIVE_LINK,                          work_process_msg)
FSM_TRANSITION_CREATE(      comm, ST_ACTIVE_LINK,  FSM_TIMEOUT_EV, ST_DISCONNECTED)
FSM_TRANSITION_CREATE(      comm, ST_CONNECTED,    EV_DISCONNECT,  ST_DISCONNECTED)
FSM_TRANSITIONS_END()

// Guards
static bool guard_can_retry(fsm_t *self, void *data)
{
    (void)data;
    const comm_ctx_t *ctx = (const comm_ctx_t *)fsm_data_get(self);
    return ctx->retries < MAX_HANDSHAKE_RETRIES;
}

static bool guard_retries_exhausted(fsm_t *self, void *data)
{
    (void)data;
    const comm_ctx_t *ctx = (const comm_ctx_t *)fsm_data_get(self);
    return ctx->retries >= MAX_HANDSHAKE_RETRIES;
}

// Transition actions
static void work_inc_retry(fsm_t *self, void *data)
{
    (void)data;
    comm_ctx_t *ctx = (comm_ctx_t *)fsm_data_get(self);
    ctx->retries++;
    printf("Handshake timeout -> retry %u/%u\n", ctx->retries, MAX_HANDSHAKE_RETRIES);
}

static void work_max_retries(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("Handshake failed after max retries\n");
}

static void work_process_msg(fsm_t *self, void *data)
{
    comm_ctx_t *ctx = (comm_ctx_t *)fsm_data_get(self);
    const comm_msg_t *msg = (const comm_msg_t *)data;
    if (msg != NULL) {
        ctx->messages_rx++;
        printf("RX msg #%u: \"%s\" (watchdog reset)\n", msg->seq, msg->payload);
    }
}

// State actions
static void enter_disconnected(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("State: DISCONNECTED\n");
}

static void enter_handshake(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("State: HANDSHAKE\n");
}

static void enter_auth(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("  Substate: AUTH\n");
}

static void enter_sync(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("  Substate: SYNC\n");
}

static void enter_connected(fsm_t *self, void *data)
{
    (void)data;
    comm_ctx_t *ctx = (comm_ctx_t *)fsm_data_get(self);
    ctx->retries = 0U;
    ctx->session_open = true;
    printf("State: CONNECTED (session opened)\n");
}

static void exit_connected(fsm_t *self, void *data)
{
    (void)data;
    comm_ctx_t *ctx = (comm_ctx_t *)fsm_data_get(self);
    ctx->session_open = false;
    printf("Leaving CONNECTED (session closed)\n");
}

static void enter_fault(fsm_t *self, void *data)
{
    (void)data;
    printf("State: FAULT -> terminating FSM (code %d)\n", FATAL_FAULT_CODE);
    fsm_terminate(self, FATAL_FAULT_CODE);
}

static int step_ticks(fsm_t *fsm, unsigned int ticks)
{
    int rc = 0;
    for (unsigned int i = 0; i < ticks; i++) {
        fsm_ticks_hook(fsm);
        if (fsm_has_pending_events(fsm) > 0) {
            rc = fsm_run(fsm);
            if (rc != 0) {
                return rc;
            }
        }
    }
    return rc;
}

int main(void)
{
    fsm_t comm = {0};
    comm_ctx_t ctx = { .retries = 0U, .messages_rx = 0U, .session_open = false };

    printf("--- Starting Comm Retry FSM ---\n");

    if (fsm_init(&comm, FSM_TRANSITIONS_GET(comm), FSM_TRANSITIONS_SIZE(comm),
                 EV_LAST, 1, &FSM_STATE_GET(comm, ST_ROOT), &ctx) != 0)
    {
        return EXIT_FAILURE;
    }

    // Hierarchical timeout on ST_HANDSHAKE (applies across ST_AUTH and ST_SYNC)
    fsm_timed_event_set(&comm, &FSM_STATE_GET(comm, ST_HANDSHAKE),   FSM_MS_2_TICKS(&comm, HANDSHAKE_TIMEOUT_MS));
    // Inactivity watchdog on ST_ACTIVE_LINK
    fsm_timed_event_set(&comm, &FSM_STATE_GET(comm, ST_ACTIVE_LINK), FSM_MS_2_TICKS(&comm, LINK_WATCHDOG_MS));

    // 1. Start handshake, experience 1 timeout (guarded retry), then succeed via FSM_ST_END -> FSM_END_EV
    fsm_dispatch(&comm, EV_CONNECT, NULL);
    fsm_run(&comm);
    step_ticks(&comm, HANDSHAKE_TIMEOUT_MS);

    fsm_dispatch(&comm, EV_AUTH_OK, NULL);
    fsm_dispatch(&comm, EV_SYNC_OK, NULL);
    fsm_run(&comm);

    // 2. Receive data with per-event payload at 300 ms -> resets 500 ms watchdog without closing session
    step_ticks(&comm, 300U);
    comm_msg_t msg = { .seq = 1U, .payload = "HELLO_ESP" };
    fsm_dispatch(&comm, EV_RX_DATA, &msg);
    fsm_run(&comm);

    // 3. 300 ms later still connected (only 300/500 ms elapsed); 200 ms more triggers watchdog -> session closed
    step_ticks(&comm, 300U);
    if (!ctx.session_open || fsm_state_get(&comm) != ST_ACTIVE_LINK) {
        return EXIT_FAILURE;
    }
    step_ticks(&comm, 200U);
    if (ctx.session_open || fsm_state_get(&comm) != ST_DISCONNECTED) {
        return EXIT_FAILURE;
    }

    // 4. Reconnect and let all retries expire -> ST_FAULT -> fsm_terminate(FATAL_FAULT_CODE)
    fsm_dispatch(&comm, EV_CONNECT, NULL);
    fsm_run(&comm);
    int term_rc = step_ticks(&comm, HANDSHAKE_TIMEOUT_MS * (MAX_HANDSHAKE_RETRIES + 1U));

    printf("Terminated with code: %d (rx=%u)\n", term_rc, ctx.messages_rx);
    fsm_deinit(&comm);

    return (term_rc == FATAL_FAULT_CODE && ctx.messages_rx == 1U) ? EXIT_SUCCESS : EXIT_FAILURE;
}
