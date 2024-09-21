/**
 * @file fsm.h
 * @author Mauro Medina 
 * @brief 
 * @version 1.0.1
 * @date 2024-07-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef FSM_H
#define FSM_H

#include <stddef.h>
#include <stdint.h>

#include "ring_buff.h"

//----------------------------------------------------------------------
//	DEFINES
//----------------------------------------------------------------------

#ifndef FSM_MAX_EVENTS
#define FSM_MAX_EVENTS 64
#endif

#ifndef MAX_HIERARCHY_DEPTH 
#define MAX_HIERARCHY_DEPTH  8
#endif

//----------------------------------------------------------------------
//	DEFINITIONS
//----------------------------------------------------------------------
/**
 * @brief FSM NULL STATE
 * 
 */
#define FSM_ST_NONE 0

/**
 * @brief FSM FIRST STATE
 * 
 */
#define FSM_ST_FIRST 1

/**
 * @brief FSM FIRST EVENT
 * 
 */
#define FSM_EV_FIRST 0

//----------------------------------------------------------------------
//	MACROS
//----------------------------------------------------------------------
#define fsm_action(fsm, state, work) (fsm->transitions[state].action[work])

#define FSM_STATES_INIT(name)    static const fsm_state_t name##_states[] = { [0] = {0},
#define FSM_STATES_END()        };

/**
 * @brief Create a states array for the FSM
 * 
 * @param _name Should be the same as used in FSM_STATES_INIT(name)
 * @param _id Should be in order starting from 1
 * @param _parent ID of the parent state, or 0 if no parent
 * @param _sub ID of the default substate, or 0 if no default substate
 * @param _entry Entry action function pointer
 * @param _run Run action function pointer
 * @param _exit Exit action function pointer
 * 
 */
#define FSM_CREATE_STATE(_name, _id, _parent, _sub, _entry, _run, _exit)    \
[_id] = {                                                                   \
    .state_id = _id,                                                        \
    .parent = (_parent == 0) ? (fsm_state_t*)_parent : (fsm_state_t*)&_name##_states[_parent],       \
    .default_substate = (_sub == 0) ? (fsm_state_t*)_sub : (fsm_state_t*)&_name##_states[_sub],      \
    .entry_action = _entry,                                                 \
    .exit_action = _exit,                                                   \
    .run_action = _run                                                      \
},

#define FSM_TRANSITIONS_INIT(name) static const fsm_transition_t name##_transitions[] = { [0] = {0},
#define FSM_TRANSITIONS_END()   };

/**
 * @brief Create a states array for the FSM
 * 
 * @param _name Should be the same as used in FSM_STATES_INIT(name)
 * @param _source_id Source state ID
 * @param event Event of the transition
 * @param _target_id Target state ID
 * 
 */
#define FSM_TRANSITION_CREATE(_name, _source_id, _event, _target_id)    \
{                                                                       \
    .source_state = (fsm_state_t*)&_name##_states[_source_id],          \
    .event = _event,                                                    \
    .target_state = (fsm_state_t*)&_name##_states[_target_id],          \
},                          

#define FSM_TRANSITIONS_GET(name) name##_transitions
#define FSM_TRANSITIONS_SIZE(name) (sizeof(name##_transitions)/sizeof(name##_transitions[0]))

#define FSM_STATE_GET(name, id)   name##_states[id]
//----------------------------------------------------------------------
//	DECLARATIONS
//----------------------------------------------------------------------
enum fsm_action_e
{
    ACTION_ENTRY = 0,
    ACTION_RUN,
    ACTION_EXIT
};

typedef struct fsm_state_t fsm_state_t;
typedef struct fsm_t fsm_t;

struct fsm_state_t {
    int state_id;
    fsm_state_t* parent;
    fsm_state_t* default_substate;
    void (*entry_action)(fsm_t* self, void* data);
    void (*exit_action)(fsm_t* self, void* data);
    void (*run_action)(fsm_t* self, void* data);
};

typedef struct {
    fsm_state_t* source_state;
    int event;
    fsm_state_t* target_state;
} fsm_transition_t;

struct fsm_events_t
{
    int event;
    void *data;
};

struct fsm_t {
    // States transutions table
    const fsm_transition_t *transitions;
    // Total number of transitions
    size_t num_transitions;
    // Events ring buffer 
    struct ringbuff event_queue;
    struct fsm_events_t events_buff[FSM_MAX_EVENTS];
    // Current state running
    fsm_state_t* current_state;
    // Current data
    void* current_data;
    // Terminate value
    int terminate_val;
    // Internal info
    uint32_t internal;
};

//----------------------------------------------------------------------
//	FUNCTIONS
//----------------------------------------------------------------------

/**
 * @brief Inits the state machine object.
 * 
 * @param fsm               fsm pointer
 * @param transitions       Transitions table pointer
 * @param num_transitions   Number of transitions in the table
 * @param initial_state     Default first state
 * @param initial_data      User custom data struct pointer
 */
void fsm_init(fsm_t *fsm, const fsm_transition_t *transitions, size_t num_transitions, const fsm_state_t* initial_state, void *initial_data);

/**
 * @brief Dispatches an event to the state machine. It will be process when fsm_run is called.
 * 
 * @param fsm 
 * @param event 
 * @param data 
 */
void fsm_dispatch(fsm_t *fsm, int event, void *data);

/**
 * @brief Runs the state machine.
 * 
 * @details Process ALL pending events and then runs the current state once per call.
 * 
 * @param fsm 
 * @return int 
 */
int fsm_run(fsm_t *fsm);

/**
 * @brief Gets the current active state ID.
 * 
 * @param fsm 
 * @return int 
 */
int fsm_state_get(fsm_t *fsm);

/**
 * @brief Terminates the state machine.
 * 
 * @param fsm 
 * @param val 
 */
void fsm_terminate(fsm_t *fsm, int val);

/**
 * @brief Gets the number of pending events in the fsm
 * 
 * @param fsm 
 * @return int 
 */
int fsm_has_pending_events(fsm_t *fsm);

/**
 * @brief Fluches all pending events. 
 * 
 * @param fsm 
 */
void fsm_flush_events(fsm_t *fsm);

#endif /* FSM_H */
