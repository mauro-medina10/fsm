/**
 * @file fsm_button.c
 * @author Mauro Medina
 * @brief Button debounce FSM example using timed events and an actor
 * @version 2.0.0
 * @date 2025
 *
 * @copyright Copyright (c) 2025
 */
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

#define ANTIBOUNCE_TIME_MS 50U

typedef struct {
    fsm_t fsm;
    int gpio;
    int simulated_level; /* 0 = pressed (active low), 1 = released */
} btn_device_t;

static int gpio_get_level(const btn_device_t *btn)
{
    return btn->simulated_level;
}

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

static void btn_pressed(fsm_t *self, void *data);
static void btn_unpressed(fsm_t *self, void *data);

// Define FSM states
FSM_STATES_INIT(button)
//               name    state id            parent       sub            entry run   exit
FSM_CREATE_STATE(button, ST_ROOT,            FSM_ST_NONE, ST_IDLE,       NULL, NULL, NULL)
FSM_CREATE_STATE(button, ST_IDLE,            ST_ROOT,     FSM_ST_NONE,   NULL, NULL, NULL)
FSM_CREATE_STATE(button, ST_PRESSED,         ST_ROOT,     ST_ANTIBOUNCE, NULL, NULL, NULL)
FSM_CREATE_STATE(button, ST_ANTIBOUNCE,      ST_PRESSED,  FSM_ST_NONE,   NULL, NULL, NULL)
FSM_CREATE_STATE(button, ST_PRESSED_CONFIRM, ST_PRESSED,  FSM_ST_NONE,   NULL, NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(button)
//                    fsm name  State source   event           state target
FSM_TRANSITION_CREATE(button,   ST_IDLE,       EV_PRESS,       ST_PRESSED)
FSM_TRANSITION_CREATE(button,   ST_PRESSED,    EV_UNPRESS,     ST_IDLE)
FSM_TRANSITION_CREATE(button,   ST_ANTIBOUNCE, FSM_TIMEOUT_EV, ST_PRESSED_CONFIRM)
FSM_TRANSITIONS_END()

// Button actor
FSM_ACTOR_INIT(btn_actor)
FSM_ACTOR_CREATE(ST_PRESSED_CONFIRM, btn_pressed, NULL, btn_unpressed)
FSM_ACTOR_END()

/**
 * @brief Simulated GPIO edge handler
 */
static void gpio_isr_handler(void *arg)
{
    btn_device_t *btn = (btn_device_t *)arg;
    fsm_dispatch(&btn->fsm, (gpio_get_level(btn) == 0) ? EV_PRESS : EV_UNPRESS, btn);
}

/**
 * @brief Simulated periodic timer called every 1 ms
 */
static void periodic_timer_1ms(void *data)
{
    fsm_t *fsm = (fsm_t *)data;
    fsm_ticks_hook(fsm);
}

static void btn_pressed(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("Button pressed (debounced)\n");
}

static void btn_unpressed(fsm_t *self, void *data)
{
    (void)self;
    (void)data;
    printf("Button unpressed\n");
}

int main(void)
{
    btn_device_t dev = { .gpio = 0, .simulated_level = 1 };
    int ret = 0;

    printf("--- Starting BUTTON FSM ---\n");

    ret = fsm_init(&dev.fsm,
                   FSM_TRANSITIONS_GET(button),
                   FSM_TRANSITIONS_SIZE(button),
                   EV_LAST,
                   1,
                   &FSM_STATE_GET(button, ST_ROOT),
                   &dev);
    if (ret != 0) {
        return EXIT_FAILURE;
    }

    fsm_timed_event_set(&dev.fsm, &FSM_STATE_GET(button, ST_ANTIBOUNCE), FSM_MS_2_TICKS(&dev.fsm, ANTIBOUNCE_TIME_MS));
    ret |= fsm_actor_link(&dev.fsm, FSM_ACTOR_GET(btn_actor), FSM_ACTOR_SIZE(btn_actor));

    // Simulate a short glitch (< 50 ms) that must NOT confirm
    dev.simulated_level = 0;
    gpio_isr_handler(&dev);
    ret |= fsm_run(&dev.fsm);
    for (unsigned int ms = 0; ms < 20U; ms++) {
        periodic_timer_1ms(&dev.fsm);
    }
    dev.simulated_level = 1;
    gpio_isr_handler(&dev);
    ret |= fsm_run(&dev.fsm);

    // Simulate a valid press held for 60 ms, then released
    dev.simulated_level = 0;
    gpio_isr_handler(&dev);
    ret |= fsm_run(&dev.fsm);
    for (unsigned int ms = 0; ms < 60U; ms++) {
        periodic_timer_1ms(&dev.fsm);
        if (fsm_has_pending_events(&dev.fsm) > 0) {
            ret |= fsm_run(&dev.fsm);
        }
    }
    dev.simulated_level = 1;
    gpio_isr_handler(&dev);
    ret |= fsm_run(&dev.fsm);

    fsm_deinit(&dev.fsm);
    return ret;
}
