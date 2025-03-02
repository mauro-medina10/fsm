/**
 * @file fsm_music.c
 * @author Mauro Medina 
 * @brief 
 * @version 1.0.1
 * @date 2025
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

// Define states
enum {
    ST_ROOT = FSM_ST_FIRST,   
    ST_OFF,                  
    ST_ON,                   
    ST_ON_IDLE,                   
    ST_BLINK,                   
    ST_BLINK_ON,                   
    ST_BLINK_OFF,                   
};

// Define events
enum {
    EV_ON = FSM_EV_FIRST,    
    EV_OFF,                  
    EV_TOGGLE,                  
    EV_BLINK,                  
    EV_LAST,                            
};

// Action function prototypes
static void enter_on(fsm_t *self, void* data);
static void enter_off(fsm_t *self, void* data);
static void enter_blink_on(fsm_t *self, void* data);
static void enter_blink_off(fsm_t *self, void* data);

// Define FSM states
FSM_STATES_INIT(led)
//             name   state id        parent        sub         entry      run     exit
FSM_CREATE_STATE(led, ST_ROOT,      FSM_ST_NONE, ST_OFF,        NULL,      NULL,   NULL)
FSM_CREATE_STATE(led, ST_OFF,       ST_ROOT,     FSM_ST_NONE,  enter_off,  NULL,   NULL)
FSM_CREATE_STATE(led, ST_ON,        ST_ROOT,     ST_ON_IDLE,   enter_on,   NULL,   NULL)
FSM_CREATE_STATE(led, ST_ON_IDLE,   ST_ON,       FSM_ST_NONE,   NULL,      NULL,   NULL)
FSM_CREATE_STATE(led, ST_BLINK,     ST_ON,       ST_BLINK_OFF,  NULL,      NULL,   NULL)
FSM_CREATE_STATE(led, ST_BLINK_OFF, ST_BLINK,    FSM_ST_NONE,  enter_off,  NULL,   NULL)
FSM_CREATE_STATE(led, ST_BLINK_ON,  ST_BLINK,    FSM_ST_NONE,  enter_on,   NULL,   NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(led)
//                fsm name  State source    event         state target
FSM_TRANSITION_CREATE(led,   ST_OFF,        EV_ON,          ST_ON)
FSM_TRANSITION_CREATE(led,   ST_ON,         EV_OFF,         ST_OFF)
FSM_TRANSITION_CREATE(led,   ST_OFF,        EV_TOGGLE,      ST_ON)
FSM_TRANSITION_CREATE(led,   ST_ON,         EV_TOGGLE,      ST_OFF)
FSM_TRANSITION_CREATE(led,   ST_ON_IDLE,    EV_BLINK,       ST_BLINK)
FSM_TRANSITION_CREATE(led,   ST_BLINK_ON,   FSM_TIMEOUT_EV, ST_BLINK_OFF)
FSM_TRANSITION_CREATE(led,   ST_BLINK_OFF,  FSM_TIMEOUT_EV, ST_BLINK_ON)
FSM_TRANSITIONS_END()


// Action function implementations

static void enter_off(fsm_t *self, void* data) 
{ 
    print("Led OFF\n");
}

static void enter_on(fsm_t *self, void *data) 
{ 
    print("Led ON\n"); 
}

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

int main() {
    fsm_t led = {0};
    led_device_t dev;
    int ret = 0;

    // Simulate music player actions
    print("--- Starting LED fsm ---\n");

    // FSM init
    fsm_init(&led,                              // fsm:               fsm pointer
                FSM_TRANSITIONS_GET(led),       // transitions:       Transitions table pointer
                FSM_TRANSITIONS_SIZE(led),      // num_transitions:   Number of transitions in the table
                EV_LAST,                        // num_events:        Number of events in the fsm
                1,                              // time_period_ticks: Timer hook period (ticks / ms), can be 0
                &FSM_STATE_GET(led, ST_ROOT),   // initial_state:     Default first state
                &dev                            // initial_data:      User custom data struct pointer
            );
    
    fsm_timed_event_set(&FSM_STATE_GET(LED, ST_BLINK_ON), BLINK_PERIOD);
    fsm_timed_event_set(&FSM_STATE_GET(LED, ST_BLINK_OFF), BLINK_PERIOD);

    // Powers on the led
    fsm_dispatch(&led, EV_ON, NULL);
    
    // Running the FSM
    ret |= fsm_run(&led);
    
    // Starts blinking
    fsm_dispatch(&led, EV_BLINK, NULL);
    
    // Running the FSM
    ret |= fsm_run(&led);

    print("End %d\n", ret);

    while (1); // Led should blink as long as periodic_timer_1ms is called

    return ret;
}