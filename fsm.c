/**
 * @file fsm.c
 * @author Mauro Medina
 * @brief 
 * @version 1.0.1
 * @date 2024-07-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "fsm.h"

#ifdef FREERTOS_API
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#else
#include "ring_buff.h"
#endif 

struct internal_ctx {
	int terminate:  1;
	int is_exit:    1;
    int handled:    1;
};

static void enter_state(fsm_t *fsm, fsm_state_t *lca, fsm_state_t *target, void *data) {
    fsm_state_t* state_path[MAX_HIERARCHY_DEPTH];
    fsm_state_t* state_target = (fsm_state_t*)target;
    int depth = 0;

    // Check for default substate
    while (state_target->default_substate) {
        state_target = state_target->default_substate;
    }

    // Build path from target to LCA (exclusive)
    for (fsm_state_t* s = (fsm_state_t*)state_target; s != lca && s != NULL; s = s->parent) {
        state_path[depth++] = s;
        if (depth >= MAX_HIERARCHY_DEPTH) break;
    }

    // Execute entry actions from LCA (exclusive) to target state
    for (int i = depth - 1; i >= 0; i--) {
        if (state_path[i]->entry_action) {
            state_path[i]->entry_action(fsm, data);
        }
    }

    // When source state is target state, execute entry action
    if((lca == state_target) && (depth == 0))
    {
        if(lca->entry_action) lca->entry_action(fsm, data);
    }
    
    // Actors
    for (size_t i = 0; ((i < FSM_MAX_ACTORS) && (fsm->actors_table[i].actor != NULL)); i++)
    {
        for (size_t j = FSM_ACTOR_FIRST; j < fsm->actors_table[i].len; j++)
        {
            if((fsm->actors_table[i].actor[j].state_id == target->state_id) && (fsm->actors_table[i].actor[j].entry_action != NULL)) fsm->actors_table[i].actor[j].entry_action(fsm, data);
        }
    }

    fsm->current_state = (fsm_state_t*)state_target;
}

static void exit_state(fsm_t *fsm, fsm_state_t *state, void *data) {
    for (fsm_state_t* s = fsm->current_state; s != state && s != NULL; s = s->parent) {
        if (s->exit_action) {
            s->exit_action(fsm, data);
        }
        s->t_count = s->t_period;
    }
    // Actors
    for (size_t i = 0; ((i < FSM_MAX_ACTORS) && (fsm->actors_table[i].actor != NULL)); i++)
    {
        for (size_t j = FSM_ACTOR_FIRST; j < fsm->actors_table[i].len; j++)
        {
            if((fsm->actors_table[i].actor[j].state_id == state->state_id) && (fsm->actors_table[i].actor[j].exit_action != NULL)) fsm->actors_table[i].actor[j].exit_action(fsm, data);
        }
    }
}

static void transition_work(fsm_t *fsm, fsm_action_t action, void *data) {
    if (action) {
        action(fsm, data);
    }
}

static fsm_state_t* find_lca(fsm_state_t *s1, fsm_state_t *s2) {
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

static void fsm_smart_events_init(fsm_t *fsm)
{
    uint32_t idx = 0;

    memset(fsm->smart_event, 0, sizeof(fsm_smt_events_t));

    // Sorts transitions by event id
    for (int i = FSM_TIMEOUT_EV; i <= (fsm->num_events+FSM_EV_FIRST); i++)
    {
        for (int j = 1; j <= fsm->num_transitions; j++)
        {
            if(fsm->transitions[j].event == i)
            {
                fsm->smart_event[i].source_state[idx] = fsm->transitions[j].source_state;
                fsm->smart_event[i].transition_action[idx] = fsm->transitions[j].transition_action;
                fsm->smart_event[i].target_state[idx] = fsm->transitions[j].target_state;
                if(++idx >= FSM_MAX_TRANSITIONS) break;
            }
        }
        idx = 0;
    }
    
}

int fsm_init(fsm_t *fsm, const fsm_transition_t *transitions, size_t num_transitions, size_t num_events, uint32_t time_period_ticks, fsm_state_t* initial_state, void *initial_data) {
    struct internal_ctx *const internal = (void *)&fsm->internal;

    if(fsm == NULL || transitions == NULL || initial_state == NULL) return -1;
    if(num_transitions == 0) return -2;

    fsm->transitions         = transitions;
    fsm->num_transitions     = num_transitions;
    fsm->num_events          = num_events;
    fsm->terminate_val       = 0;   
    internal->terminate      = false;
    internal->is_exit        = false;
    fsm->current_data        = initial_data;
    fsm->fsm_ms_ticks        = time_period_ticks;
    
    memset(fsm->actors_table, 0, sizeof(fsm->actors_table));

    fsm_smart_events_init(fsm);

#ifdef FREERTOS_API
    fsm->event_queue = xQueueCreate(FSM_MAX_EVENTS, sizeof(struct fsm_events_t));
    if(fsm->event_queue == NULL) return -3;
#else
    ringbuff_init(&fsm->event_queue, fsm->events_buff, FSM_MAX_EVENTS, sizeof(struct fsm_events_t));
#endif
    enter_state(fsm, initial_state, initial_state, initial_data);

    return 0;
}

int fsm_actor_link(fsm_t *fsm, struct fsm_actor_t *actor, int size) {
    
    if(fsm == NULL || actor == NULL) return -1;

    for (uint16_t i = 0; i < FSM_MAX_ACTORS; i++)
    {
        // Search empty spot
        if(fsm->actors_table[i].len == 0)
        {
            fsm->actors_table[i].actor = actor;
            fsm->actors_table[i].len = size;

            return 0;
        }
    }
    return -2;
}

int fsm_timed_event_set(fsm_state_t *state, uint32_t ticks)
{
    if(state == NULL) return -1;

    state->t_period = ticks;
    state->t_count = ticks;

    return 0;
}

void fsm_dispatch(fsm_t *fsm, uint32_t event, void *data) {
    
    if(fsm == NULL) return;
    if(fsm->num_transitions == 0) return;

    struct fsm_events_t new_event = {event, data};

#ifdef FREERTOS_API
    if(xPortInIsrContext())
    {
        xQueueSendFromISR(fsm->event_queue, &new_event, NULL);
    }else
    {
        xQueueSend(fsm->event_queue, &new_event, 0);
    }
#else
    ringbuff_put(&fsm->event_queue, &new_event);
#endif    
}

static int fsm_process_events(fsm_t *fsm) {
    
    if(fsm == NULL) return -1;
    if(fsm->num_transitions == 0) return -2;

    struct internal_ctx *const internal = (void *)&fsm->internal;

    struct fsm_events_t current_event;

#ifdef FREERTOS_API
    int event_ready = 0;
    if(xPortInIsrContext())
    {
        event_ready = xQueueReceiveFromISR(fsm->event_queue, &current_event, NULL);
    }else
    {
        event_ready = xQueueReceive(fsm->event_queue, &current_event, 0);
    }
    while(event_ready) {
#else
    while (ringbuff_get(&fsm->event_queue, &current_event) == 0) {
#endif    
        internal->handled = 0;

        fsm_state_t* current = fsm->current_state;
        while (internal->handled == 0 && current != NULL) 
        {
            for (int i = 0; (i < FSM_MAX_TRANSITIONS+1) && (fsm->smart_event[current_event.event].source_state[i] != NULL); i++)
            {
                if(fsm->smart_event[current_event.event].source_state[i] == current)
                {
                    fsm_state_t* lca = find_lca(fsm->current_state, fsm->smart_event[current_event.event].target_state[i]);

                    exit_state(fsm, lca, current_event.data);
                    transition_work(fsm, fsm->smart_event[current_event.event].transition_action[i], current_event.data);
                    enter_state(fsm, lca, fsm->smart_event[current_event.event].target_state[i], current_event.data);

                    /* No need to continue if terminate was set in the exit action */
                    if (internal->terminate) {
                        return fsm->terminate_val;
                    }
                    internal->handled = 1;
                }
            }
            current = current->parent;
        }
        
        if (internal->terminate) {
            return fsm->terminate_val;
        }
#ifdef FREERTOS_API        
        if(xPortInIsrContext())
        {
            event_ready = xQueueReceiveFromISR(fsm->event_queue, &current_event, NULL);
        }else
        {
            event_ready = xQueueReceive(fsm->event_queue, &current_event, 0);
        }
#endif        
    }
    return 0;
}

int fsm_run(fsm_t *fsm)
{
    if(fsm == NULL) return -1;

    struct internal_ctx *const internal = (void *)&fsm->internal;

    /* No need to continue if terminate was set */
	if (internal->terminate) {
		return fsm->terminate_val;
	}
    
    fsm_process_events(fsm);

    // Run state
    if (fsm->current_state->run_action) {
        fsm->current_state->run_action(fsm, fsm->current_data);
    }

    // Actors
    for (size_t i = 0; ((i < FSM_MAX_ACTORS) && (fsm->actors_table[i].actor != NULL)); i++)
    {
        for (size_t j = FSM_ACTOR_FIRST; j < fsm->actors_table[i].len; j++)
        {
            if((fsm->actors_table[i].actor[j].state_id == fsm->current_state->state_id) && (fsm->actors_table[i].actor[j].run_action != NULL)) fsm->actors_table[i].actor[j].run_action(fsm, fsm->current_data);
        }
    }
    return 0;
}

int fsm_state_get(fsm_t *fsm)
{
    if(fsm == NULL) return FSM_ST_NONE;

    return fsm->current_state->state_id;
}

void fsm_terminate(fsm_t *fsm, int val)
{
    if(fsm == NULL) return;

    struct internal_ctx *const internal = (void *)&fsm->internal;

    internal->terminate = true;
    fsm->terminate_val = val;  
}

int fsm_has_pending_events(fsm_t *fsm) {
    if(fsm == NULL) return -1;

#ifdef FREERTOS_API
        if(xPortInIsrContext())
        {
            return uxQueueMessagesWaitingFromISR(fsm->event_queue) > 0;
        }else
        {
            return uxQueueMessagesWaiting(fsm->event_queue) > 0;
        }
#else
    return ringbuff_num(&fsm->event_queue) > 0;
#endif
}

void fsm_flush_events(fsm_t *fsm) {
    
    if(fsm == NULL) return;

#ifdef FREERTOS_API
    xQueueReset(fsm->event_queue);
#else
    ringbuff_flush(&fsm->event_queue);
#endif
}

void fsm_ticks_hook(fsm_t *fsm)
{
    struct fsm_events_t new_event = {FSM_TIMEOUT_EV, fsm->current_data};

    if(fsm->current_state->t_count > 0)
    {
        fsm->current_state->t_count--;
        if(fsm->current_state->t_count == 0) 
        {
            ringbuff_put_first(&fsm->event_queue, &new_event);
#ifdef CONFIG_RUN_ON_TIMER_HOOK            
            fsm_run(fsm);
#endif            
        }
    }
}