/**
 * @file fsm_multi_led.c
 * @author Mauro Medina
 * @brief Multi-instance FSM example: two LEDs sharing one const state/transition table
 * @version 2.0.0
 * @date 2026
 *
 * @copyright Copyright (c) 2026
 */
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

typedef struct {
    const char *name;
    int pin;
    int level;
    unsigned int toggles;
} led_ctx_t;

// Define states
enum {
    ST_ROOT = FSM_ST_FIRST,
    ST_OFF,
    ST_ON,
};

// Define events
enum {
    EV_STOP = FSM_EV_FIRST,
    EV_START,
    EV_LAST,
};

// Action function prototypes
static void enter_off(fsm_t *self, void *data);
static void enter_on(fsm_t *self, void *data);
static void exit_on(fsm_t *self, void *data);

// Single immutable state table shared by all LED instances (stored in Flash/RODATA)
FSM_STATES_INIT(multi_led)
//               name       state id  parent       sub          entry      run   exit
FSM_CREATE_STATE(multi_led, ST_ROOT,  FSM_ST_NONE, ST_OFF,      NULL,      NULL, NULL)
FSM_CREATE_STATE(multi_led, ST_OFF,   ST_ROOT,     FSM_ST_NONE, enter_off, NULL, NULL)
FSM_CREATE_STATE(multi_led, ST_ON,    ST_ROOT,     FSM_ST_NONE, enter_on,  NULL, exit_on)
FSM_STATES_END()

// Single immutable transition table shared by all LED instances
FSM_TRANSITIONS_INIT(multi_led)
//                    fsm name   State source  event           state target
FSM_TRANSITION_CREATE(multi_led, ST_OFF,       FSM_TIMEOUT_EV, ST_ON)
FSM_TRANSITION_CREATE(multi_led, ST_ON,        FSM_TIMEOUT_EV, ST_OFF)
FSM_TRANSITION_CREATE(multi_led, ST_ON,        EV_STOP,        ST_OFF)
FSM_TRANSITION_CREATE(multi_led, ST_OFF,       EV_START,       ST_ON)
FSM_TRANSITIONS_END()

static void enter_off(fsm_t *self, void *data)
{
    (void)data;
    led_ctx_t *ctx = (led_ctx_t *)fsm_data_get(self);
    ctx->level = 0;
    printf("[%s pin=%d] OFF\n", ctx->name, ctx->pin);
}

static void enter_on(fsm_t *self, void *data)
{
    (void)data;
    led_ctx_t *ctx = (led_ctx_t *)fsm_data_get(self);
    ctx->level = 1;
    ctx->toggles++;
    printf("[%s pin=%d] ON (toggle #%u)\n", ctx->name, ctx->pin, ctx->toggles);
}

static void exit_on(fsm_t *self, void *data)
{
    (void)data;
    led_ctx_t *ctx = (led_ctx_t *)fsm_data_get(self);
    printf("[%s pin=%d] leaving ON\n", ctx->name, ctx->pin);
}

int main(void)
{
    fsm_t status_fsm = {0};
    fsm_t error_fsm  = {0};

    led_ctx_t status_ctx = { .name = "STATUS", .pin = 2, .level = 0, .toggles = 0 };
    led_ctx_t error_ctx  = { .name = "ERROR",  .pin = 5, .level = 0, .toggles = 0 };

    printf("--- Starting Multi-Instance LED FSM ---\n");

    if (fsm_init(&status_fsm, FSM_TRANSITIONS_GET(multi_led), FSM_TRANSITIONS_SIZE(multi_led),
                 EV_LAST, 1, &FSM_STATE_GET(multi_led, ST_ROOT), &status_ctx) != 0)
    {
        return EXIT_FAILURE;
    }

    if (fsm_init(&error_fsm, FSM_TRANSITIONS_GET(multi_led), FSM_TRANSITIONS_SIZE(multi_led),
                 EV_LAST, 1, &FSM_STATE_GET(multi_led, ST_ROOT), &error_ctx) != 0)
    {
        return EXIT_FAILURE;
    }

    // Independent per-instance periods using the exact same const state table
    fsm_timed_event_set(&status_fsm, &FSM_STATE_GET(multi_led, ST_OFF), FSM_MS_2_TICKS(&status_fsm, 100U));
    fsm_timed_event_set(&status_fsm, &FSM_STATE_GET(multi_led, ST_ON),  FSM_MS_2_TICKS(&status_fsm, 100U));

    fsm_timed_event_set(&error_fsm, &FSM_STATE_GET(multi_led, ST_OFF), FSM_MS_2_TICKS(&error_fsm, 300U));
    fsm_timed_event_set(&error_fsm, &FSM_STATE_GET(multi_led, ST_ON),  FSM_MS_2_TICKS(&error_fsm, 300U));

    // Simulate 600 ms: STATUS enters ON at 100, 300, 500 ms (3x); ERROR enters ON at 300 ms (1x)
    int ret = 0;
    for (unsigned int ms = 0; ms < 600U; ms++) {
        fsm_ticks_hook(&status_fsm);
        fsm_ticks_hook(&error_fsm);

        if (fsm_has_pending_events(&status_fsm) > 0) {
            ret |= fsm_run(&status_fsm);
        }
        if (fsm_has_pending_events(&error_fsm) > 0) {
            ret |= fsm_run(&error_fsm);
        }
    }

    printf("Summary: STATUS toggles=%u, ERROR toggles=%u\n", status_ctx.toggles, error_ctx.toggles);

    fsm_deinit(&status_fsm);
    fsm_deinit(&error_fsm);

    return (ret == 0 && status_ctx.toggles == 3U && error_ctx.toggles == 1U) ? EXIT_SUCCESS : EXIT_FAILURE;
}
