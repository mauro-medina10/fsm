/**
 * @file test_fsm.c
 * @brief Host unit test suite for ring_buff and fsm components
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsm.h"
#include "ring_buff.h"

/* -------------------------------------------------------------------------- */
/* Minimal test harness                                                       */
/* -------------------------------------------------------------------------- */
static int g_tests_run = 0;
static int g_tests_failed = 0;

#define ASSERT_TRUE(cond)                                                              \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #cond);            \
            g_tests_failed++;                                                          \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define ASSERT_EQ_INT(expected, actual)                                                \
    do {                                                                               \
        int _e = (int)(expected);                                                      \
        int _a = (int)(actual);                                                        \
        if (_e != _a) {                                                                \
            fprintf(stderr, "FAIL %s:%d: expected %d, got %d (%s)\n",                  \
                    __func__, __LINE__, _e, _a, #actual);                              \
            g_tests_failed++;                                                          \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define ASSERT_EQ_STR(expected, actual)                                                \
    do {                                                                               \
        if (strcmp((expected), (actual)) != 0) {                                       \
            fprintf(stderr, "FAIL %s:%d: expected \"%s\", got \"%s\"\n",               \
                    __func__, __LINE__, (expected), (actual));                         \
            g_tests_failed++;                                                          \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define RUN_TEST(fn)                     \
    do {                                 \
        int _before = g_tests_failed;    \
        g_tests_run++;                   \
        fn();                            \
        if (g_tests_failed == _before) { \
            printf("[PASS] %s\n", #fn);  \
        }                                \
    } while (0)

/* -------------------------------------------------------------------------- */
/* Action trace log helper                                                    */
/* -------------------------------------------------------------------------- */
static char g_trace[1024];

static void trace_reset(void)
{
    g_trace[0] = '\0';
}

static void trace_add(const char *entry)
{
    size_t len = strlen(g_trace);
    if (len > 0 && len + 1 < sizeof(g_trace)) {
        g_trace[len++] = ',';
        g_trace[len] = '\0';
    }
    strncat(g_trace, entry, sizeof(g_trace) - len - 1);
}

/* -------------------------------------------------------------------------- */
/* 1. Ring buffer tests                                                       */
/* -------------------------------------------------------------------------- */
static void test_ringbuff_basic_and_wraparound(void)
{
    uint32_t storage[4];
    struct ringbuff rb;

    /* Invalid init parameters */
    ASSERT_EQ_INT(-1, ringbuff_init(NULL, storage, 4, sizeof(uint32_t)));
    ASSERT_EQ_INT(-1, ringbuff_init(&rb, NULL, 4, sizeof(uint32_t)));
    ASSERT_EQ_INT(-1, ringbuff_init(&rb, storage, 1, sizeof(uint32_t)));
    ASSERT_EQ_INT(-1, ringbuff_init(&rb, storage, 4, 0));

    ASSERT_EQ_INT(0, ringbuff_init(&rb, storage, 4, sizeof(uint32_t)));
    ASSERT_EQ_INT(0, (int)ringbuff_num(&rb));

    /* Capacity is len - 1 = 3 elements */
    uint32_t v = 0;
    ASSERT_EQ_INT(-1, ringbuff_get(&rb, &v));

    for (uint32_t i = 1; i <= 3; i++) {
        ASSERT_EQ_INT(0, ringbuff_put(&rb, &i));
        ASSERT_EQ_INT((int)i, (int)ringbuff_num(&rb));
    }

    /* Pop 2 -> p_read advances to slot 2 */
    ASSERT_EQ_INT(0, ringbuff_get(&rb, &v));
    ASSERT_EQ_INT(1, (int)v);
    ASSERT_EQ_INT(0, ringbuff_get(&rb, &v));
    ASSERT_EQ_INT(2, (int)v);
    ASSERT_EQ_INT(1, (int)ringbuff_num(&rb));

    /* Push 2 more -> p_write wraps around below p_read (tests signed diff wrap bug) */
    uint32_t v4 = 4, v5 = 5;
    ASSERT_EQ_INT(0, ringbuff_put(&rb, &v4));
    ASSERT_EQ_INT(2, (int)ringbuff_num(&rb));
    ASSERT_EQ_INT(0, ringbuff_put(&rb, &v5));
    ASSERT_EQ_INT(3, (int)ringbuff_num(&rb));

    /* Overflow overwrites oldest (3), leaving [4, 5, 6] */
    uint32_t v6 = 6;
    ASSERT_EQ_INT(0, ringbuff_put(&rb, &v6));
    ASSERT_EQ_INT(3, (int)ringbuff_num(&rb));

    ASSERT_EQ_INT(0, ringbuff_get(&rb, &v));
    ASSERT_EQ_INT(4, (int)v);
    ASSERT_EQ_INT(0, ringbuff_get(&rb, &v));
    ASSERT_EQ_INT(5, (int)v);
    ASSERT_EQ_INT(0, ringbuff_get(&rb, &v));
    ASSERT_EQ_INT(6, (int)v);
    ASSERT_EQ_INT(0, (int)ringbuff_num(&rb));
    ASSERT_EQ_INT(-1, ringbuff_get(&rb, &v));
}

static void test_ringbuff_put_first_and_full_preservation(void)
{
    uint32_t storage[4]; /* capacity 3 */
    struct ringbuff rb;
    ASSERT_EQ_INT(0, ringbuff_init(&rb, storage, 4, sizeof(uint32_t)));

    uint32_t v1 = 10, v2 = 20, v3 = 30, v_prio = 99, out = 0;
    ASSERT_EQ_INT(0, ringbuff_put(&rb, &v1));
    ASSERT_EQ_INT(0, ringbuff_put(&rb, &v2));
    ASSERT_EQ_INT(0, ringbuff_put_first(&rb, &v_prio));
    ASSERT_EQ_INT(3, (int)ringbuff_num(&rb));

    /* Buffer is now full: [99, 10, 20]. put_first(30) must preserve full count (3) and drop tail (20) */
    ASSERT_EQ_INT(0, ringbuff_put_first(&rb, &v3));
    ASSERT_EQ_INT(3, (int)ringbuff_num(&rb));

    ASSERT_EQ_INT(0, ringbuff_get(&rb, &out));
    ASSERT_EQ_INT(30, (int)out);
    ASSERT_EQ_INT(0, ringbuff_get(&rb, &out));
    ASSERT_EQ_INT(99, (int)out);
    ASSERT_EQ_INT(0, ringbuff_get(&rb, &out));
    ASSERT_EQ_INT(10, (int)out);
    ASSERT_EQ_INT(0, (int)ringbuff_num(&rb));

    /* Flush */
    ASSERT_EQ_INT(0, ringbuff_put(&rb, &v1));
    ASSERT_EQ_INT(0, (int)ringbuff_flush(&rb));
    ASSERT_EQ_INT(0, (int)ringbuff_num(&rb));
}

/* -------------------------------------------------------------------------- */
/* 2. Hierarchical FSM test fixture                                           */
/* -------------------------------------------------------------------------- */
enum {
    H_ROOT = FSM_ST_FIRST,
    H_OFF,
    H_ON,
    H_PAUSED,
    H_PLAYING,
    H_NORMAL,
    H_SHUFFLE,
    H_MENU,
    H_SUBMENU,
};

enum {
    HEV_POWER = FSM_EV_FIRST,
    HEV_PLAY,
    HEV_SHUFFLE,
    HEV_RESTART_PLAY,
    HEV_SELF_OFF,
    HEV_MENU,
    HEV_SUBMENU,
    HEV_BACK,
    HEV_LAST,
};

static void h_ent_root(fsm_t *s, void *d)    { (void)s; (void)d; trace_add("ent:root"); }
static void h_ext_root(fsm_t *s, void *d)    { (void)s; (void)d; trace_add("ext:root"); }
static void h_ent_off(fsm_t *s, void *d)     { (void)s; (void)d; trace_add("ent:off"); }
static void h_ext_off(fsm_t *s, void *d)     { (void)s; (void)d; trace_add("ext:off"); }
static void h_ent_on(fsm_t *s, void *d)      { (void)s; (void)d; trace_add("ent:on"); }
static void h_ext_on(fsm_t *s, void *d)      { (void)s; (void)d; trace_add("ext:on"); }
static void h_ent_paused(fsm_t *s, void *d)  { (void)s; (void)d; trace_add("ent:paused"); }
static void h_ext_paused(fsm_t *s, void *d)  { (void)s; (void)d; trace_add("ext:paused"); }
static void h_ent_playing(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ent:playing"); }
static void h_ext_playing(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ext:playing"); }
static void h_ent_normal(fsm_t *s, void *d)  { (void)s; (void)d; trace_add("ent:normal"); }
static void h_ext_normal(fsm_t *s, void *d)  { (void)s; (void)d; trace_add("ext:normal"); }
static void h_ent_shuffle(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ent:shuffle"); }
static void h_ext_shuffle(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ext:shuffle"); }
static void h_ent_menu(fsm_t *s, void *d)    { (void)s; (void)d; trace_add("ent:menu"); }
static void h_ext_menu(fsm_t *s, void *d)    { (void)s; (void)d; trace_add("ext:menu"); }
static void h_ent_submenu(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ent:submenu"); }
static void h_ext_submenu(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ext:submenu"); }
static void h_work(fsm_t *s, void *d)        { (void)s; (void)d; trace_add("work"); }

FSM_STATES_INIT(hsm)
FSM_CREATE_STATE(hsm, H_ROOT,    FSM_ST_NONE, H_OFF,       h_ent_root,    NULL, h_ext_root)
FSM_CREATE_STATE(hsm, H_OFF,     H_ROOT,      FSM_ST_NONE, h_ent_off,     NULL, h_ext_off)
FSM_CREATE_STATE(hsm, H_ON,      H_ROOT,      H_PAUSED,    h_ent_on,      NULL, h_ext_on)
FSM_CREATE_STATE(hsm, H_PAUSED,  H_ON,        FSM_ST_NONE, h_ent_paused,  NULL, h_ext_paused)
FSM_CREATE_STATE(hsm, H_PLAYING, H_ON,        H_NORMAL,    h_ent_playing, NULL, h_ext_playing)
FSM_CREATE_STATE(hsm, H_NORMAL,  H_PLAYING,   FSM_ST_NONE, h_ent_normal,  NULL, h_ext_normal)
FSM_CREATE_STATE(hsm, H_SHUFFLE, H_PLAYING,   FSM_ST_NONE, h_ent_shuffle, NULL, h_ext_shuffle)
FSM_CREATE_STATE(hsm, H_MENU,    H_ON,        FSM_ST_NONE, h_ent_menu,    NULL, h_ext_menu)
FSM_CREATE_STATE(hsm, H_SUBMENU, H_MENU,      FSM_ST_NONE, h_ent_submenu, NULL, h_ext_submenu)
FSM_STATES_END()

FSM_TRANSITIONS_INIT(hsm)
FSM_TRANSITION_WORK_CREATE(hsm, H_OFF,     HEV_POWER,        H_ON,      h_work)
FSM_TRANSITION_CREATE(hsm,      H_ON,      HEV_POWER,        H_OFF)
FSM_TRANSITION_CREATE(hsm,      H_PAUSED,  HEV_PLAY,         H_PLAYING)
FSM_TRANSITION_CREATE(hsm,      H_NORMAL,  HEV_SHUFFLE,      H_SHUFFLE)
FSM_TRANSITION_WORK_CREATE(hsm, H_PLAYING, HEV_RESTART_PLAY, H_PLAYING, h_work)
FSM_TRANSITION_WORK_CREATE(hsm, H_OFF,     HEV_SELF_OFF,     H_OFF,     h_work)
FSM_TRANSITION_CREATE(hsm,      H_ON,      HEV_MENU,         H_MENU)
FSM_TRANSITION_CREATE(hsm,      H_MENU,    HEV_SUBMENU,      H_SUBMENU)
FSM_TRANSITION_CREATE(hsm,      H_SUBMENU, HEV_BACK,         H_MENU)
FSM_TRANSITIONS_END()

static void test_fsm_init_guards_and_footprint(void)
{
    fsm_t m = {0};
    int ctx = 42;

    /* Compact RAM footprint check (was ~16 KB on 64-bit before optimization) */
    ASSERT_TRUE(sizeof(fsm_t) < 2048U);

    /* Uninitialized / NULL guards */
    ASSERT_EQ_INT(FSM_ST_NONE, fsm_state_get(NULL));
    ASSERT_EQ_INT(FSM_ST_NONE, fsm_state_get(&m));
    ASSERT_EQ_INT(-1, fsm_run(NULL));
    ASSERT_EQ_INT(-1, fsm_run(&m));
    ASSERT_EQ_INT(-1, fsm_has_pending_events(NULL));
    ASSERT_TRUE(fsm_data_get(NULL) == NULL);
    fsm_ticks_hook(NULL);
    fsm_ticks_hook(&m);
    fsm_dispatch(NULL, HEV_POWER, NULL);

    /* Invalid init args */
    ASSERT_EQ_INT(-1, fsm_init(NULL, FSM_TRANSITIONS_GET(hsm), FSM_TRANSITIONS_SIZE(hsm), HEV_LAST, 0, &FSM_STATE_GET(hsm, H_ROOT), NULL));
    ASSERT_EQ_INT(-1, fsm_init(&m, NULL, 1, HEV_LAST, 0, &FSM_STATE_GET(hsm, H_ROOT), NULL));
    ASSERT_EQ_INT(-1, fsm_init(&m, FSM_TRANSITIONS_GET(hsm), FSM_TRANSITIONS_SIZE(hsm), HEV_LAST, 0, NULL, NULL));
    ASSERT_EQ_INT(-2, fsm_init(&m, FSM_TRANSITIONS_GET(hsm), 0, HEV_LAST, 0, &FSM_STATE_GET(hsm, H_ROOT), NULL));

    /* Invalid transition entry caught at init */
    fsm_transition_t bad_tr[1] = { { .source_state = &FSM_STATE_GET(hsm, H_OFF), .event = FSM_EV_NONE, .target_state = &FSM_STATE_GET(hsm, H_ON) } };
    ASSERT_EQ_INT(-3, fsm_init(&m, bad_tr, 1, HEV_LAST, 0, &FSM_STATE_GET(hsm, H_ROOT), NULL));

    /* Valid init + data get/set + deinit */
    trace_reset();
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(hsm), FSM_TRANSITIONS_SIZE(hsm), HEV_LAST, 2, &FSM_STATE_GET(hsm, H_ROOT), &ctx));
    ASSERT_TRUE(fsm_data_get(&m) == &ctx);
    ASSERT_EQ_INT(30, (int)FSM_MS_2_TICKS(&m, 10 + 5)); /* verifies macro parenthesization */

    int ctx2 = 99;
    fsm_data_set(&m, &ctx2);
    ASSERT_TRUE(fsm_data_get(&m) == &ctx2);

    /* Out-of-range unknown event is safely ignored */
    fsm_dispatch(&m, 999999U, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_OFF, fsm_state_get(&m));

    fsm_deinit(&m);
    ASSERT_EQ_INT(FSM_ST_NONE, fsm_state_get(&m));
    ASSERT_EQ_INT(-1, fsm_run(&m));
}

static void test_initial_composite_entry_and_hsm_transitions(void)
{
    fsm_t m;
    trace_reset();

    /* Composite initial state H_ROOT must run ent:root THEN ent:off */
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(hsm), FSM_TRANSITIONS_SIZE(hsm), HEV_LAST, 0, &FSM_STATE_GET(hsm, H_ROOT), NULL));
    ASSERT_EQ_INT(H_OFF, fsm_state_get(&m));
    ASSERT_EQ_STR("ent:root,ent:off", g_trace);

    /* H_OFF -> H_ON (default substate H_PAUSED) with transition work */
    trace_reset();
    fsm_dispatch(&m, HEV_POWER, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_PAUSED, fsm_state_get(&m));
    ASSERT_EQ_STR("ext:off,work,ent:on,ent:paused", g_trace);

    /* H_PAUSED -> H_PLAYING (default substate H_NORMAL) */
    trace_reset();
    fsm_dispatch(&m, HEV_PLAY, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_NORMAL, fsm_state_get(&m));
    ASSERT_EQ_STR("ext:paused,ent:playing,ent:normal", g_trace);

    /* Event bubbling: HEV_POWER handled by ancestor H_ON while in H_NORMAL */
    trace_reset();
    fsm_dispatch(&m, HEV_POWER, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_OFF, fsm_state_get(&m));
    ASSERT_EQ_STR("ext:normal,ext:playing,ext:on,ent:off", g_trace);

    /* Local transition to enclosing parent H_SUBMENU -> H_MENU (no redundant ent:menu) */
    fsm_dispatch(&m, HEV_POWER, NULL);
    fsm_dispatch(&m, HEV_MENU, NULL);
    fsm_dispatch(&m, HEV_SUBMENU, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_SUBMENU, fsm_state_get(&m));

    trace_reset();
    fsm_dispatch(&m, HEV_BACK, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_MENU, fsm_state_get(&m));
    ASSERT_EQ_STR("ext:submenu", g_trace);

    fsm_deinit(&m);
}

static void test_self_transitions_symmetric(void)
{
    fsm_t m;
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(hsm), FSM_TRANSITIONS_SIZE(hsm), HEV_LAST, 0, &FSM_STATE_GET(hsm, H_ROOT), NULL));

    /* Leaf self-transition: H_OFF -> H_OFF must exit, run work, then enter */
    trace_reset();
    fsm_dispatch(&m, HEV_SELF_OFF, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_OFF, fsm_state_get(&m));
    ASSERT_EQ_STR("ext:off,work,ent:off", g_trace);

    /* Composite self-transition: H_PLAYING -> H_PLAYING while in H_SHUFFLE */
    fsm_dispatch(&m, HEV_POWER, NULL);
    fsm_dispatch(&m, HEV_PLAY, NULL);
    fsm_dispatch(&m, HEV_SHUFFLE, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_SHUFFLE, fsm_state_get(&m));

    trace_reset();
    fsm_dispatch(&m, HEV_RESTART_PLAY, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(H_NORMAL, fsm_state_get(&m));
    ASSERT_EQ_STR("ext:shuffle,ext:playing,work,ent:playing,ent:normal", g_trace);

    fsm_deinit(&m);
}

/* -------------------------------------------------------------------------- */
/* 3. End state & chained FSM_END_EV tests                                    */
/* -------------------------------------------------------------------------- */
enum {
    E_ROOT = FSM_ST_FIRST,
    E_GROUP_A,
    E_A_SUB,
    E_GROUP_B,
    E_B_SUB,
    E_DONE,
    E_FALLBACK,
};

enum {
    EEV_FINISH = FSM_EV_FIRST,
    EEV_GOTO_B,
    EEV_FINISH_B,
    EEV_FINISH_DONE,
    EEV_LAST,
};

static void e_ent_root(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ent:root"); }
static void e_ext_root(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ext:root"); }
static void e_ent_ga(fsm_t *s, void *d)   { (void)s; (void)d; trace_add("ent:ga"); }
static void e_ext_ga(fsm_t *s, void *d)   { (void)s; (void)d; trace_add("ext:ga"); }
static void e_ent_asub(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ent:asub"); }
static void e_ext_asub(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ext:asub"); }
static void e_ent_gb(fsm_t *s, void *d)   { (void)s; (void)d; trace_add("ent:gb"); }
static void e_ext_gb(fsm_t *s, void *d)   { (void)s; (void)d; trace_add("ext:gb"); }
static void e_ent_bsub(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ent:bsub"); }
static void e_ext_bsub(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ext:bsub"); }
static void e_ent_done(fsm_t *s, void *d) { (void)s; (void)d; trace_add("ent:done"); }
static void e_ent_fb(fsm_t *s, void *d)   { (void)s; (void)d; trace_add("ent:fb"); }
static void e_work_a(fsm_t *s, void *d)   { (void)s; (void)d; trace_add("work:a"); }
static void e_work_end(fsm_t *s, void *d) { (void)s; (void)d; trace_add("work:end"); }

FSM_STATES_INIT(endfsm)
FSM_CREATE_STATE(endfsm, E_ROOT,     FSM_ST_NONE, E_GROUP_A,   e_ent_root, NULL, e_ext_root)
FSM_CREATE_STATE(endfsm, E_GROUP_A,  E_ROOT,      E_A_SUB,     e_ent_ga,   NULL, e_ext_ga)
FSM_CREATE_STATE(endfsm, E_A_SUB,    E_GROUP_A,   FSM_ST_NONE, e_ent_asub, NULL, e_ext_asub)
FSM_CREATE_STATE(endfsm, E_GROUP_B,  E_ROOT,      E_B_SUB,     e_ent_gb,   NULL, e_ext_gb)
FSM_CREATE_STATE(endfsm, E_B_SUB,    E_GROUP_B,   FSM_ST_NONE, e_ent_bsub, NULL, e_ext_bsub)
FSM_CREATE_STATE(endfsm, E_DONE,     E_ROOT,      FSM_ST_NONE, e_ent_done, NULL, NULL)
FSM_CREATE_STATE(endfsm, E_FALLBACK, E_ROOT,      FSM_ST_NONE, e_ent_fb,   NULL, NULL)
FSM_STATES_END()

/* Put E_ROOT's FSM_END_EV AFTER E_GROUP_A's to specifically catch the old ancestor_depth loop bug */
FSM_TRANSITIONS_INIT(endfsm)
FSM_TRANSITION_WORK_CREATE(endfsm, E_A_SUB,   EEV_FINISH,      FSM_ST_END, e_work_a)
FSM_TRANSITION_WORK_CREATE(endfsm, E_GROUP_A, FSM_END_EV,      E_DONE,     e_work_end)
FSM_TRANSITION_CREATE(endfsm,      E_ROOT,    FSM_END_EV,      E_FALLBACK)
/* Also put a duplicate parent handler for EEV_FINISH to ensure FSM_ST_END sets HANDLED and doesn't bubble */
FSM_TRANSITION_CREATE(endfsm,      E_ROOT,    EEV_FINISH,      E_FALLBACK)
FSM_TRANSITION_CREATE(endfsm,      E_DONE,    EEV_GOTO_B,      E_GROUP_B)
FSM_TRANSITION_CREATE(endfsm,      E_B_SUB,   EEV_FINISH_B,    FSM_ST_END)
FSM_TRANSITION_CREATE(endfsm,      E_GROUP_B, FSM_END_EV,      FSM_ST_END) /* chained completion to E_ROOT */
FSM_TRANSITION_CREATE(endfsm,      E_DONE,    EEV_FINISH_DONE, FSM_ST_END) /* direct to E_ROOT */
FSM_TRANSITIONS_END()

static void test_end_state_closest_root_and_chaining(void)
{
    fsm_t m;
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(endfsm), FSM_TRANSITIONS_SIZE(endfsm), EEV_LAST, 0, &FSM_STATE_GET(endfsm, E_ROOT), NULL));
    ASSERT_EQ_INT(E_A_SUB, fsm_state_get(&m));

    /* 1. Closest ancestor E_GROUP_A (not E_ROOT) handles FSM_END_EV; exits E_A_SUB and E_GROUP_A, does NOT re-enter E_ROOT, does NOT bubble EEV_FINISH to E_ROOT */
    trace_reset();
    fsm_dispatch(&m, EEV_FINISH, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(E_DONE, fsm_state_get(&m));
    ASSERT_EQ_STR("work:a,ext:asub,ext:ga,work:end,ent:done", g_trace);

    /* 2. Direct root-ancestor FSM_END_EV: E_DONE -> FSM_ST_END -> E_ROOT(FSM_END_EV->E_FALLBACK) */
    trace_reset();
    fsm_dispatch(&m, EEV_FINISH_DONE, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(E_FALLBACK, fsm_state_get(&m));
    ASSERT_EQ_STR("ent:fb", g_trace);

    /* 3. Chained FSM_ST_END: E_B_SUB -> FSM_ST_END -> E_GROUP_B(FSM_END_EV->FSM_ST_END) -> E_ROOT(FSM_END_EV->E_FALLBACK) */
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(endfsm), FSM_TRANSITIONS_SIZE(endfsm), EEV_LAST, 0, &FSM_STATE_GET(endfsm, E_ROOT), NULL));
    fsm_dispatch(&m, EEV_FINISH, NULL);
    fsm_dispatch(&m, EEV_GOTO_B, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(E_B_SUB, fsm_state_get(&m));

    trace_reset();
    fsm_dispatch(&m, EEV_FINISH_B, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(E_FALLBACK, fsm_state_get(&m));
    /* E_FALLBACK is a child of E_ROOT, so LCA is E_ROOT (no redundant ext:root/ent:root) */
    ASSERT_EQ_STR("ext:bsub,ext:gb,ent:fb", g_trace);

    fsm_deinit(&m);
}

/* Unhandled FSM_ST_END terminates cleanly */
enum { U_A = FSM_ST_FIRST, U_B };
enum { UEV_END = FSM_EV_FIRST, UEV_LAST };

FSM_STATES_INIT(unh)
FSM_CREATE_STATE(unh, U_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
FSM_CREATE_STATE(unh, U_B, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
FSM_STATES_END()

FSM_TRANSITIONS_INIT(unh)
FSM_TRANSITION_CREATE(unh, U_A, UEV_END, FSM_ST_END)
FSM_TRANSITIONS_END()

static void test_end_state_unhandled_terminates(void)
{
    fsm_t m;
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(unh), FSM_TRANSITIONS_SIZE(unh), UEV_LAST, 0, &FSM_STATE_GET(unh, U_A), NULL));
    fsm_dispatch(&m, UEV_END, NULL);
    ASSERT_EQ_INT(FSM_END_EV, fsm_run(&m));
    /* Subsequent runs remain terminated */
    ASSERT_EQ_INT(FSM_END_EV, fsm_run(&m));
    fsm_deinit(&m);
}

/* -------------------------------------------------------------------------- */
/* 4. Guard condition tests                                                   */
/* -------------------------------------------------------------------------- */
enum { G_ROOT = FSM_ST_FIRST, G_IDLE, G_HIGH, G_LOW, G_PARENT_HANDLED };
enum { GEV_GO = FSM_EV_FIRST, GEV_LAST };

static bool guard_gt_10(fsm_t *s, void *d) { (void)s; return (d != NULL) && (*(int *)d > 10); }
static bool guard_le_10(fsm_t *s, void *d) { (void)s; return (d != NULL) && (*(int *)d > 0) && (*(int *)d <= 10); }

FSM_STATES_INIT(grdfsm)
FSM_CREATE_STATE(grdfsm, G_ROOT,           FSM_ST_NONE, G_IDLE,      NULL, NULL, NULL)
FSM_CREATE_STATE(grdfsm, G_IDLE,           G_ROOT,      FSM_ST_NONE, NULL, NULL, NULL)
FSM_CREATE_STATE(grdfsm, G_HIGH,           G_ROOT,      FSM_ST_NONE, NULL, NULL, NULL)
FSM_CREATE_STATE(grdfsm, G_LOW,            G_ROOT,      FSM_ST_NONE, NULL, NULL, NULL)
FSM_CREATE_STATE(grdfsm, G_PARENT_HANDLED, G_ROOT,      FSM_ST_NONE, NULL, NULL, NULL)
FSM_STATES_END()

FSM_TRANSITIONS_INIT(grdfsm)
FSM_TRANSITION_GUARD_CREATE(grdfsm, G_IDLE, GEV_GO, G_HIGH, guard_gt_10, NULL)
FSM_TRANSITION_GUARD_CREATE(grdfsm, G_IDLE, GEV_GO, G_LOW,  guard_le_10, NULL)
FSM_TRANSITION_CREATE(grdfsm,       G_ROOT, GEV_GO, G_PARENT_HANDLED)
FSM_TRANSITIONS_END()

static void test_transition_guards(void)
{
    fsm_t m;
    int v_high = 25, v_low = 5, v_zero = 0;

    /* Guard 1 matches */
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(grdfsm), FSM_TRANSITIONS_SIZE(grdfsm), GEV_LAST, 0, &FSM_STATE_GET(grdfsm, G_ROOT), NULL));
    fsm_dispatch(&m, GEV_GO, &v_high);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(G_HIGH, fsm_state_get(&m));

    /* Guard 2 matches when guard 1 fails */
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(grdfsm), FSM_TRANSITIONS_SIZE(grdfsm), GEV_LAST, 0, &FSM_STATE_GET(grdfsm, G_ROOT), NULL));
    fsm_dispatch(&m, GEV_GO, &v_low);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(G_LOW, fsm_state_get(&m));

    /* Both child guards fail -> bubbles to G_ROOT */
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(grdfsm), FSM_TRANSITIONS_SIZE(grdfsm), GEV_LAST, 0, &FSM_STATE_GET(grdfsm, G_ROOT), NULL));
    fsm_dispatch(&m, GEV_GO, &v_zero);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(G_PARENT_HANDLED, fsm_state_get(&m));

    fsm_deinit(&m);
}

/* -------------------------------------------------------------------------- */
/* 5. Actor tests                                                             */
/* -------------------------------------------------------------------------- */
static void act1_ent_on(fsm_t *s, void *d)     { (void)s; (void)d; trace_add("a1:ent:on"); }
static void act1_ext_on(fsm_t *s, void *d)     { (void)s; (void)d; trace_add("a1:ext:on"); }
static void act1_run_paused(fsm_t *s, void *d) { (void)s; (void)d; trace_add("a1:run:paused"); }
static void act2_ent_paused(fsm_t *s, void *d) { (void)s; (void)d; trace_add("a2:ent:paused"); }

FSM_ACTOR_INIT(actor1)
FSM_ACTOR_CREATE(H_ON,     act1_ent_on, NULL,           act1_ext_on)
FSM_ACTOR_CREATE(H_PAUSED, NULL,        act1_run_paused, NULL)
FSM_ACTOR_END()

FSM_ACTOR_INIT(actor2)
FSM_ACTOR_CREATE(H_PAUSED, act2_ent_paused, NULL, NULL)
FSM_ACTOR_END()

static void test_actors_hierarchy_and_limits(void)
{
    fsm_t m;
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(hsm), FSM_TRANSITIONS_SIZE(hsm), HEV_LAST, 0, &FSM_STATE_GET(hsm, H_ROOT), NULL));

    /* Validation */
    ASSERT_EQ_INT(-1, fsm_actor_link(NULL, FSM_ACTOR_GET(actor1), FSM_ACTOR_SIZE(actor1)));
    ASSERT_EQ_INT(-1, fsm_actor_link(&m, NULL, 1));
    ASSERT_EQ_INT(-1, fsm_actor_link(&m, FSM_ACTOR_GET(actor1), 0));

    ASSERT_EQ_INT(0, fsm_actor_link(&m, FSM_ACTOR_GET(actor1), FSM_ACTOR_SIZE(actor1)));
    ASSERT_EQ_INT(0, fsm_actor_link(&m, FSM_ACTOR_GET(actor2), FSM_ACTOR_SIZE(actor2)));

    trace_reset();
    fsm_dispatch(&m, HEV_POWER, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_STR("ext:off,work,ent:on,a1:ent:on,ent:paused,a2:ent:paused,a1:run:paused", g_trace);

    trace_reset();
    fsm_dispatch(&m, HEV_POWER, NULL);
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_STR("ext:paused,ext:on,a1:ext:on,ent:off", g_trace);

    /* Fill remaining actor slots up to FSM_MAX_ACTORS, then expect -2 */
    for (size_t i = 2; i < FSM_MAX_ACTORS; i++) {
        ASSERT_EQ_INT(0, fsm_actor_link(&m, FSM_ACTOR_GET(actor2), FSM_ACTOR_SIZE(actor2)));
    }
    ASSERT_EQ_INT(-2, fsm_actor_link(&m, FSM_ACTOR_GET(actor2), FSM_ACTOR_SIZE(actor2)));

    fsm_deinit(&m);
}

/* -------------------------------------------------------------------------- */
/* 6. Per-instance timer & multi-instance tests                               */
/* -------------------------------------------------------------------------- */
enum { T_ROOT = FSM_ST_FIRST, T_OFF, T_ON, T_GROUP, T_SUB };
enum { TEV_GROUP = FSM_EV_FIRST, TEV_LAST };

FSM_STATES_INIT(tmrfsm)
FSM_CREATE_STATE(tmrfsm, T_ROOT,  FSM_ST_NONE, T_OFF,       NULL, NULL, NULL)
FSM_CREATE_STATE(tmrfsm, T_OFF,   T_ROOT,      FSM_ST_NONE, NULL, NULL, NULL)
FSM_CREATE_STATE(tmrfsm, T_ON,    T_ROOT,      FSM_ST_NONE, NULL, NULL, NULL)
FSM_CREATE_STATE(tmrfsm, T_GROUP, T_ROOT,      T_SUB,       NULL, NULL, NULL)
FSM_CREATE_STATE(tmrfsm, T_SUB,   T_GROUP,     FSM_ST_NONE, NULL, NULL, NULL)
FSM_STATES_END()

FSM_TRANSITIONS_INIT(tmrfsm)
FSM_TRANSITION_CREATE(tmrfsm, T_OFF,   FSM_TIMEOUT_EV, T_ON)
FSM_TRANSITION_CREATE(tmrfsm, T_ON,    FSM_TIMEOUT_EV, T_OFF)
FSM_TRANSITION_CREATE(tmrfsm, T_OFF,   TEV_GROUP,      T_GROUP)
FSM_TRANSITION_CREATE(tmrfsm, T_GROUP, FSM_TIMEOUT_EV, T_OFF)
FSM_TRANSITIONS_END()

static void test_per_instance_timers_and_multi_instance(void)
{
    fsm_t fast, slow;
    ASSERT_EQ_INT(0, fsm_init(&fast, FSM_TRANSITIONS_GET(tmrfsm), FSM_TRANSITIONS_SIZE(tmrfsm), TEV_LAST, 1, &FSM_STATE_GET(tmrfsm, T_ROOT), NULL));
    ASSERT_EQ_INT(0, fsm_init(&slow, FSM_TRANSITIONS_GET(tmrfsm), FSM_TRANSITIONS_SIZE(tmrfsm), TEV_LAST, 1, &FSM_STATE_GET(tmrfsm, T_ROOT), NULL));

    /* Setting timer AFTER fsm_init must immediately arm currently active state (T_OFF) */
    ASSERT_EQ_INT(0, fsm_timed_event_set(&fast, &FSM_STATE_GET(tmrfsm, T_OFF), 2));
    ASSERT_EQ_INT(0, fsm_timed_event_set(&fast, &FSM_STATE_GET(tmrfsm, T_ON),  2));

    /* Different periods on second instance sharing the SAME const tables */
    ASSERT_EQ_INT(0, fsm_timed_event_set(&slow, &FSM_STATE_GET(tmrfsm, T_OFF), 5));
    ASSERT_EQ_INT(0, fsm_timed_event_set(&slow, &FSM_STATE_GET(tmrfsm, T_ON),  5));

    /* Tick 1: neither expires yet */
    fsm_ticks_hook(&fast);
    fsm_ticks_hook(&slow);
    /* Updating inactive T_ON period on slow must NOT reset active T_OFF countdown */
    ASSERT_EQ_INT(0, fsm_timed_event_set(&slow, &FSM_STATE_GET(tmrfsm, T_ON), 4));
    ASSERT_EQ_INT(0, fsm_run(&fast));
    ASSERT_EQ_INT(0, fsm_run(&slow));
    ASSERT_EQ_INT(T_OFF, fsm_state_get(&fast));
    ASSERT_EQ_INT(T_OFF, fsm_state_get(&slow));

    /* Tick 2: fast toggles to T_ON; slow still in T_OFF */
    fsm_ticks_hook(&fast);
    fsm_ticks_hook(&slow);
    ASSERT_EQ_INT(0, fsm_run(&fast));
    ASSERT_EQ_INT(0, fsm_run(&slow));
    ASSERT_EQ_INT(T_ON,  fsm_state_get(&fast));
    ASSERT_EQ_INT(T_OFF, fsm_state_get(&slow));

    /* Disarming active state with ticks = 0 stops timeouts */
    ASSERT_EQ_INT(0, fsm_timed_event_set(&fast, &FSM_STATE_GET(tmrfsm, T_ON), 0));
    for (int i = 0; i < 10; i++) {
        fsm_ticks_hook(&fast);
        ASSERT_EQ_INT(0, fsm_run(&fast));
    }
    ASSERT_EQ_INT(T_ON, fsm_state_get(&fast));

    /* Ticks 3, 4, 5: slow toggles at tick 5 */
    for (int i = 0; i < 3; i++) {
        fsm_ticks_hook(&slow);
        ASSERT_EQ_INT(0, fsm_run(&slow));
    }
    ASSERT_EQ_INT(T_ON, fsm_state_get(&slow));

    /* Hierarchical timer on parent state T_GROUP fires while in T_SUB */
    ASSERT_EQ_INT(0, fsm_init(&fast, FSM_TRANSITIONS_GET(tmrfsm), FSM_TRANSITIONS_SIZE(tmrfsm), TEV_LAST, 1, &FSM_STATE_GET(tmrfsm, T_ROOT), NULL));
    ASSERT_EQ_INT(0, fsm_timed_event_set(&fast, &FSM_STATE_GET(tmrfsm, T_GROUP), 3));
    fsm_dispatch(&fast, TEV_GROUP, NULL);
    ASSERT_EQ_INT(0, fsm_run(&fast));
    ASSERT_EQ_INT(T_SUB, fsm_state_get(&fast));

    for (int i = 0; i < 3; i++) {
        fsm_ticks_hook(&fast);
    }
    ASSERT_EQ_INT(0, fsm_run(&fast));
    ASSERT_EQ_INT(T_OFF, fsm_state_get(&fast));

    fsm_deinit(&fast);
    fsm_deinit(&slow);
}

/* -------------------------------------------------------------------------- */
/* 7. Termination & re-dispatch tests                                         */
/* -------------------------------------------------------------------------- */
enum { X_A = FSM_ST_FIRST, X_B, X_C };
enum { XEV_TO_B = FSM_EV_FIRST, XEV_TO_C, XEV_LAST };

static int g_term_phase = 0; /* 1=exit, 2=work, 3=entry, 4=run */

static void x_ext_a(fsm_t *s, void *d)
{
    (void)d;
    trace_add("ext:a");
    if (g_term_phase == 1) fsm_terminate(s, 101);
}
static void x_work_ab(fsm_t *s, void *d)
{
    (void)d;
    trace_add("work:ab");
    if (g_term_phase == 2) fsm_terminate(s, 102);
}
static void x_ent_b(fsm_t *s, void *d)
{
    (void)d;
    trace_add("ent:b");
    if (g_term_phase == 3) fsm_terminate(s, 103);
    if (g_term_phase == 5) fsm_dispatch(s, XEV_TO_C, NULL);
}
static void x_run_b(fsm_t *s, void *d)
{
    (void)d;
    trace_add("run:b");
    if (g_term_phase == 4) fsm_terminate(s, 104);
}
static void x_ent_c(fsm_t *s, void *d)
{
    (void)s;
    (void)d;
    trace_add("ent:c");
}

FSM_STATES_INIT(termfsm)
FSM_CREATE_STATE(termfsm, X_A, FSM_ST_NONE, FSM_ST_NONE, NULL,    NULL,    x_ext_a)
FSM_CREATE_STATE(termfsm, X_B, FSM_ST_NONE, FSM_ST_NONE, x_ent_b, x_run_b, NULL)
FSM_CREATE_STATE(termfsm, X_C, FSM_ST_NONE, FSM_ST_NONE, x_ent_c, NULL,    NULL)
FSM_STATES_END()

FSM_TRANSITIONS_INIT(termfsm)
FSM_TRANSITION_WORK_CREATE(termfsm, X_A, XEV_TO_B, X_B, x_work_ab)
FSM_TRANSITION_CREATE(termfsm,      X_B, XEV_TO_C, X_C)
FSM_TRANSITIONS_END()

static void test_termination_in_every_phase_and_redispatch(void)
{
    fsm_t m;

    /* 1. Terminate in exit_action -> skips work, enter, run */
    g_term_phase = 1;
    trace_reset();
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(termfsm), FSM_TRANSITIONS_SIZE(termfsm), XEV_LAST, 0, &FSM_STATE_GET(termfsm, X_A), NULL));
    fsm_dispatch(&m, XEV_TO_B, NULL);
    ASSERT_EQ_INT(101, fsm_run(&m));
    ASSERT_EQ_STR("ext:a", g_trace);

    /* 2. Terminate in transition_action -> skips enter, run */
    g_term_phase = 2;
    trace_reset();
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(termfsm), FSM_TRANSITIONS_SIZE(termfsm), XEV_LAST, 0, &FSM_STATE_GET(termfsm, X_A), NULL));
    fsm_dispatch(&m, XEV_TO_B, NULL);
    ASSERT_EQ_INT(102, fsm_run(&m));
    ASSERT_EQ_STR("ext:a,work:ab", g_trace);

    /* 3. Terminate in entry_action -> skips run */
    g_term_phase = 3;
    trace_reset();
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(termfsm), FSM_TRANSITIONS_SIZE(termfsm), XEV_LAST, 0, &FSM_STATE_GET(termfsm, X_A), NULL));
    fsm_dispatch(&m, XEV_TO_B, NULL);
    ASSERT_EQ_INT(103, fsm_run(&m));
    ASSERT_EQ_STR("ext:a,work:ab,ent:b", g_trace);

    /* 4. Terminate in run_action -> returns terminate_val immediately */
    g_term_phase = 4;
    trace_reset();
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(termfsm), FSM_TRANSITIONS_SIZE(termfsm), XEV_LAST, 0, &FSM_STATE_GET(termfsm, X_A), NULL));
    fsm_dispatch(&m, XEV_TO_B, NULL);
    ASSERT_EQ_INT(104, fsm_run(&m));
    ASSERT_EQ_STR("ext:a,work:ab,ent:b,run:b", g_trace);

    /* 5. Re-dispatch from entry_action runs to completion in same fsm_run() */
    g_term_phase = 5;
    trace_reset();
    ASSERT_EQ_INT(0, fsm_init(&m, FSM_TRANSITIONS_GET(termfsm), FSM_TRANSITIONS_SIZE(termfsm), XEV_LAST, 0, &FSM_STATE_GET(termfsm, X_A), NULL));
    fsm_dispatch(&m, XEV_TO_B, NULL);
    ASSERT_EQ_INT(1, fsm_has_pending_events(&m));
    ASSERT_EQ_INT(0, fsm_run(&m));
    ASSERT_EQ_INT(0, fsm_has_pending_events(&m));
    ASSERT_EQ_INT(X_C, fsm_state_get(&m));
    ASSERT_EQ_STR("ext:a,work:ab,ent:b,ent:c", g_trace);

    /* 6. Flush discards queued events */
    fsm_dispatch(&m, XEV_TO_B, NULL);
    ASSERT_EQ_INT(1, fsm_has_pending_events(&m));
    fsm_flush_events(&m);
    ASSERT_EQ_INT(0, fsm_has_pending_events(&m));

    fsm_deinit(&m);
}

int main(void)
{
    RUN_TEST(test_ringbuff_basic_and_wraparound);
    RUN_TEST(test_ringbuff_put_first_and_full_preservation);
    RUN_TEST(test_fsm_init_guards_and_footprint);
    RUN_TEST(test_initial_composite_entry_and_hsm_transitions);
    RUN_TEST(test_self_transitions_symmetric);
    RUN_TEST(test_end_state_closest_root_and_chaining);
    RUN_TEST(test_end_state_unhandled_terminates);
    RUN_TEST(test_transition_guards);
    RUN_TEST(test_actors_hierarchy_and_limits);
    RUN_TEST(test_per_instance_timers_and_multi_instance);
    RUN_TEST(test_termination_in_every_phase_and_redispatch);

    printf("\nSummary: %d tests run, %d failed (sizeof(fsm_t) = %zu bytes)\n",
           g_tests_run, g_tests_failed, sizeof(fsm_t));

    return (g_tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
