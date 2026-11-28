/**
 * @file fsm.c
 * @author Mauro Medina
 * @brief Hierarchical Finite State Machine (FSM) implementation
 * @version 2.0.0
 * @date 2024-07-17
 *
 * @copyright Copyright (c) 2024
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "fsm.h"

#define FSM_FLAG_TERMINATE (1U << 0)
#define FSM_FLAG_RUNNING   (1U << 1)

static const fsm_state_t *find_lca(const fsm_state_t *s1, const fsm_state_t *s2);
static uint8_t find_ancestor_depth(const fsm_state_t *state, const fsm_state_t *ancestor);

static uint32_t state_timer_period_get(const fsm_t *fsm, const fsm_state_t *state)
{
    int steps = 0;
    for (const fsm_state_t *s = state; (s != NULL) && (steps < MAX_HIERARCHY_DEPTH); s = s->parent, steps++)
    {
        for (size_t i = 0; i < FSM_MAX_TIMED_STATES; i++)
        {
            if ((fsm->state_timers[i].state_id == s->id) && (fsm->state_timers[i].t_period > 0U)) {
                return fsm->state_timers[i].t_period;
            }
        }
    }
    return 0U;
}

static void actor_action(fsm_t *fsm, const fsm_state_t *target, enum fsm_action_e action, void *data)
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

static void enter_state(fsm_t *fsm, const fsm_state_t *lca, const fsm_state_t *target, void *data)
{
    const fsm_state_t *state_path[MAX_HIERARCHY_DEPTH];
    const fsm_state_t *state_target = target;
    int depth = 0;

    // Follow default substates (bounded against malformed cycles)
    for (int step = 0; (state_target->default_substate != NULL) && (step < MAX_HIERARCHY_DEPTH); step++) {
        state_target = state_target->default_substate;
    }

    fsm->current_state = state_target;
    fsm->timer_count   = state_timer_period_get(fsm, fsm->current_state);

    // Build path from target to LCA (exclusive)
    for (const fsm_state_t *s = state_target; (s != lca) && (s != NULL); s = s->parent) {
        state_path[depth++] = s;
        if (depth >= MAX_HIERARCHY_DEPTH) break;
    }

    // Execute entry actions from LCA (exclusive) down to target state
    for (int i = depth - 1; i >= 0; i--) {
        if (state_path[i]->entry_action != NULL) {
            state_path[i]->entry_action(fsm, data);
        }
        actor_action(fsm, state_path[i], ACTION_ENTRY, data);
        if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
            return;
        }
    }
}

static void exit_state(fsm_t *fsm, const fsm_state_t *lca, void *data)
{
    int steps = 0;
    for (const fsm_state_t *s = fsm->current_state;
         (s != lca) && (s != NULL) && (steps < MAX_HIERARCHY_DEPTH);
         s = s->parent, steps++)
    {
        if (s->exit_action != NULL) {
            s->exit_action(fsm, data);
        }
        actor_action(fsm, s, ACTION_EXIT, data);
        if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
            return;
        }
    }
}

static void transition_work(fsm_t *fsm, fsm_action_t action, void *data)
{
    if (action != NULL) {
        action(fsm, data);
    }
}

static void execute_transition(fsm_t *fsm,
                               const fsm_state_t *source,
                               const fsm_state_t *target,
                               fsm_action_t action,
                               void *data)
{
    /* Self-transitions (source == target) exit and re-enter source symmetrically. */
    const fsm_state_t *lca = (source == target) ? source->parent : find_lca(fsm->current_state, target);

    exit_state(fsm, lca, data);
    if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
        return;
    }

    transition_work(fsm, action, data);
    if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
        return;
    }

    enter_state(fsm, lca, target, data);
}

static uint8_t find_ancestor_depth(const fsm_state_t *state, const fsm_state_t *ancestor)
{
    uint8_t depth = 0;
    const fsm_state_t *s = state;

    while ((s != NULL) && (depth <= MAX_HIERARCHY_DEPTH))
    {
        if (s == ancestor) {
            return depth;
        }
        s = s->parent;
        depth++;
    }
    return UINT8_MAX; // Not an ancestor
}

static void end_state(fsm_t *fsm, const fsm_state_t *state, void *data)
{
    const fsm_state_t *search_state = state;

    for (int chain = 0; (chain < MAX_HIERARCHY_DEPTH) && (search_state != NULL); chain++)
    {
        const fsm_transition_t *best = NULL;
        uint8_t ancestor_depth = MAX_HIERARCHY_DEPTH + 1;

        // Find the closest ancestor with a matching FSM_END_EV transition
        for (size_t i = 0; i < fsm->num_transitions; i++)
        {
            const fsm_transition_t *t = &fsm->transitions[i];
            if (t->event != FSM_END_EV) {
                continue;
            }

            uint8_t depth = find_ancestor_depth(search_state, t->source_state);
            if (depth >= ancestor_depth) {
                continue;
            }

            if ((t->guard != NULL) && !t->guard(fsm, data)) {
                continue;
            }

            ancestor_depth = depth;
            best = t;
        }

        // If no valid ancestor handles FSM_END_EV, terminate the FSM
        if (best == NULL) {
            fsm_terminate(fsm, FSM_END_EV);
            return;
        }

        if (best->target_state->id == FSM_ST_END) {
            // Chained completion: exit through ancestor, run action, and continue up to parent
            exit_state(fsm, best->source_state->parent, data);
            if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
                return;
            }
            transition_work(fsm, best->transition_action, data);
            if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
                return;
            }
            fsm->current_state = best->source_state->parent;
            search_state = best->source_state->parent;
            continue;
        }

        execute_transition(fsm, best->source_state, best->target_state, best->transition_action, data);
        return;
    }

    fsm_terminate(fsm, FSM_END_EV);
}

static const fsm_state_t *find_lca(const fsm_state_t *s1, const fsm_state_t *s2)
{
    if (s1 == NULL || s2 == NULL) {
        return NULL;
    }

    const fsm_state_t *a = s1;
    const fsm_state_t *b = s2;
    int max_steps = 2 * (MAX_HIERARCHY_DEPTH + 1);

    while ((a != b) && (max_steps-- > 0)) {
        if (a == NULL) {
            a = s2;
        } else if (b == NULL) {
            b = s1;
        } else {
            a = a->parent;
            b = b->parent;
        }
    }
    return (a == b) ? a : NULL;
}

int fsm_init(fsm_t *fsm,
             const fsm_transition_t *transitions,
             size_t num_transitions,
             size_t num_events,
             uint32_t time_period_ticks,
             const fsm_state_t *initial_state,
             void *initial_data)
{
    (void)num_events;

    if (fsm == NULL || transitions == NULL || initial_state == NULL) return -1;
    if (num_transitions == 0) return -2;

    for (size_t i = 0; i < num_transitions; i++) {
        if ((transitions[i].source_state == NULL) ||
            (transitions[i].target_state == NULL) ||
            (transitions[i].event == FSM_EV_NONE))
        {
            return -3;
        }
    }

    memset(fsm, 0, sizeof(fsm_t));

    fsm->transitions     = transitions;
    fsm->num_transitions = num_transitions;
    fsm->terminate_val   = 0;
    fsm->internal        = 0;
    fsm->current_data    = initial_data;
    fsm->fsm_ms_ticks    = time_period_ticks;

#if FSM_USE_FREERTOS_API
    fsm->event_queue = xQueueCreateStatic(FSM_MAX_EVENTS,
                                          sizeof(struct fsm_events_t),
                                          (uint8_t *)fsm->events_buff,
                                          &fsm->event_queue_storage);
    if (fsm->event_queue == NULL) return -4;
#else
    if (ringbuff_init(&fsm->event_queue, fsm->events_buff, FSM_MAX_EVENTS, sizeof(struct fsm_events_t)) != 0) {
        return -4;
    }
#endif
    enter_state(fsm, NULL, initial_state, initial_data);

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
    fsm->current_state   = NULL;
    fsm->transitions     = NULL;
    fsm->num_transitions = 0;
    fsm->timer_count     = 0;
    fsm->internal        = 0;
}

int fsm_actor_link(fsm_t *fsm, const struct fsm_actor_t *actor, size_t size)
{
    if (fsm == NULL || actor == NULL || size == 0U) return -1;

    for (size_t i = 0; i < FSM_MAX_ACTORS; i++)
    {
        if (fsm->actors_table[i].len == 0U) {
            fsm->actors_table[i].actor = actor;
            fsm->actors_table[i].len   = size;
            return 0;
        }
    }
    return -2;
}

int fsm_timed_event_set(fsm_t *fsm, const fsm_state_t *state, uint32_t ticks)
{
    if (fsm == NULL || state == NULL || state->id == FSM_ST_NONE) return -1;

    size_t empty_slot = FSM_MAX_TIMED_STATES;
    bool updated = false;

    for (size_t i = 0; i < FSM_MAX_TIMED_STATES; i++)
    {
        if (fsm->state_timers[i].state_id == state->id) {
            if (ticks == 0U) {
                fsm->state_timers[i].state_id = FSM_ST_NONE;
                fsm->state_timers[i].t_period = 0U;
            } else {
                fsm->state_timers[i].t_period = ticks;
            }
            updated = true;
            break;
        }
        if ((empty_slot == FSM_MAX_TIMED_STATES) && (fsm->state_timers[i].state_id == FSM_ST_NONE)) {
            empty_slot = i;
        }
    }

    if (!updated && (ticks > 0U)) {
        if (empty_slot == FSM_MAX_TIMED_STATES) {
            return -2;
        }
        fsm->state_timers[empty_slot].state_id = state->id;
        fsm->state_timers[empty_slot].t_period = ticks;
    }

    // If this state is currently active (or an active ancestor), refresh the active countdown
    if ((fsm->current_state != NULL) && (find_ancestor_depth(fsm->current_state, state) != UINT8_MAX)) {
        fsm->timer_count = state_timer_period_get(fsm, fsm->current_state);
    }

    return 0;
}

void *fsm_data_get(const fsm_t *fsm)
{
    if (fsm == NULL) return NULL;
    return fsm->current_data;
}

void fsm_data_set(fsm_t *fsm, void *data)
{
    if (fsm == NULL) return;
    fsm->current_data = data;
}

void fsm_dispatch(fsm_t *fsm, uint32_t event, void *data)
{
    if (fsm == NULL) return;
    if (fsm->num_transitions == 0U || event == FSM_EV_NONE) return;

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
    if (fsm->num_transitions == 0U || fsm->current_state == NULL) return -2;

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
        if (current_event.event != FSM_EV_NONE) {
            bool handled = false;

            const fsm_state_t *current = fsm->current_state;
            int depth_steps = 0;
            while (!handled &&
                   (current != NULL) &&
                   (depth_steps < MAX_HIERARCHY_DEPTH))
            {
                for (size_t i = 0; i < fsm->num_transitions; i++)
                {
                    const fsm_transition_t *t = &fsm->transitions[i];
                    if ((t->event != current_event.event) || (t->source_state != current)) {
                        continue;
                    }
                    if ((t->guard != NULL) && !t->guard(fsm, current_event.data)) {
                        continue;
                    }

                    if (t->target_state->id == FSM_ST_END) {
                        transition_work(fsm, t->transition_action, current_event.data);
                        if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
                            return fsm->terminate_val;
                        }
                        end_state(fsm, t->source_state, current_event.data);
                    } else {
                        execute_transition(fsm, t->source_state, t->target_state, t->transition_action, current_event.data);
                    }

                    handled = true;
                    if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
                        return fsm->terminate_val;
                    }
                    break;
                }
                current = current->parent;
                depth_steps++;
            }

            if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
                return fsm->terminate_val;
            }
        }

#if FSM_USE_FREERTOS_API
        if (xPortInIsrContext()) {
            event_ready = xQueueReceiveFromISR(fsm->event_queue, &current_event, NULL);
        } else {
            event_ready = xQueueReceive(fsm->event_queue, &current_event, 0);
        }
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

    if ((fsm->internal & FSM_FLAG_RUNNING) != 0U) {
        return -2;
    }

    fsm->internal |= FSM_FLAG_RUNNING;
    int ret = 0;

    int rc = fsm_process_events(fsm);
    if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
        ret = fsm->terminate_val;
        goto out;
    }
    if (rc < 0) {
        ret = rc;
        goto out;
    }

    // Run state
    if (fsm->current_state->run_action != NULL) {
        fsm->current_state->run_action(fsm, fsm->current_data);
        if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
            ret = fsm->terminate_val;
            goto out;
        }
    }

    // Actors
    actor_action(fsm, fsm->current_state, ACTION_RUN, fsm->current_data);
    if ((fsm->internal & FSM_FLAG_TERMINATE) != 0U) {
        ret = fsm->terminate_val;
        goto out;
    }

out:
    fsm->internal &= ~FSM_FLAG_RUNNING;
    return ret;
}

int fsm_state_get(const fsm_t *fsm)
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

int fsm_has_pending_events(const fsm_t *fsm)
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

    if (fsm->timer_count > 0U)
    {
        fsm->timer_count--;
        if (fsm->timer_count == 0U)
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
