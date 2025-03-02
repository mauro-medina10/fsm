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
    ST_IDLE,                   
    ST_PRESSED,                  
    ST_PRESSED_CONFIRM,                  
    ST_ANTIBOUNCE,                   
};

// Define events
enum {
    EV_PRESS = FSM_EV_FIRST,    
    EV_UNPRESS,                  
    EV_LAST,                            
};

static void btn_pressed(fsm_t* self, void* data);
static void btn_unpressed(fsm_t* self, void* data);

// Define FSM states
FSM_STATES_INIT(button)
//             name   state id               parent        sub            entry     run     exit
FSM_CREATE_STATE(button, ST_ROOT,            FSM_ST_NONE,   ST_IDLE,   NULL,     NULL,   NULL)
FSM_CREATE_STATE(button, ST_IDLE,              ST_ROOT,     FSM_ST_NONE,    NULL,     NULL,   NULL)
FSM_CREATE_STATE(button, ST_PRESSED,           ST_ROOT,     ST_ANTIBOUNCE,  NULL,     NULL,   NULL)
FSM_CREATE_STATE(button, ST_ANTIBOUNCE,        ST_PRESSED,  FSM_ST_NONE,    NULL,     NULL,   NULL)
FSM_CREATE_STATE(button, ST_PRESSED_CONFIRM,   ST_PRESSED,  FSM_ST_NONE,    NULL,     NULL,   NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(button)
//                  fsm name     State source       event             state target
FSM_TRANSITION_CREATE(button,     ST_IDLE,         EV_PRESS,          ST_PRESSED)
FSM_TRANSITION_CREATE(button,     ST_PRESSED,      EV_UNPRESS,        ST_IDLE)
FSM_TRANSITION_CREATE(button,     ST_ANTIBOUNCE,   FSM_TIMEOUT_EV,    ST_PRESSED_CONFIRM)
FSM_TRANSITIONS_END()

// Button Actor 
FSM_ACTOR_INIT(btn_actor)
FSM_ACTOR_CREATE(ST_PRESSED_CONFIRM, btn_pressed, NULL, btn_unpressed)
FSM_ACTOR_END()

/**
 * @brief Button interrupt handler (example)
 * 
 * @param arg 
 */
static void gpio_isr_handler(void* arg)
{
    btn_device_t * btn = (btn_device_t *) arg;

    fsm_dispatch(&btn->fsm, (gpio_get_level(btn->gpio) == 0) ? EV_PRESS : EV_UNPRESS, btn);
}

/**
 * @brief User defined timer that is called every 1ms (example)
 * 
 * @param data 
 */
static void periodic_timer_1ms(void *data)
{
    fsm_t *fsm = (fsm_t*) data;

    fsm_ticks_hook(fsm);
}  

/**
 * @brief Button is pressed
 */
static void btn_pressed(fsm_t* self, void* data)
{
    printf("Button pressed\n");
}

/**
 *  @brief Button is unpressed
 */
static void btn_unpressed(fsm_t* self, void* data)
{
    printf("Button unpressed\n");
}

int main() {
    fsm_t button = {0};
    btn_device_t dev;
    int ret = 0;

    // Simulate music player actions
    printf("--- Starting BUTTON fsm ---\n");

    // FSM init
    fsm_init(&button,                              // fsm:               fsm pointer
                FSM_TRANSITIONS_GET(button),       // transitions:       Transitions table pointer
                FSM_TRANSITIONS_SIZE(button),      // num_transitions:   Number of transitions in the table
                EV_LAST,                        // num_events:        Number of events in the fsm
                1,                              // time_period_ticks: Timer hook period (ticks / ms), can be 0
                &FSM_STATE_GET(button, ST_ROOT),   // initial_state:     Default first state
                &dev                            // initial_data:      User custom data struct pointer
            );
    
    fsm_timed_event_set(&FSM_STATE_GET(button, ST_ANTIBOUNCE), ANTIBOUNCE_TIME_MS);

    // Actor link
    btn_actor_link(&button, FSM_ACTOR_GET(btn_actor), FSM_ACTOR_SIZE(btn_actor));

    // Running the FSM
    while (!ret)
    {
        ret = fsm_run(&button);
        delay_ms(10);
    }

    return ret;
}