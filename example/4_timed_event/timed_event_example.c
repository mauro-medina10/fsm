/**
 * @file timed_event_example.c
 * @author Mauro Medina
 * @brief Timed event blinker example with transition work callback
 * @version 2.0.0
 * @date 2025
 *
 * @copyright Copyright (c) 2025
 */
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

#define BLINK_PERIOD_MS 250U

enum {
    ROOT_ST = FSM_ST_FIRST,
    OFF_ST,
    ON_ST,
};

enum {
    ON_EV = FSM_EV_FIRST,
    OFF_EV,
    TOGGLE_EV,
    LAST_EV,
};

// Action function prototypes
static void enter_on(fsm_t *self, void *data);
static void enter_off(fsm_t *self, void *data);
static void timeout_work(fsm_t *self, void *data);

// Define FSM states
FSM_STATES_INIT(blinker)
//               name     state id  parent       sub          entry      run   exit
FSM_CREATE_STATE(blinker, ROOT_ST,  FSM_ST_NONE, OFF_ST,      NULL,      NULL, NULL)
FSM_CREATE_STATE(blinker, OFF_ST,   ROOT_ST,     FSM_ST_NONE, enter_off, NULL, NULL)
FSM_CREATE_STATE(blinker, ON_ST,    ROOT_ST,     FSM_ST_NONE, enter_on,  NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(blinker)
//                         fsm name  State source  event           state target  transition work
FSM_TRANSITION_CREATE(     blinker,  OFF_ST,       ON_EV,          ON_ST)
FSM_TRANSITION_CREATE(     blinker,  ON_ST,        OFF_EV,         OFF_ST)
FSM_TRANSITION_CREATE(     blinker,  OFF_ST,       FSM_TIMEOUT_EV, ON_ST)
FSM_TRANSITION_WORK_CREATE(blinker,  ON_ST,        FSM_TIMEOUT_EV, OFF_ST,       timeout_work)
FSM_TRANSITIONS_END()

static void enter_on(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("Blinker ON\n");
}

static void enter_off(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("Blinker OFF\n");
}

static void timeout_work(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("ON state timeout -> turning OFF\n");
}

static void periodic_timer_1ms(void *data)
{
    fsm_t *fsm = (fsm_t *)data;
    fsm_ticks_hook(fsm);
}

int main(void)
{
    fsm_t blinker = {0};
    int ret = 0;

    printf("--- Starting Timed Event Blinker ---\n");

    ret = fsm_init(&blinker,
                   FSM_TRANSITIONS_GET(blinker),
                   FSM_TRANSITIONS_SIZE(blinker),
                   LAST_EV,
                   1,
                   &FSM_STATE_GET(blinker, ROOT_ST),
                   NULL);
    if (ret != 0) {
        return EXIT_FAILURE;
    }

    // Setting OFF_ST timer after fsm_init() immediately arms the active state
    fsm_timed_event_set(&blinker, &FSM_STATE_GET(blinker, ON_ST),  FSM_MS_2_TICKS(&blinker, BLINK_PERIOD_MS));
    fsm_timed_event_set(&blinker, &FSM_STATE_GET(blinker, OFF_ST), FSM_MS_2_TICKS(&blinker, BLINK_PERIOD_MS));

    for (unsigned int ms = 0; ms < 1000U; ms++) {
        periodic_timer_1ms(&blinker);
        if (fsm_has_pending_events(&blinker) > 0) {
            ret |= fsm_run(&blinker);
        }
    }

    fsm_deinit(&blinker);
    return ret;
}
