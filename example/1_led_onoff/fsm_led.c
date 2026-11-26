/**
 * @file fsm_led.c
 * @author Mauro Medina
 * @brief Simple LED on/off FSM example
 * @version 2.0.0
 * @date 2025
 *
 * @copyright Copyright (c) 2025
 */
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

typedef struct {
    int pin;
    int level;
} led_device_t;

// Define states
enum {
    ST_ROOT = FSM_ST_FIRST,
    ST_OFF,
    ST_ON,
};

// Define events
enum {
    EV_ON = FSM_EV_FIRST,
    EV_OFF,
    EV_LAST,
};

// Action function prototypes
static void enter_on(fsm_t *self, void *data);
static void enter_off(fsm_t *self, void *data);

// Define FSM states
FSM_STATES_INIT(led)
//               name  state id  parent       sub          entry      run   exit
FSM_CREATE_STATE(led,  ST_ROOT,  FSM_ST_NONE, ST_OFF,      NULL,      NULL, NULL)
FSM_CREATE_STATE(led,  ST_OFF,   ST_ROOT,     FSM_ST_NONE, enter_off, NULL, NULL)
FSM_CREATE_STATE(led,  ST_ON,    ST_ROOT,     FSM_ST_NONE, enter_on,  NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(led)
//                    fsm name  State source  event   state target
FSM_TRANSITION_CREATE(led,      ST_OFF,       EV_ON,  ST_ON)
FSM_TRANSITION_CREATE(led,      ST_ON,        EV_OFF, ST_OFF)
FSM_TRANSITIONS_END()

// Action function implementations
static void enter_off(fsm_t *self, void *data)
{
    (void)data;
    led_device_t *dev = (led_device_t *)fsm_data_get(self);
    if (dev != NULL) {
        dev->level = 0;
    }
    printf("LED (pin %d) OFF\n", dev ? dev->pin : -1);
}

static void enter_on(fsm_t *self, void *data)
{
    (void)data;
    led_device_t *dev = (led_device_t *)fsm_data_get(self);
    if (dev != NULL) {
        dev->level = 1;
    }
    printf("LED (pin %d) ON\n", dev ? dev->pin : -1);
}

int main(void)
{
    fsm_t led = {0};
    led_device_t dev = { .pin = 2, .level = 0 };
    int ret = 0;

    printf("--- Starting LED ON/OFF FSM ---\n");

    ret = fsm_init(&led,
                   FSM_TRANSITIONS_GET(led),
                   FSM_TRANSITIONS_SIZE(led),
                   EV_LAST,
                   FSM_NO_TICKS,
                   &FSM_STATE_GET(led, ST_ROOT),
                   &dev);
    if (ret != 0) {
        return EXIT_FAILURE;
    }

    // Power on the LED
    fsm_dispatch(&led, EV_ON, NULL);
    ret |= fsm_run(&led);

    // Power off the LED
    fsm_dispatch(&led, EV_OFF, NULL);
    ret |= fsm_run(&led);

    printf("End %d\n", ret);
    fsm_deinit(&led);

    return ret;
}
