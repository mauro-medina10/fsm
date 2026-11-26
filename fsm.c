/**
 * @file fsm.c
 * @author Mauro Medina
 * @brief Hierarchical Finite State Machine (FSM) implementation
 * @version 1.1.0
 * @date 2024-07-17
 *
 * @copyright Copyright (c) 2024
 */
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "fsm.h"

#define FSM_FLAG_TERMINATE (1U << 0)
#define FSM_FLAG_HANDLED   (1U << 1)

#define FSM_SMART_EVENT_SLOTS (FSM_MAX_EVENTS + FSM_EV_FIRST)

static fsm_state_t *find_lca(fsm_state_t *s1, fsm_state_t *s2);

static void actor_action(fsm_t *fsm, fsm_state_t *target, enum fsm_action_e action, void *data)
{
    for (size_t i = 0; (i < FSM_MAX_ACTORS) && (fsm->actors_table[i].actor != NULL); i++)
    {
        for (size_t j = FSM_ACTOR_FIRST; j < fsm->actors_table[i].len; j++)
        {
            if (fsm->actors_table[i].actor[j].id == target->id)
            {
                switch (action)
                {
                    case ACTION_ENTRY:
                        if (fsm->actors_table[i].actor[j].entry_action != NULL) {
                            fsm->actors_table[i].actor[j].entry_action(fsm, data);
                        }
                        break;
                    case ACTION_RUN:
                        if (fsm->actors_table[i].actor[j].run_action != NULL) {
                            fsm->actors_table[i].actor[j].run_action(fsm, data);
                        }
                        break;
                    case ACTION_EXIT:
                        if (fsm->actors_table[i].actor[j].exit_action != NULL) {
                            fsm->actors_table[i].actor[j].exit_action(fsm, data);
                        }
                        break;
                }
            }
        }
    }
}

static void enter_state(fsm_t *fsm, fsm_state_t *lca, fsm_state_t *target, void *data)
{
    fsm_state_t *state_path[MAX_HIERARCHY_DEPTH];
    fsm_state_t *state_target = target;
    int depth = 0;

    // Check for default substate
    while (state_target->default_substate != NULL) {
        state_target = state_target->default_substate;
    }

    fsm->current_state = state_target;
    fsm->timer.t_count = fsm->current_state->timer.t_period;

    // Build path from target to LCA (exclusive)
    for (fsm_state_t *s = state_target; s != lca && s != NULL; s = s->parent) {
        state_path[depth++] = s;
        if (depth >= MAX_HIERARCHY_DEPTH) break;
    }

    // Execute entry actions from LCA (exclusive) to target state
    for (int i = depth - 1; i >= 0; i--) {
        if (state_path[i]->entry_action != NULL) {
            state_path[i]->entry_action(fsm, data);
        }
        actor_action(fsm, state_path[i], ACTION_ENTRY, data);
    }

    // When source state is target state, execute entry action
    if ((lca == state_target) && (depth == 0)) {
        if (lca->entry_action != NULL) {
            lca->entry_action(fsm, data);
        }
        actor_action(fsm, state_target, ACTION_ENTRY, data);
    }
}

static void exit_state(fsm_t *fsm, fsm_state_t *state, void *data)
{
    for (fsm_state_t *s = fsm->current_state; s != state && s != NULL; s = s->parent) {
        if (s->exit_action != NULL) {
            s->exit_action(fsm, data);
        }
        actor_action(fsm, s, ACTION_EXIT, data);
    }
}

static void transition_work(fsm_t *fsm, fsm_action_t action, void *data)
{
    if (action != NULL) {
        action(fsm, data);
    }
}

static uint8_t find_ancestor_depth(fsm_state_t *state, fsm_state_t *ancestor)
{
    uint8_t depth = 0;
    fsm_state_t *s = state;

    if (s == ancestor) {
        return depth;
    }

    while (s->parent != NULL)
    {
        if (s == ancestor) {
            return depth;
        }
        s = s->parent;
        depth++;
    }
    return UINT8_MAX; // Not an ancestor
}

static void end_state(fsm_t *fsm, fsm_state_t *state, void *data)
{
    int ancestor_idx = -1;

    // Find an end event transition of an ancestor state
    for (int i = 0; (i < FSM_MAX_TRANSITIONS + 1) && (fsm->smart_event[FSM_END_EV].source_state[i] != NULL); i++)
    {
        uint8_t ancestor_depth = MAX_HIERARCHY_DEPTH + 1;
        uint8_t depth = 0;
        fsm_state_t *s = fsm->smart_event[FSM_END_EV].source_state[i];

        // Check if s is an ancestor of state
        if (find_lca(state, s) != s) continue;

        depth = find_ancestor_depth(state, s);
        if (depth < ancestor_depth) {
            ancestor_depth = depth;
            ancestor_idx = i;
        }
    }

    // If no valid ancestor was found, terminate
    if (ancestor_idx == -1) {
        fsm_terminate(fsm, FSM_END_EV);
        return;
    }

    // Exit to the ancestor state and then enter the target state
    exit_state(fsm, fsm->smart_event[FSM_END_EV].source_state[ancestor_idx], data);
    transition_work(fsm, fsm->smart_event[FSM_END_EV].transition_action[ancestor_idx], data);
    enter_state(fsm, fsm->smart_event[FSM_END_EV].source_state[ancestor_idx],
                fsm->smart_event[FSM_END_EV].target_state[ancestor_idx], data);
}

static fsm_state_t *find_lca(fsm_state_t *s1, fsm_state_t *s2)
{
    fsm_state_t *a = s1, *b = s2;
    while (a != b) {
        if (a == NULL) a = s2;
        else if (b == NULL) b = s1;
        else {
            a = a->parent;
            b = b->parent;
        }
    }
    return a;
}

static int fsm_smart_events_init(fsm_t *fsm)
{
    size_t counts[FSM_SMART_EVENT_SLOTS];

    memset(fsm->smart_event, 0, sizeof(fsm->smart_event));
    memset(counts, 0, sizeof(counts));

    for (size_t j = 1; j <= fsm->num_transitions; j++)
    {
        uint32_t ev = fsm->transitions[j].event;
        if (ev == FSM_EV_NONE || ev >= FSM_SMART_EVENT_SLOTS) {
            return -3;
        }

        size_t idx = counts[ev];
        if (idx >= FSM_MAX_TRANSITIONS) {
            return -4;
        }

        fsm->smart_event[ev].source_state[idx]      = fsm->transitions[j].source_state;
        fsm->smart_event[ev].transition_action[idx] = fsm->transitions[j].transition_action;
        fsm->smart_event[ev].target_state[idx]      = fsm->transitions[j].target_state;
        counts[ev] = idx + 1U;
    }

    return 0;
}

int fsm_init(fsm_t *fsm,
             const fsm_transition_t *transitions,
             size_t num_transitions,
             size_t num_events,
             uint32_t time_period_ticks,
             fsm_state_t *initial_state,
             void *initial_data)
{
    if (fsm == NULL || transitions == NULL || initial_state == NULL) return -1;
    if (num_transitions == 0) return -2;

    memset(fsm, 0, sizeof(fsm_t));

    fsm->transitions     = transitions;
    fsm->num_transitions = num_transitions;
    fsm->num_events      = num_events;
    fsm->terminate_val   = 0;
    fsm->internal        = 0;
    fsm->current_data    = initial_data;
    fsm->fsm_ms_ticks    = time_period_ticks;

    int rc = fsm_smart_events_init(fsm);
    if (rc != 0) {
        return rc;
    }

#if FSM_USE_FREERTOS_API
    fsm->event_queue = xQueueCreateStatic(FSM_MAX_EVENTS,
                                          sizeof(struct fsm_events_t),
                                          (uint8_t *)fsm->events_buff,
                                          &fsm->event_queue_storage);
    if (fsm->event_queue == NULL) return -5;
#else
    if (ringbuff_init(&fsm->event_queue, fsm->events_buff, FSM_MAX_EVENTS, sizeof(struct fsm_events_t)) != 0) {
        return -5;
    }
#endif
    enter_state(fsm, initial_state, initial_state, initial_data);

    return 0;
}

void fsm_deinit(fsm_t *fsm)
{
    if (fsm == NULL) return;

#if FSM_USE_FREERTOS_API
    if (fsm->event_queue != NULL) {
        vQueueDelete(fsm->event_queue);
        fsm->event_queue = NULL;
    }
#else
    ringbuff_flush(&fsm->event_queue);
#endif
    fsm->current_state = NULL;
    fsm->transitions   = NULL;
    fsm->num_transitions = 0;
    fsm->internal      = 0;
}

int fsm_actor_link(fsm_t *fsm, struct fsm_actor_t *actor, int size)
{
    if (fsm == NULL || actor == NULL || size <= 0) return -1;

    for (size_t i = 0; i < FSM_MAX_ACTORS; i++)
    {
        if (fsm->actors_table[i].len == 0) {
            fsm->actors_table[i].actor = actor;
            fsm->actors_table[i].len   = (size_t)size;
            return 0;
        }
    }
    return -2;
}

int fsm_timed_event_set(fsm_state_t *state, uint32_t ticks)
{
    if (state == NULL) return -1;

    state->timer.t_period = ticks;
    state->timer.t_count  = ticks;

    return 0;
}

void fsm_dispatch(fsm_t *fsm, uint32_t event, void *data)
{
    if (fsm == NULL) return;
    if (fsm->num_transitions == 0) return;
    if (event == FSM_EV_NONE || event >= FSM_SMART_EVENT_SLOTS) return;

    struct fsm_events_t new_event = { event, data };

#if FSM_USE_FREERTOS_API
    if (fsm->event_queue == NULL) return;
    if (xPortInIsrContext()) {
        xQueueSendFromISR(fsm->event_queue, &new_event, NULL);
    } else {
        xQueueSend(fsm->event_queue, &new_event, 0);
    }
#else
    ringbuff_put(&fsm->event_queue, &new_event);
#endif
}

static int fsm_process_events(fsm_t *fsm)
{
    if (fsm == NULL) return -1;
    if (fsm->num_transitions == 0 || fsm->current_state == NULL) return -2;

    struct fsm_events_t current_event;

#if FSM_USE_FREERTOS_API
    int event_ready = 0;
    if (xPortInIsrContext()) {
        event_ready = xQueueReceiveFromISR(fsm->event_queue, &current_event, NULL);
    } else {
        event_ready = xQueueReceive(fsm->event_queue, &current_event, 0);
    }
    while (event_ready) {
#else
    while (ringbuff_get(&fsm->event_queue, &current_event) == 0) {
#endif
        if (current_event.event == FSM_EV_NONE || current_event.event >= FSM_SMART_EVENT_SLOTS) {
            goto next_event;
        }

        fsm->internal &= ~FSM_FLAG_HANDLED;

        fsm_state_t *current = fsm->current_state;
        while (((fsm->internal & FSM_FLAG_HANDLED) == 0U) && (current != NULL))
        {
            for (int i = 0; (i < FSM_MAX_TRANSITIONS + 1) && (fsm->smart_event[current_event.event].source_state[i] != NULL); i++)
            {
                if (fsm->smart_event[current_event.event].source_state[i] == current &&
                    fsm->smart_event[current_event.event].target_state[i]->id == FSM_ST_END)
                {
                    end_state(fsm, fsm->smart_event[current_event.event].source_state[i], current_event.data);
                }
                else if (fsm->smart_event[current_event.event].source_state[i] == current)
                {
                    fsm_state_t *lca = find_lca(fsm->current_state, fsm->smart_event[current_event.event].target_state[i]);

                    exit_state(fsm, lca, current_event.data);
                    transition_work(fsm, fsm->smart_event[current_event.event].transition_action[i], current_event.data);
                    enter_state(fsm, lca, fsm->smart_event[current_event.event].target_state[i], current_event.data);

                    if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
                        return fsm->terminate_val;
                    }
                    fsm->internal |= FSM_FLAG_HANDLED;
                }
            }
            current = current->parent;
        }

        if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
            return fsm->terminate_val;
        }

next_event:
#if FSM_USE_FREERTOS_API
        if (xPortInIsrContext()) {
            event_ready = xQueueReceiveFromISR(fsm->event_queue, &current_event, NULL);
        } else {
            event_ready = xQueueReceive(fsm->event_queue, &current_event, 0);
        }
#else
        (void)0;
#endif
    }
    return 0;
}

int fsm_run(fsm_t *fsm)
{
    if (fsm == NULL || fsm->current_state == NULL) return -1;

    if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
        return fsm->terminate_val;
    }

    fsm_process_events(fsm);

    // Run state
    if (fsm->current_state->run_action != NULL) {
        fsm->current_state->run_action(fsm, fsm->current_data);
    }

    // Actors
    actor_action(fsm, fsm->current_state, ACTION_RUN, fsm->current_data);

    return 0;
}

int fsm_state_get(fsm_t *fsm)
{
    if (fsm == NULL || fsm->current_state == NULL) return FSM_ST_NONE;

    return fsm->current_state->id;
}

void fsm_terminate(fsm_t *fsm, int val)
{
    if (fsm == NULL) return;

    fsm->internal |= FSM_FLAG_TERMINATE;
    fsm->terminate_val = val;
}

int fsm_has_pending_events(fsm_t *fsm)
{
    if (fsm == NULL) return -1;

#if FSM_USE_FREERTOS_API
    if (fsm->event_queue == NULL) return -1;
    if (xPortInIsrContext()) {
        return uxQueueMessagesWaitingFromISR(fsm->event_queue) > 0;
    } else {
        return uxQueueMessagesWaiting(fsm->event_queue) > 0;
    }
#else
    return ringbuff_num(&fsm->event_queue) > 0U;
#endif
}

void fsm_flush_events(fsm_t *fsm)
{
    if (fsm == NULL) return;

#if FSM_USE_FREERTOS_API
    if (fsm->event_queue == NULL || xPortInIsrContext()) return;
    xQueueReset(fsm->event_queue);
#else
    ringbuff_flush(&fsm->event_queue);
#endif
}

void fsm_ticks_hook(fsm_t *fsm)
{
    if (fsm == NULL || fsm->current_state == NULL) return;

    struct fsm_events_t new_event = { FSM_TIMEOUT_EV, fsm->current_data };

    if (fsm->timer.t_count > 0U)
    {
        fsm->timer.t_count--;
        if (fsm->timer.t_count == 0U)
        {
#if FSM_USE_FREERTOS_API
            if (fsm->event_queue == NULL) return;
            if (xPortInIsrContext()) {
                xQueueGenericSendFromISR(fsm->event_queue, &new_event, NULL, queueSEND_TO_FRONT);
            } else {
                xQueueGenericSend(fsm->event_queue, &new_event, 0, queueSEND_TO_FRONT);
            }
#else
            ringbuff_put_first(&fsm->event_queue, &new_event);
#endif
#if CONFIG_RUN_ON_TIMER_HOOK
            fsm_run(fsm);
#endif
        }
    }
}
