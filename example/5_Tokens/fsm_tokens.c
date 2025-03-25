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
    ST_PRINT_JOBS,                  
    ST_PRINTERS,                   
    ST_PRINTING,    
    ST_FAIL,                
};

// Define events
enum {
    EV_PRINT = FSM_EV_FIRST,    
    EV_DONE,                  
    EV_PRINT_START,    
    EV_FAIL,        
    EV_PRINT_JOB,
    EV_LAST,                            
};

// Action function prototypes
static void enter_printing(fsm_t *self, void* data);
static void run_printing(fsm_t *self, void* data);
static void enter_print_job(fsm_t *self, void* data);

// Define FSM states
FSM_TOKEN_STATES_INIT(printers)
//             name   state id        parent        sub             entry           run             exit
FSM_CREATE_STATE(printers, ST_ROOT,        FSM_ST_NONE,  ST_PRINTERS,    NULL,           NULL,           NULL)
FSM_CREATE_STATE(printers, ST_PRINT_JOBS,  ST_ROOT,      FSM_ST_NONE,  enter_print_job,  NULL,           NULL)
FSM_CREATE_STATE(printers, ST_PRINTERS,    ST_ROOT,      FSM_ST_NONE,  NULL,             NULL,           NULL)
FSM_CREATE_STATE(printers, ST_PRINTING,    ST_ROOT,      FSM_ST_NONE,  enter_printing,  run_printing,   NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(printers)
//                          sm name      State source      event           state target
FSM_TRANSITION_JOIN_CREATE(printers,   ST_PRINT_JOBS,    EV_PRINT,       ST_PRINTING,  0)
FSM_TRANSITION_JOIN_CREATE(printers,   ST_PRINTERS,      EV_PRINT,       ST_PRINTING,  0)
FSM_TRANSITION_JOIN_CREATE(printers,   ST_PRINTING,      EV_DONE,        FSM_ST_END,   0)
FSM_TRANSITION_JOIN_CREATE(printers,   ST_PRINTING,      EV_DONE,        ST_PRINTERS,  0)
FSM_TRANSITION_CREATE(printers,   ST_PRINTING,      FSM_TIMEOUT_EV, ST_PRINTERS)
FSM_TRANSITION_CREATE(printers,   ST_PRINTING,      FSM_TIMEOUT_EV, ST_FAIL)
FSM_TRANSITION_CREATE(printers,   FSM_ST_USER,      EV_PRINT_JOB,   ST_PRINT_JOBS)
FSM_TRANSITIONS_END()


// Action function implementations
static void enter_printing(fsm_t *self, void* data)
{
    printf("Entering printing state\n");
}
static void run_printing(fsm_t *self, void* data)
{
    printf("Running printing state\n");
}
static void enter_print_job(fsm_t *self, void* data)
{
    printf("Entering print job state\n");
}

int main() {
    fsm_t printers = {0};
    printer_device_t dev_1;
    printer_device_t dev_2;
    printer_device_t dev_3;
    printer_device_t printers[] = {dev_1, dev_2, dev_3};
    print_job_t job_1;

    int ret = 0;

    // Simulate music player actions
    printf("--- Starting PRINTERS fsm ---\n");

    // FSM init
    fsm_init(&printers,                              // fsm:               fsm pointer
                FSM_TRANSITIONS_GET(printers),       // transitions:       Transitions table pointer
                FSM_TRANSITIONS_SIZE(printers),      // num_transitions:   Number of transitions in the table
                EV_LAST,                             // num_events:        Number of events in the fsm
                FSM_NO_TICKS,                        // time_period_ticks: Timer hook period (ticks / ms), can be 0
                &FSM_STATE_GET(printers, ST_ROOT),   // initial_state:     Default first state
                &printers                            // initial_data:      User custom data struct pointer
            );
    
    // Running the FSM
    ret |= fsm_run(&printers);
    
    // Starts printing
    fsm_token_give(&printers, EV_PRINT_JOB, 1, &job_1);
    fsm_dispatch(&printers, EV_PRINT_START, NULL);
    
    // Running the FSM
    while(fsm_state_get(&printers) == ST_PRINTING)
    {
        ret |= fsm_run(&printers);
    }

    printf("End %d\n", ret);

    while (1); 

    return ret;
}