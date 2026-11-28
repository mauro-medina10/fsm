/**
 * @file fsm_music.c
 * @author Mauro Medina
 * @brief Hierarchical music player FSM example with an LED actor
 * @version 2.0.0
 * @date 2024-07-17
 *
 * @copyright Copyright (c) 2024
 */
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

#ifndef LOG_CHECK
#define LOG_CHECK(val) ((val) ? "OK" : "ERROR")
#endif

// Define states
enum {
    ST_ROOT = FSM_ST_FIRST,
    ST_OFF,
    ST_ON,
    ST_PLAYING,
    ST_NORMAL,
    ST_SHUFFLE,
    ST_REPEAT,
    ST_PAUSED,
    ST_MENU,
    ST_VOLUME_ADJUST,
    ST_PLAYLIST_SELECT,
    ST_LOW_BATTERY,
};

// Define events
enum {
    EV_POWER = FSM_EV_FIRST,
    EV_PLAY,
    EV_PAUSE,
    EV_STOP,
    EV_NEXT,
    EV_PREV,
    EV_MODE_CHANGE,
    EV_MENU,
    EV_VOLUME_UP,
    EV_VOLUME_DOWN,
    EV_SELECT,
    EV_BACK,
    EV_LOW_BATTERY,
    EV_CHARGE,
    EV_LAST,
};

// Action function prototypes
static void enter_root(fsm_t *self, void *data);
static void enter_off(fsm_t *self, void *data);
static void enter_on(fsm_t *self, void *data);
static void enter_playing(fsm_t *self, void *data);
static void enter_normal(fsm_t *self, void *data);
static void enter_shuffle(fsm_t *self, void *data);
static void enter_repeat(fsm_t *self, void *data);
static void enter_paused(fsm_t *self, void *data);
static void enter_menu(fsm_t *self, void *data);
static void enter_volume_adjust(fsm_t *self, void *data);
static void enter_playlist_select(fsm_t *self, void *data);
static void enter_low_battery(fsm_t *self, void *data);

static void run_root(fsm_t *self, void *data);
static void run_off(fsm_t *self, void *data);
static void run_on(fsm_t *self, void *data);
static void run_playing(fsm_t *self, void *data);
static void run_normal(fsm_t *self, void *data);
static void run_shuffle(fsm_t *self, void *data);
static void run_repeat(fsm_t *self, void *data);
static void run_paused(fsm_t *self, void *data);
static void run_menu(fsm_t *self, void *data);
static void run_volume_adjust(fsm_t *self, void *data);
static void run_playlist_select(fsm_t *self, void *data);
static void run_low_battery(fsm_t *self, void *data);

// LED actor
static void led_enter_off(fsm_t *self, void *data);
static void led_enter_on(fsm_t *self, void *data);
static void led_run_playing(fsm_t *self, void *data);

// Define FSM states
FSM_STATES_INIT(music_player)
//               name          state id            parent       sub          entry                  run                  exit
FSM_CREATE_STATE(music_player, ST_ROOT,            FSM_ST_NONE, ST_OFF,      enter_root,            run_root,            NULL)
FSM_CREATE_STATE(music_player, ST_OFF,             ST_ROOT,     FSM_ST_NONE, enter_off,             run_off,             NULL)
FSM_CREATE_STATE(music_player, ST_ON,              ST_ROOT,     ST_PAUSED,   enter_on,              run_on,              NULL)
FSM_CREATE_STATE(music_player, ST_PLAYING,         ST_ON,       ST_NORMAL,   enter_playing,         run_playing,         NULL)
FSM_CREATE_STATE(music_player, ST_NORMAL,          ST_PLAYING,  FSM_ST_NONE, enter_normal,          run_normal,          NULL)
FSM_CREATE_STATE(music_player, ST_SHUFFLE,         ST_PLAYING,  FSM_ST_NONE, enter_shuffle,         run_shuffle,         NULL)
FSM_CREATE_STATE(music_player, ST_REPEAT,          ST_PLAYING,  FSM_ST_NONE, enter_repeat,          run_repeat,          NULL)
FSM_CREATE_STATE(music_player, ST_PAUSED,          ST_ON,       FSM_ST_NONE, enter_paused,          run_paused,          NULL)
FSM_CREATE_STATE(music_player, ST_MENU,            ST_ON,       FSM_ST_NONE, enter_menu,            run_menu,            NULL)
FSM_CREATE_STATE(music_player, ST_VOLUME_ADJUST,   ST_MENU,     FSM_ST_NONE, enter_volume_adjust,   run_volume_adjust,   NULL)
FSM_CREATE_STATE(music_player, ST_PLAYLIST_SELECT, ST_MENU,     FSM_ST_NONE, enter_playlist_select, run_playlist_select, NULL)
FSM_CREATE_STATE(music_player, ST_LOW_BATTERY,     ST_ROOT,     FSM_ST_NONE, enter_low_battery,     run_low_battery,     NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(music_player)
//                    fsm name      State source        event           state target
FSM_TRANSITION_CREATE(music_player, ST_OFF,             EV_POWER,       ST_ON)
FSM_TRANSITION_CREATE(music_player, ST_ON,              EV_POWER,       ST_OFF)
FSM_TRANSITION_CREATE(music_player, ST_PAUSED,          EV_PLAY,        ST_PLAYING)
FSM_TRANSITION_CREATE(music_player, ST_PLAYING,         EV_PAUSE,       ST_PAUSED)
FSM_TRANSITION_CREATE(music_player, ST_PLAYING,         EV_STOP,        ST_PAUSED)
FSM_TRANSITION_CREATE(music_player, ST_NORMAL,          EV_MODE_CHANGE, ST_SHUFFLE)
FSM_TRANSITION_CREATE(music_player, ST_SHUFFLE,         EV_MODE_CHANGE, ST_REPEAT)
FSM_TRANSITION_CREATE(music_player, ST_REPEAT,          EV_MODE_CHANGE, ST_NORMAL)
FSM_TRANSITION_CREATE(music_player, ST_ON,              EV_MENU,        ST_MENU)
FSM_TRANSITION_CREATE(music_player, ST_MENU,            EV_BACK,        ST_ON)
FSM_TRANSITION_CREATE(music_player, ST_MENU,            EV_VOLUME_UP,   ST_VOLUME_ADJUST)
FSM_TRANSITION_CREATE(music_player, ST_MENU,            EV_VOLUME_DOWN, ST_VOLUME_ADJUST)
FSM_TRANSITION_CREATE(music_player, ST_VOLUME_ADJUST,   EV_BACK,        ST_MENU)
FSM_TRANSITION_CREATE(music_player, ST_MENU,            EV_SELECT,      ST_PLAYLIST_SELECT)
FSM_TRANSITION_CREATE(music_player, ST_PLAYLIST_SELECT, EV_BACK,        ST_MENU)
FSM_TRANSITION_CREATE(music_player, ST_ROOT,            EV_LOW_BATTERY, ST_LOW_BATTERY)
FSM_TRANSITION_CREATE(music_player, ST_LOW_BATTERY,     EV_CHARGE,      ST_ON)
FSM_TRANSITIONS_END()

// Actor LED
FSM_ACTOR_INIT(speaker_led)
FSM_ACTOR_CREATE(ST_OFF,     led_enter_off, NULL,            NULL)
FSM_ACTOR_CREATE(ST_ON,      led_enter_on,  NULL,            NULL)
FSM_ACTOR_CREATE(ST_NORMAL,  NULL,          led_run_playing, NULL)
FSM_ACTOR_CREATE(ST_SHUFFLE, NULL,          led_run_playing, NULL)
FSM_ACTOR_CREATE(ST_REPEAT,  NULL,          led_run_playing, NULL)
FSM_ACTOR_END()

// Action function implementations
static void enter_root(fsm_t *self, void *data)            { (void)self; (void)data; printf("Entering ROOT state\n"); }
static void enter_off(fsm_t *self, void *data)             { (void)self; (void)data; printf("Entering OFF state\n"); }
static void enter_on(fsm_t *self, void *data)              { (void)self; (void)data; printf("Entering ON state\n"); }
static void enter_playing(fsm_t *self, void *data)         { (void)self; (void)data; printf("Entering PLAYING state\n"); }
static void enter_normal(fsm_t *self, void *data)          { (void)self; (void)data; printf("Entering NORMAL play state\n"); }
static void enter_shuffle(fsm_t *self, void *data)         { (void)self; (void)data; printf("Entering SHUFFLE play state\n"); }
static void enter_repeat(fsm_t *self, void *data)          { (void)self; (void)data; printf("Entering REPEAT play state\n"); }
static void enter_paused(fsm_t *self, void *data)          { (void)self; (void)data; printf("Entering PAUSED state\n"); }
static void enter_menu(fsm_t *self, void *data)            { (void)self; (void)data; printf("Entering MENU state\n"); }
static void enter_volume_adjust(fsm_t *self, void *data)   { (void)self; (void)data; printf("Entering VOLUME ADJUST state\n"); }
static void enter_playlist_select(fsm_t *self, void *data) { (void)self; (void)data; printf("Entering PLAYLIST SELECT state\n"); }
static void enter_low_battery(fsm_t *self, void *data)     { (void)self; (void)data; printf("Entering LOW BATTERY state\n"); }

static void run_root(fsm_t *self, void *data)            { (void)self; (void)data; printf("Running ROOT state\n"); }
static void run_off(fsm_t *self, void *data)             { (void)self; (void)data; printf("Music player is OFF\n"); }
static void run_on(fsm_t *self, void *data)              { (void)self; (void)data; printf("Music player is ON\n"); }
static void run_playing(fsm_t *self, void *data)         { (void)self; (void)data; printf("Music is playing\n"); }
static void run_normal(fsm_t *self, void *data)          { (void)self; (void)data; printf("Playing in NORMAL mode\n"); }
static void run_shuffle(fsm_t *self, void *data)         { (void)self; (void)data; printf("Playing in SHUFFLE mode\n"); }
static void run_repeat(fsm_t *self, void *data)          { (void)self; (void)data; printf("Playing in REPEAT mode\n"); }
static void run_paused(fsm_t *self, void *data)          { (void)self; (void)data; printf("Music is PAUSED\n"); }
static void run_menu(fsm_t *self, void *data)            { (void)self; (void)data; printf("In MENU\n"); }
static void run_volume_adjust(fsm_t *self, void *data)   { (void)self; (void)data; printf("Adjusting VOLUME\n"); }
static void run_playlist_select(fsm_t *self, void *data) { (void)self; (void)data; printf("Selecting PLAYLIST\n"); }
static void run_low_battery(fsm_t *self, void *data)     { (void)self; (void)data; printf("LOW BATTERY warning\n"); }

// LED actor
static void led_enter_off(fsm_t *self, void *data)   { (void)self; (void)data; printf("Speaker's LED is off\n"); }
static void led_enter_on(fsm_t *self, void *data)    { (void)self; (void)data; printf("Speaker's LED is on\n"); }
static void led_run_playing(fsm_t *self, void *data) { (void)self; (void)data; printf("Speaker's LED is dancing\n"); }

int main(void)
{
    fsm_t music_player = {0};
    int ret = 0;

    printf("--- Starting Complex Music Player Simulation ---\n");

    ret = fsm_init(&music_player,
                   FSM_TRANSITIONS_GET(music_player),
                   FSM_TRANSITIONS_SIZE(music_player),
                   EV_LAST,
                   0,
                   &FSM_STATE_GET(music_player, ST_ROOT),
                   NULL);
    if (ret != 0) {
        return EXIT_FAILURE;
    }

    ret |= fsm_actor_link(&music_player, FSM_ACTOR_GET(speaker_led), FSM_ACTOR_SIZE(speaker_led));

    ret |= fsm_run(&music_player); // OFF (default substate of ROOT)

    fsm_dispatch(&music_player, EV_POWER, NULL);
    ret |= fsm_run(&music_player);
    printf("Turning on the player... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_PAUSED));

    fsm_dispatch(&music_player, EV_PLAY, NULL);
    ret |= fsm_run(&music_player);
    printf("Starting playback... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_NORMAL));

    fsm_dispatch(&music_player, EV_MODE_CHANGE, NULL);
    ret |= fsm_run(&music_player);
    printf("Changing play mode... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_SHUFFLE));

    fsm_dispatch(&music_player, EV_MENU, NULL);
    ret |= fsm_run(&music_player);
    printf("Opening menu... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_MENU));

    fsm_dispatch(&music_player, EV_VOLUME_UP, NULL);
    ret |= fsm_run(&music_player);
    printf("Adjusting volume... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_VOLUME_ADJUST));

    fsm_dispatch(&music_player, EV_BACK, NULL);
    ret |= fsm_run(&music_player);
    printf("Going back to menu... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_MENU));

    fsm_dispatch(&music_player, EV_SELECT, NULL);
    ret |= fsm_run(&music_player);
    printf("Selecting playlist... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_PLAYLIST_SELECT));

    fsm_dispatch(&music_player, EV_LOW_BATTERY, NULL);
    ret |= fsm_run(&music_player);
    printf("Low battery event... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_LOW_BATTERY));

    fsm_dispatch(&music_player, EV_CHARGE, NULL);
    ret |= fsm_run(&music_player);
    printf("Charging the player... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_PAUSED));

    fsm_dispatch(&music_player, EV_POWER, NULL);
    ret |= fsm_run(&music_player);
    printf("Turning off the player... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_OFF));

    printf("--- End of Complex Music Player Simulation %s ---\n", LOG_CHECK(ret == 0));

    fsm_deinit(&music_player);
    return ret;
}
