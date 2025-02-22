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
#ifndef FSM_H_
#define FSM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_FREERTOS_PORT
// #define FREERTOS_API
#endif

#ifdef FREERTOS_API
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#else
#include "ring_buff.h"
#endif 

//----------------------------------------------------------------------
//	CONFIGS
//----------------------------------------------------------------------
#define CONFIG_RUN_ON_TIMER_HOOK 1              // Runs the fsm inside the timed hook when a timout is triggered

//----------------------------------------------------------------------
//	DEFINES
//----------------------------------------------------------------------

#ifndef FSM_MAX_EVENTS
#define FSM_MAX_EVENTS 64
#endif

#ifndef MAX_HIERARCHY_DEPTH 
#define MAX_HIERARCHY_DEPTH  8
#endif

#ifndef FSM_MAX_ACTORS 
#define FSM_MAX_ACTORS  10
#endif 

#ifndef FSM_MAX_TRANSITIONS
// Max number of transitions for an event
#define FSM_MAX_TRANSITIONS 8
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
#define FSM_EV_FIRST 2

/**
 * @brief FSM FIRST ACTOR
 * 
 */
#define FSM_ACTOR_FIRST 1

/**
 * @brief FSM TIMED EVENT
 * 
 */
#define FSM_TIMEOUT_EV 1

//----------------------------------------------------------------------
//	MACROS
//----------------------------------------------------------------------

// States table definition
#define FSM_STATES_INIT(name)    static fsm_state_t name##_states[] = { [0] = {0},
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

// Transition table definition
#define FSM_TRANSITIONS_INIT(name) static const fsm_transition_t name##_transitions[] = { [0] = {0},
#define FSM_TRANSITIONS_END()   };

/**
 * @brief Internal helper macro to create a transition (used by other macros)
 */
#define FSM_TRANSITION_GENERAL_CREATE(_name, _source_id, _event, _target_id, _work) \
{                                                                                   \
    .source_state = (fsm_state_t*)&_name##_states[_source_id],                      \
    .event = _event,                                                                \
    .target_state = (fsm_state_t*)&_name##_states[_target_id],                      \
    .transition_action = (_work),                                                   \
},

/**
 * @brief Create a transitions array for the FSM
 * 
 * @param _name Should be the same as used in FSM_STATES_INIT(name)
 * @param _source_id Source state ID
 * @param event Event of the transition
 * @param _target_id Target state ID
 * 
 */
#define FSM_TRANSITION_CREATE(_name, _source_id, _event, _target_id) \
    FSM_TRANSITION_GENERAL_CREATE(_name, _source_id, _event, _target_id, NULL)

/**
 * @brief Create a transitions array for the FSM with work to be done
 * 
 * @param _name Should be the same as used in FSM_STATES_INIT(name)
 * @param _source_id Source state ID
 * @param event Event of the transition
 * @param _target_id Target state ID
 * @param _work Pointer to the work function of the transition 
 * 
 */
#define FSM_TRANSITION_WORK_CREATE(_name, _source_id, _event, _target_id, _work) \
    FSM_TRANSITION_GENERAL_CREATE(_name, _source_id, _event, _target_id, _work)

// Gets the number of ticks from time value in ms
#define FSM_MS_2_TICKS(fsm, ms) (fsm.fsm_ms_ticks*ms)

// actor table definition
#define FSM_ACTOR_INIT(name) static struct fsm_actor_t name##_actor[] = { [0] = {0},
#define FSM_ACTOR_END()   };

/**
 * @brief Create an actor array for the FSM
 * 
 * @param _source_id Source state ID
 * @param _entry Entry action function pointer
 * @param _run Run action function pointer
 * @param _exit Exit action function pointer
 * 
 */
#define FSM_ACTOR_CREATE(_source_id, _entry, _run, _exit)    \
{                                                            \
    .state_id       = _source_id,                            \
    .entry_action   = _entry,                                \
    .exit_action    = _exit,                                 \
    .run_action     = _run,                                  \
},    

#define FSM_TRANSITIONS_GET(name) name##_transitions
#define FSM_TRANSITIONS_SIZE(name) ((sizeof(name##_transitions)/sizeof(name##_transitions[0])-1))

#define FSM_STATE_GET(name, id)   name##_states[id]

#define FSM_ACTOR_GET(name)   name##_actor
#define FSM_ACTOR_SIZE(name) ((sizeof(name##_actor)/sizeof(name##_actor[0])))

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
typedef void (*fsm_action_t)(fsm_t* self, void* data);

struct fsm_state_t {
    
    int state_id;
    
    uint32_t t_period;
    uint32_t t_count;
    
    fsm_state_t* parent;
    fsm_state_t* default_substate;
    
    fsm_action_t entry_action;
    fsm_action_t exit_action;
    fsm_action_t run_action;
};

typedef struct {
    fsm_state_t* source_state;
    uint32_t event;
    fsm_state_t* target_state;
    fsm_action_t transition_action;
} fsm_transition_t;

typedef struct {
    fsm_state_t* source_state[FSM_MAX_TRANSITIONS+1];
    fsm_action_t transition_action[FSM_MAX_TRANSITIONS+1];
    fsm_state_t* target_state[FSM_MAX_TRANSITIONS+1];
} fsm_smt_events_t;

struct fsm_events_t
{
    uint32_t event;
    void *data;
};

struct fsm_actor_t {
    // State relevant to actor
    int state_id;
    // Work to be done
    fsm_action_t entry_action;
    fsm_action_t exit_action;
    fsm_action_t run_action;
} ;

typedef struct {
    // Actor
    struct fsm_actor_t* actor;
    // Actor's number of states
    int len;
} fsm_actors_net_t;


struct fsm_t {
    // States transutions table
    const fsm_transition_t *transitions;
    // Total number of transitions
    size_t num_transitions;
    // Total number of events
    size_t num_events;
    // Events ring buffer
#ifdef FREERTOS_API
    QueueHandle_t event_queue;
#else
    struct ringbuff event_queue;
#endif 
    struct fsm_events_t events_buff[FSM_MAX_EVENTS];
    // Events table
    fsm_smt_events_t smart_event[FSM_MAX_EVENTS+FSM_EV_FIRST];
    // Current state running
    fsm_state_t* current_state;
    // Actors
    fsm_actors_net_t actors_table[FSM_MAX_ACTORS];
    // Current data
    void* current_data;
    // Terminate value
    int terminate_val;
    // Timer hook period (ticks / ms)
    uint32_t fsm_ms_ticks;
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
 * @param num_events        Number of events in the fsm
 * @param time_period_ticks Timer hook period (ticks / ms), can be 0
 * @param initial_state     Default first state
 * @param initial_data      User custom data struct pointer
 */
int fsm_init(fsm_t *fsm, 
            const fsm_transition_t *transitions, 
            size_t num_transitions, 
            size_t num_events, 
            uint32_t time_period_ticks,
            fsm_state_t* initial_state, 
            void *initial_data);

/**
 * @brief Links an actor to a fsm
 * 
 * @param fsm 
 * @param actor 
 * @param size 
 * @return int 
 */
int fsm_actor_link(fsm_t *fsm, struct fsm_actor_t *actor, int size);

/**
 * @brief Sets the period in tick of a state's transition
 * 
 * @param state State where the timed transition is
 * @param ticks Ticks to wait for the trigger
 * @return int 
 */
int fsm_timed_event_set(fsm_state_t *state, uint32_t ticks);

/**
 * @brief Dispatches an event to the state machine. It will be process when fsm_run is called.
 * 
 * @param fsm 
 * @param event 
 * @param data 
 */
void fsm_dispatch(fsm_t *fsm, uint32_t event, void *data);

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

/**
 * @brief Updates timed events.
 * 
 * @param fsm 
 * @param data 
 */
void fsm_ticks_hook(fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* FSM_H_ */
