/**
 * @file fsm_music.c
 * @author Mauro Medina 
 * @brief 
 * @version 1.0.1
 * @date 2024-07-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

#ifndef LOG_CHECK
#define LOG_CHECK(val) (val == 1 ? "OK" : "ERROR")
#endif

// Define states
enum {
    ST_ROOT = FSM_ST_FIRST,            // 1 
    ST_OFF,                 // 2
    ST_ON,                  // 3
    ST_PLAYING,             // 4
    ST_NORMAL,              // 5
    ST_SHUFFLE,             // 6
    ST_REPEAT,              // 7
    ST_PAUSED,              // 8
    ST_MENU,                // 9
    ST_VOLUME_ADJUST,       // 10
    ST_PLAYLIST_SELECT,     // 11
    ST_LOW_BATTERY          // 12
};

// Define events
enum {
    EV_POWER = FSM_EV_FIRST,       // 0
    EV_PLAY,            // 1
    EV_PAUSE,           // 2
    EV_STOP,            // 3
    EV_NEXT,            // 4
    EV_PREV,            // 5
    EV_MODE_CHANGE,     // 6
    EV_MENU,            // 7
    EV_VOLUME_UP,       // 8
    EV_VOLUME_DOWN,     // 9
    EV_SELECT,          // 10
    EV_BACK,            // 11
    EV_LOW_BATTERY,     // 12
    EV_CHARGE           // 13
};

// Action function prototypes
static void enter_root(fsm_t *self, void* data);
static void enter_off(fsm_t *self, void* data);
static void enter_on(fsm_t *self, void* data);
static void enter_playing(fsm_t *self, void* data);
static void enter_normal(fsm_t *self, void* data);
static void enter_shuffle(fsm_t *self, void* data);
static void enter_repeat(fsm_t *self, void* data);
static void enter_paused(fsm_t *self, void* data);
static void enter_menu(fsm_t *self, void* data);
static void enter_volume_adjust(fsm_t *self, void* data);
static void enter_playlist_select(fsm_t *self, void* data);
static void enter_low_battery(fsm_t *self, void* data);

static void run_root(fsm_t* self, void* data);
static void run_off(fsm_t* self, void* data);
static void run_on(fsm_t* self, void* data);
static void run_playing(fsm_t* self, void* data);
static void run_normal(fsm_t* self, void* data);
static void run_shuffle(fsm_t* self, void* data);
static void run_repeat(fsm_t* self, void* data);
static void run_paused(fsm_t* self, void* data);
static void run_menu(fsm_t* self, void* data);
static void run_volume_adjust(fsm_t* self, void* data);
static void run_playlist_select(fsm_t* self, void* data);
static void run_low_battery(fsm_t* self, void* data);

// Define FSM states
FSM_STATES_INIT(music_player)
//                  name        state id          parent           sub          entry                 run                   exit
FSM_CREATE_STATE(music_player, ST_ROOT,             FSM_ST_NONE, ST_OFF,      enter_root,            run_root,              NULL)
FSM_CREATE_STATE(music_player, ST_OFF,              ST_ROOT,     FSM_ST_NONE, enter_off,             run_off,               NULL)
FSM_CREATE_STATE(music_player, ST_ON,               ST_ROOT,     ST_PAUSED,   enter_on,              run_on,                NULL)
FSM_CREATE_STATE(music_player, ST_PLAYING,          ST_ON,       ST_NORMAL,   enter_playing,         run_playing,           NULL)
FSM_CREATE_STATE(music_player, ST_NORMAL,           ST_PLAYING,  FSM_ST_NONE, enter_normal,          run_normal,            NULL)
FSM_CREATE_STATE(music_player, ST_SHUFFLE,          ST_PLAYING,  FSM_ST_NONE, enter_shuffle,         run_shuffle,           NULL)
FSM_CREATE_STATE(music_player, ST_REPEAT,           ST_PLAYING,  FSM_ST_NONE, enter_repeat,          run_repeat,            NULL)
FSM_CREATE_STATE(music_player, ST_PAUSED,           ST_ON,       FSM_ST_NONE, enter_paused,          run_paused,            NULL)
FSM_CREATE_STATE(music_player, ST_MENU,             ST_ON,       FSM_ST_NONE, enter_menu,            run_menu,              NULL)
FSM_CREATE_STATE(music_player, ST_VOLUME_ADJUST,    ST_MENU,     FSM_ST_NONE, enter_volume_adjust,   run_volume_adjust,     NULL)
FSM_CREATE_STATE(music_player, ST_PLAYLIST_SELECT,  ST_MENU,     FSM_ST_NONE, enter_playlist_select, run_playlist_select,   NULL)
FSM_CREATE_STATE(music_player, ST_LOW_BATTERY,      ST_ROOT,     FSM_ST_NONE, enter_low_battery,     run_low_battery,       NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(music_player)
//                      fsm name    State source        event       state target
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

// Action function implementations
static void enter_root(fsm_t *self, void* data) { print("Entering ROOT state\n"); }
static void enter_off(fsm_t *self, void* data) { print("Entering OFF state\n"); }
static void enter_on(fsm_t *self, void* data) { print("Entering ON state\n"); }
static void enter_playing(fsm_t *self, void* data) { print("Entering PLAYING state\n"); }
static void enter_normal(fsm_t *self, void* data) { print("Entering NORMAL play state\n"); }
static void enter_shuffle(fsm_t *self, void* data) { print("Entering SHUFFLE play state\n"); }
static void enter_repeat(fsm_t *self, void* data) { print("Entering REPEAT play state\n"); }
static void enter_paused(fsm_t *self, void* data) { print("Entering PAUSED state\n"); }
static void enter_menu(fsm_t *self, void* data) { print("Entering MENU state\n"); }
static void enter_volume_adjust(fsm_t *self, void* data) { print("Entering VOLUME ADJUST state\n"); }
static void enter_playlist_select(fsm_t *self, void* data) { print("Entering PLAYLIST SELECT state\n"); }
static void enter_low_battery(fsm_t *self, void* data) { print("Entering LOW BATTERY state\n"); }

static void run_root(fsm_t* self, void* data) { print("Running ROOT state\n"); }
static void run_off(fsm_t* self, void* data) { print("Music player is OFF\n"); }
static void run_on(fsm_t* self, void* data) { print("Music player is ON\n"); }
static void run_playing(fsm_t* self, void* data) { print("Music is playing\n"); }
static void run_normal(fsm_t* self, void* data) { print("Playing in NORMAL mode\n"); }
static void run_shuffle(fsm_t* self, void* data) { print("Playing in SHUFFLE mode\n"); }
static void run_repeat(fsm_t* self, void* data) { print("Playing in REPEAT mode\n"); }
static void run_paused(fsm_t* self, void* data) { print("Music is PAUSED\n"); }
static void run_menu(fsm_t* self, void* data) { print("In MENU\n"); }
static void run_volume_adjust(fsm_t* self, void* data) { print("Adjusting VOLUME\n"); }
static void run_playlist_select(fsm_t* self, void* data) { print("Selecting PLAYLIST\n"); }
static void run_low_battery(fsm_t* self, void* data) { print("LOW BATTERY warning\n"); }


int main() {
    fsm_t music_player;
    int ret = 0;

    // Simulate music player actions
    print("--- Starting Complex Music Player Simulation ---\n");

    fsm_init(&music_player, FSM_TRANSITIONS_GET(music_player), FSM_TRANSITIONS_SIZE(music_player),
             &FSM_STATE_GET(music_player, ST_ROOT), NULL);

    ret |= fsm_run(&music_player);  // Should be in OFF state (default substate of ROOT)

    fsm_dispatch(&music_player, EV_POWER, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PAUSED state
    print("Turning on the player... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_PAUSED));
    
    fsm_dispatch(&music_player, EV_PLAY, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PLAYING -> NORMAL state
    print("Starting playback... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_NORMAL));
    
    fsm_dispatch(&music_player, EV_MODE_CHANGE, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PLAYING -> SHUFFLE state
    print("Changing play mode... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_SHUFFLE));
    
    fsm_dispatch(&music_player, EV_MENU, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU state
    print("Opening menu... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_MENU));
    
    fsm_dispatch(&music_player, EV_VOLUME_UP, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU -> VOLUME_ADJUST state
    print("Adjusting volume... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_VOLUME_ADJUST));

    fsm_dispatch(&music_player, EV_BACK, NULL);
    ret |= fsm_run(&music_player);  // Should be back in ON -> MENU state
    print("Going back to menu... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_MENU));
    
    fsm_dispatch(&music_player, EV_SELECT, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU -> PLAYLIST_SELECT state
    print("Selecting playlist... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_PLAYLIST_SELECT));
    
    fsm_dispatch(&music_player, EV_LOW_BATTERY, NULL);
    ret |= fsm_run(&music_player);  // Should transition to LOW_BATTERY state
    print("Low battery event... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_LOW_BATTERY));
    
    fsm_dispatch(&music_player, EV_CHARGE, NULL);
    ret |= fsm_run(&music_player);  // Should transition back to ON state
    print("Charging the player... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_PAUSED));
    
    fsm_dispatch(&music_player, EV_POWER, NULL);
    ret |= fsm_run(&music_player);  // Should be in OFF state
    print("Turning off the player... %s\n", LOG_CHECK(fsm_state_get(&music_player) == ST_OFF));
    
    print("--- End of Complex Music Player Simulation %s---\n", LOG_CHECK(ret == 0));

    while (1);

    return ret;
}