/**
 * @file fsm_led.c
 * @author Mauro Medina
 * @brief Hierarchical LED blinky FSM example with timed events
 * @version 2.0.0
 * @date 2025
 *
 * @copyright Copyright (c) 2025
 */
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

#define BLINK_PERIOD_MS 500U

typedef struct {
    int pin;
    int level;
} led_device_t;

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
static void enter_on(fsm_t *self, void *data);
static void enter_off(fsm_t *self, void *data);

// Define FSM states
FSM_STATES_INIT(led)
//               name  state id      parent       sub           entry      run   exit
FSM_CREATE_STATE(led,  ST_ROOT,      FSM_ST_NONE, ST_OFF,       NULL,      NULL, NULL)
FSM_CREATE_STATE(led,  ST_OFF,       ST_ROOT,     FSM_ST_NONE,  enter_off, NULL, NULL)
FSM_CREATE_STATE(led,  ST_ON,        ST_ROOT,     ST_ON_IDLE,   enter_on,  NULL, NULL)
FSM_CREATE_STATE(led,  ST_ON_IDLE,   ST_ON,       FSM_ST_NONE,  NULL,      NULL, NULL)
FSM_CREATE_STATE(led,  ST_BLINK,     ST_ON,       ST_BLINK_OFF, NULL,      NULL, NULL)
FSM_CREATE_STATE(led,  ST_BLINK_OFF, ST_BLINK,    FSM_ST_NONE,  enter_off, NULL, NULL)
FSM_CREATE_STATE(led,  ST_BLINK_ON,  ST_BLINK,    FSM_ST_NONE,  enter_on,  NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(led)
//                    fsm name  State source  event           state target
FSM_TRANSITION_CREATE(led,      ST_OFF,       EV_ON,          ST_ON)
FSM_TRANSITION_CREATE(led,      ST_ON,        EV_OFF,         ST_OFF)
FSM_TRANSITION_CREATE(led,      ST_OFF,       EV_TOGGLE,      ST_ON)
FSM_TRANSITION_CREATE(led,      ST_ON,        EV_TOGGLE,      ST_OFF)
FSM_TRANSITION_CREATE(led,      ST_ON_IDLE,   EV_BLINK,       ST_BLINK)
FSM_TRANSITION_CREATE(led,      ST_BLINK_ON,  FSM_TIMEOUT_EV, ST_BLINK_OFF)
FSM_TRANSITION_CREATE(led,      ST_BLINK_OFF, FSM_TIMEOUT_EV, ST_BLINK_ON)
FSM_TRANSITIONS_END()

// Action function implementations
static void enter_off(fsm_t *self, void *data)
{
    (void)data;
    led_device_t *dev = (led_device_t *)fsm_data_get(self);
    if (dev != NULL) {
        dev->level = 0;
    }
    printf("LED OFF\n");
}

static void enter_on(fsm_t *self, void *data)
{
    (void)data;
    led_device_t *dev = (led_device_t *)fsm_data_get(self);
    if (dev != NULL) {
        dev->level = 1;
    }
    printf("LED ON\n");
}

/**
 * @brief Simulated periodic timer called every 1 ms
 */
static void periodic_timer_1ms(void *data)
{
    fsm_t *fsm = (fsm_t *)data;
    fsm_ticks_hook(fsm);
}

int main(void)
{
    fsm_t led = {0};
    led_device_t dev = { .pin = 2, .level = 0 };
    int ret = 0;

    printf("--- Starting LED Blinky FSM ---\n");

    ret = fsm_init(&led,
                   FSM_TRANSITIONS_GET(led),
                   FSM_TRANSITIONS_SIZE(led),
                   EV_LAST,
                   1,
                   &FSM_STATE_GET(led, ST_ROOT),
                   &dev);
    if (ret != 0) {
        return EXIT_FAILURE;
    }

    fsm_timed_event_set(&led, &FSM_STATE_GET(led, ST_BLINK_ON),  FSM_MS_2_TICKS(&led, BLINK_PERIOD_MS));
    fsm_timed_event_set(&led, &FSM_STATE_GET(led, ST_BLINK_OFF), FSM_MS_2_TICKS(&led, BLINK_PERIOD_MS));

    // Power on the LED and start blinking
    fsm_dispatch(&led, EV_ON, NULL);
    ret |= fsm_run(&led);

    fsm_dispatch(&led, EV_BLINK, NULL);
    ret |= fsm_run(&led);

    // Simulate 2 seconds of 1 ms ticks (4 blink toggles)
    for (unsigned int ms = 0; ms < 2000U; ms++) {
        periodic_timer_1ms(&led);
        if (fsm_has_pending_events(&led) > 0) {
            ret |= fsm_run(&led);
        }
    }

    printf("End %d\n", ret);
    fsm_deinit(&led);

    return ret;
}
