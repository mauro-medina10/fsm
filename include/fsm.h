/**
 * @file fsm.h
 * @author Mauro Medina
 * @brief Hierarchical Finite State Machine (FSM) library
 * @version 2.0.0
 * @date 2024-07-17
 *
 * @copyright Copyright (c) 2024
 */
#ifndef FSM_H_
#define FSM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif

#if defined(CONFIG_FSM_USE_FREERTOS_API) || defined(CONFIG_FREERTOS_API)
#define FSM_USE_FREERTOS_API 1
#else
#define FSM_USE_FREERTOS_API 0
#endif

#if FSM_USE_FREERTOS_API
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#else
#include "ring_buff.h"
#endif

//----------------------------------------------------------------------
//	CONFIGS
//----------------------------------------------------------------------

/* Set to 1 only if fsm_ticks_hook() and fsm_run() are strictly serialized. */
#ifndef CONFIG_RUN_ON_TIMER_HOOK
#ifdef CONFIG_FSM_RUN_ON_TIMER_HOOK
#define CONFIG_RUN_ON_TIMER_HOOK 1
#else
#define CONFIG_RUN_ON_TIMER_HOOK 0
#endif
#endif

//----------------------------------------------------------------------
//	DEFINES
//----------------------------------------------------------------------

#ifndef FSM_MAX_EVENTS
#ifdef CONFIG_FSM_MAX_EVENTS
#define FSM_MAX_EVENTS CONFIG_FSM_MAX_EVENTS
#else
#define FSM_MAX_EVENTS 64
#endif
#endif

#ifndef MAX_HIERARCHY_DEPTH
#ifdef CONFIG_FSM_MAX_HIERARCHY_DEPTH
#define MAX_HIERARCHY_DEPTH CONFIG_FSM_MAX_HIERARCHY_DEPTH
#else
#define MAX_HIERARCHY_DEPTH 8
#endif
#endif

#ifndef FSM_MAX_ACTORS
#ifdef CONFIG_FSM_MAX_ACTORS
#define FSM_MAX_ACTORS CONFIG_FSM_MAX_ACTORS
#else
#define FSM_MAX_ACTORS 10
#endif
#endif

#ifndef FSM_MAX_TIMED_STATES
#ifdef CONFIG_FSM_MAX_TIMED_STATES
#define FSM_MAX_TIMED_STATES CONFIG_FSM_MAX_TIMED_STATES
#else
#define FSM_MAX_TIMED_STATES 8
#endif
#endif

//----------------------------------------------------------------------
//	DEFINITIONS
//----------------------------------------------------------------------

/**
 * @brief Built-in FSM states
 */
enum fsm_states_e
{
    FSM_ST_NONE = 0,
    FSM_ST_END,
    FSM_ST_FIRST,
};

/**
 * @brief Built-in FSM events
 */
enum fsm_events_e
{
    FSM_EV_NONE = 0,
    FSM_END_EV,
    FSM_TIMEOUT_EV,
    FSM_EV_FIRST,
};

/**
 * @brief FSM first actor index
 */
#define FSM_ACTOR_FIRST 0

/**
 * @brief FSM no timed events
 */
#define FSM_NO_TICKS 0

//----------------------------------------------------------------------
//	MACROS
//----------------------------------------------------------------------

// Immutable states table definition (stored in Flash/RODATA, multi-instance safe)
#define FSM_STATES_INIT(name) static const fsm_state_t name##_states[] = { [FSM_ST_NONE] = {0}, [FSM_ST_END] = { .id = FSM_ST_END },
#define FSM_STATES_END()      };

/**
 * @brief Create a state entry in the FSM states array
 *
 * @param _name   Same identifier passed to FSM_STATES_INIT(name)
 * @param _id     Unique state ID starting from FSM_ST_FIRST
 * @param _parent ID of parent state, or FSM_ST_NONE if top-level
 * @param _sub    ID of default substate, or FSM_ST_NONE if leaf state
 * @param _entry  Entry action function pointer (or NULL)
 * @param _run    Run action function pointer (or NULL)
 * @param _exit   Exit action function pointer (or NULL)
 */
#define FSM_CREATE_STATE(_name, _id, _parent, _sub, _entry, _run, _exit)                                \
[_id] = {                                                                                               \
    .id               = (_id),                                                                          \
    .parent           = ((_parent) == FSM_ST_NONE) ? NULL : &_name##_states[_parent],                   \
    .default_substate = ((_sub) == FSM_ST_NONE) ? NULL : &_name##_states[_sub],                         \
    .entry_action     = (_entry),                                                                       \
    .exit_action      = (_exit),                                                                        \
    .run_action       = (_run)                                                                          \
},

// Transition table definition (0-indexed, stored in Flash/RODATA)
#define FSM_TRANSITIONS_INIT(name) static const fsm_transition_t name##_transitions[] = {
#define FSM_TRANSITIONS_END()      };

/**
 * @brief Internal helper macro to create a transition
 */
#define FSM_TRANSITION_GENERAL_CREATE(_name, _source_id, _event, _target_id, _guard, _work) \
{                                                                                           \
    .source_state      = &_name##_states[_source_id],                                       \
    .event             = (_event),                                                          \
    .target_state      = &_name##_states[_target_id],                                       \
    .guard             = (_guard),                                                          \
    .transition_action = (_work),                                                           \
},

/**
 * @brief Create a transition entry
 */
#define FSM_TRANSITION_CREATE(_name, _source_id, _event, _target_id) \
    FSM_TRANSITION_GENERAL_CREATE(_name, _source_id, _event, _target_id, NULL, NULL)

/**
 * @brief Create a transition entry with a transition action callback
 */
#define FSM_TRANSITION_WORK_CREATE(_name, _source_id, _event, _target_id, _work) \
    FSM_TRANSITION_GENERAL_CREATE(_name, _source_id, _event, _target_id, NULL, _work)

/**
 * @brief Create a guarded transition entry with optional transition action callback
 */
#define FSM_TRANSITION_GUARD_CREATE(_name, _source_id, _event, _target_id, _guard, _work) \
    FSM_TRANSITION_GENERAL_CREATE(_name, _source_id, _event, _target_id, _guard, _work)

// Convert milliseconds to ticks using the FSM's configured tick rate (ticks/ms)
#define FSM_MS_2_TICKS(fsm, ms) (((fsm)->fsm_ms_ticks) * (uint32_t)(ms))

// Actor table definition (0-indexed, stored in Flash/RODATA)
#define FSM_ACTOR_INIT(name) static const struct fsm_actor_t name##_actor[] = {
#define FSM_ACTOR_END()      };

/**
 * @brief Create an actor entry for a state
 */
#define FSM_ACTOR_CREATE(_source_id, _entry, _run, _exit) \
{                                                         \
    .id           = (_source_id),                         \
    .entry_action = (_entry),                             \
    .exit_action  = (_exit),                              \
    .run_action   = (_run),                               \
},

#define FSM_TRANSITIONS_GET(name)  name##_transitions
#define FSM_TRANSITIONS_SIZE(name) (sizeof(name##_transitions) / sizeof(name##_transitions[0]))

#define FSM_STATE_GET(name, id) name##_states[id]

#define FSM_ACTOR_GET(name)  name##_actor
#define FSM_ACTOR_SIZE(name) (sizeof(name##_actor) / sizeof(name##_actor[0]))

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
typedef void (*fsm_action_t)(fsm_t *self, void *data);
typedef bool (*fsm_guard_t)(fsm_t *self, void *data);

struct fsm_state_t {
    int id;

    const fsm_state_t *parent;
    const fsm_state_t *default_substate;

    fsm_action_t entry_action;
    fsm_action_t exit_action;
    fsm_action_t run_action;
};

typedef struct {
    const fsm_state_t *source_state;
    uint32_t event;
    const fsm_state_t *target_state;
    fsm_guard_t guard;
    fsm_action_t transition_action;
} fsm_transition_t;

struct fsm_events_t
{
    uint32_t event;
    void *data;
};

struct fsm_actor_t {
    int id;
    fsm_action_t entry_action;
    fsm_action_t exit_action;
    fsm_action_t run_action;
};

typedef struct {
    const struct fsm_actor_t *actor;
    size_t len;
} fsm_actors_net_t;

struct fsm_state_timer_t {
    int state_id;
    uint32_t t_period;
};

struct fsm_t {
    // State transitions table
    const fsm_transition_t *transitions;
    // Total number of transitions
    size_t num_transitions;
    // Event queue
#if FSM_USE_FREERTOS_API
    QueueHandle_t event_queue;
    StaticQueue_t event_queue_storage;
#else
    struct ringbuff event_queue;
#endif
    struct fsm_events_t events_buff[FSM_MAX_EVENTS];
    // Current active state
    const fsm_state_t *current_state;
    // Linked actors
    fsm_actors_net_t actors_table[FSM_MAX_ACTORS];
    // Per-instance timed state periods
    struct fsm_state_timer_t state_timers[FSM_MAX_TIMED_STATES];
    // User instance context data
    void *current_data;
    // Termination code
    int terminate_val;
    // Timer hook rate (ticks / ms)
    uint32_t fsm_ms_ticks;
    // Active state countdown timer (ticks remaining)
    uint32_t timer_count;
    // Internal status flags
    uint32_t internal;
};

//----------------------------------------------------------------------
//	FUNCTIONS
//----------------------------------------------------------------------

/**
 * @brief Initialize the state machine object.
 *
 * @param fsm               FSM pointer
 * @param transitions       Transitions table pointer
 * @param num_transitions   Number of transitions in the table
 * @param num_events        (Deprecated/informational) Number of events in the FSM
 * @param time_period_ticks Timer hook rate (ticks / ms), can be 0
 * @param initial_state     Default initial state
 * @param initial_data      User custom context data pointer
 * @return 0 on success, negative error code on failure
 */
int fsm_init(fsm_t *fsm,
             const fsm_transition_t *transitions,
             size_t num_transitions,
             size_t num_events,
             uint32_t time_period_ticks,
             const fsm_state_t *initial_state,
             void *initial_data);

/**
 * @brief Deinitialize the state machine and release queue resources.
 *
 * @param fsm FSM pointer
 */
void fsm_deinit(fsm_t *fsm);

/**
 * @brief Link an actor table to an FSM.
 *
 * @param fsm   FSM pointer
 * @param actor Actor table pointer
 * @param size  Number of entries in actor table (must be > 0)
 * @return 0 on success, negative error code on failure
 */
int fsm_actor_link(fsm_t *fsm, const struct fsm_actor_t *actor, size_t size);

/**
 * @brief Configure a per-instance timeout period in ticks for a state.
 *
 * If the configured state is currently active (or an ancestor of the active state),
 * the active countdown is immediately armed/updated. Pass ticks = 0 to disable.
 *
 * @param fsm   FSM pointer
 * @param state State where the timed transition is configured
 * @param ticks Ticks to wait before triggering FSM_TIMEOUT_EV (0 to disable)
 * @return 0 on success, negative error code on failure
 */
int fsm_timed_event_set(fsm_t *fsm, const fsm_state_t *state, uint32_t ticks);

/**
 * @brief Get the user context data pointer associated with the FSM instance.
 *
 * @param fsm FSM pointer
 * @return Context data pointer, or NULL if fsm is NULL
 */
void *fsm_data_get(const fsm_t *fsm);

/**
 * @brief Update the user context data pointer associated with the FSM instance.
 *
 * @param fsm  FSM pointer
 * @param data New context data pointer
 */
void fsm_data_set(fsm_t *fsm, void *data);

/**
 * @brief Enqueue an event for the state machine to process on the next fsm_run().
 *
 * @param fsm   FSM pointer
 * @param event Event ID
 * @param data  Optional per-event payload
 */
void fsm_dispatch(fsm_t *fsm, uint32_t event, void *data);

/**
 * @brief Process all pending events and execute the current state's run action once.
 *
 * @param fsm FSM pointer
 * @return 0 on normal execution, terminate_val if terminated, negative on error
 */
int fsm_run(fsm_t *fsm);

/**
 * @brief Get the current active state ID.
 *
 * @param fsm FSM pointer
 * @return Active state ID, or FSM_ST_NONE if uninitialized/NULL
 */
int fsm_state_get(const fsm_t *fsm);

/**
 * @brief Terminate the state machine with a status value.
 *
 * @param fsm FSM pointer
 * @param val Termination value returned by fsm_run()
 */
void fsm_terminate(fsm_t *fsm, int val);

/**
 * @brief Check if the FSM has pending events queued.
 *
 * @param fsm FSM pointer
 * @return 1 if events are pending, 0 if empty, -1 on invalid pointer
 */
int fsm_has_pending_events(const fsm_t *fsm);

/**
 * @brief Discard all pending events in the queue.
 *
 * @note In FreeRTOS mode, do not call from an ISR context.
 *
 * @param fsm FSM pointer
 */
void fsm_flush_events(fsm_t *fsm);

/**
 * @brief Advance the FSM tick counter and enqueue FSM_TIMEOUT_EV on expiration.
 *
 * @param fsm FSM pointer
 */
void fsm_ticks_hook(fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* FSM_H_ */
