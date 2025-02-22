#include "fsm.h"

/**
 * @brief MEF states
 * 
 */
enum {
    ROOT_ST = FSM_ST_FIRST,
    OFF_ST,
    ON_ST,
};

/**
 * @brief MEF events
 * 
 */
enum {
    ON_EV = FSM_EV_FIRST,
    OFF_EV,
    TOGGLE_EV,
    LAST_EV,
};

// Action function prototypes
static void enter_on(fsm_t *self, void* data);
static void enter_off(fsm_t *self, void* data);

static void timeout_work(fsm_t *self, void* data);

// Define FSM states
FSM_STATES_INIT(blinker)
//                  name  state id  parent          sub            entry       run   exit
FSM_CREATE_STATE(blinker, ROOT_ST,  FSM_ST_NONE,  OFF_ST,         NULL,       NULL, NULL)
FSM_CREATE_STATE(blinker, OFF_ST,   ROOT_ST,      FSM_ST_NONE,    enter_off,  NULL, NULL)
FSM_CREATE_STATE(blinker, ON_ST,    ROOT_ST,      FSM_ST_NONE,    enter_on,   NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(blinker)
//                    fsm name    State source   event           state target
FSM_TRANSITION_CREATE(blinker,      OFF_ST,      ON_EV,          ON_ST)
FSM_TRANSITION_CREATE(blinker,      ON_ST,       OFF_EV,         OFF_ST)
FSM_TRANSITION_CREATE(blinker,      OFF_ST,      FSM_TIMEOUT_EV, ON_ST)
//                          fsm name   State source        event       state target     transition work
FSM_TRANSITION_WORK_CREATE(blinker,      ON_ST,       FSM_TIMEOUT_EV,    OFF_ST,       timeout_work)
FSM_TRANSITIONS_END()

/**
 * @brief User defined timer that is called every 1ms
 * 
 * @param data 
 */
static void periodic_timer_1ms(void *data)
{
    fsm_t *fsm = (fsm_t*) data;

    fsm_ticks_hook(fsm);
}   

int main(void)
{
    fsm_t blinker;

    // Inits the fsm, defines the timed period as 1 tick per ms
    fsm_init(&blinker, FSM_TRANSITIONS_GET(blinker), FSM_TRANSITIONS_SIZE(blinker), LAST_EV, 1,
            &FSM_STATE_GET(blinker, ROOT_ST), NULL);

    fsm_timed_event_set(&FSM_STATE_GET(blinker, ON_ST), BLINK_PERIOD);
    fsm_timed_event_set(&FSM_STATE_GET(blinker, OFF_ST), BLINK_PERIOD);

    while(1);

    return 0;
}