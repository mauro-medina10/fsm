#!/usr/bin/env python3
"""Comprehensive unit tests for tools/fsm_check.py covering every lint rule and CLI mode."""

import importlib.util
import io
import json
from contextlib import redirect_stdout
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
TOOLS_DIR = ROOT / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

spec = importlib.util.spec_from_file_location("fsm_check", TOOLS_DIR / "fsm_check.py")
fsm_check = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(fsm_check)


def codes_for(source: str, config=None):
    return [d.code for d in fsm_check.lint_source(source, file_label="sample.c", config=config)]


class TestFsmCheckErrors(unittest.TestCase):
    def test_e001_duplicate_state(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, EV_1, ST_A)
        FSM_TRANSITIONS_END()
        """
        self.assertIn("E001", codes_for(src))

    def test_e002_e003_e004_hierarchy_refs(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_ROOT, FSM_ST_NONE, ST_MISSING_SUB, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_A, ST_MISSING_PARENT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_B, FSM_ST_NONE, ST_A, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_ROOT, EV_1, ST_B)
        FSM_TRANSITION_CREATE(m, ST_B, EV_2, ST_ROOT)
        FSM_TRANSITIONS_END()
        """
        codes = codes_for(src)
        self.assertIn("E002", codes)
        self.assertIn("E003", codes)
        self.assertIn("E004", codes)

    def test_e005_e006_cycles(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, ST_B, ST_B, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_B, ST_A, ST_A, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, EV_1, ST_B)
        FSM_TRANSITIONS_END()
        """
        codes = codes_for(src)
        self.assertIn("E005", codes)
        self.assertIn("E006", codes)

    def test_e007_max_hierarchy_depth(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_1, FSM_ST_NONE, ST_2, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_2, ST_1, ST_3, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_3, ST_2, ST_4, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_4, ST_3, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_4, EV_1, ST_4)
        FSM_TRANSITIONS_END()
        """
        self.assertNotIn("E007", codes_for(src, fsm_check.LintConfig(max_depth=4)))
        self.assertIn("E007", codes_for(src, fsm_check.LintConfig(max_depth=3)))

    def test_e008_e009_e010_transition_endpoints_and_event(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_NO_SRC, EV_1, ST_A)
        FSM_TRANSITION_CREATE(m, ST_A, FSM_EV_NONE, ST_NO_DST)
        FSM_TRANSITIONS_END()
        """
        codes = codes_for(src)
        self.assertIn("E008", codes)
        self.assertIn("E009", codes)
        self.assertIn("E010", codes)

    def test_e011_unguarded_duplicate_and_w011_guarded_duplicate(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_B, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_C, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, EV_1, ST_B)
        FSM_TRANSITION_CREATE(m, ST_A, EV_1, ST_C)
        FSM_TRANSITION_GUARD_CREATE(m, ST_B, EV_2, ST_A, g1, NULL)
        FSM_TRANSITION_GUARD_CREATE(m, ST_B, EV_2, ST_A, g1, NULL)
        FSM_TRANSITION_GUARD_CREATE(m, ST_B, EV_2, ST_C, g2, NULL)
        FSM_TRANSITION_CREATE(m, ST_C, EV_3, ST_A)
        FSM_TRANSITIONS_END()
        """
        codes = codes_for(src)
        self.assertIn("E011", codes)
        self.assertIn("W011", codes)

    def test_guarded_before_unguarded_fallback_is_allowed(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_B, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_C, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_GUARD_CREATE(m, ST_A, EV_1, ST_B, g_special, NULL)
        FSM_TRANSITION_CREATE(m, ST_A, EV_1, ST_C)
        FSM_TRANSITION_CREATE(m, ST_B, EV_2, ST_A)
        FSM_TRANSITION_CREATE(m, ST_C, EV_2, ST_A)
        FSM_TRANSITIONS_END()
        """
        self.assertEqual(codes_for(src), [])

    def test_e012_empty_transitions(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        """
        self.assertIn("E012", codes_for(src))

    def test_e013_e014_e015_e016_e017_runtime_calls(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_B, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, FSM_TIMEOUT_EV, ST_B)
        FSM_TRANSITION_CREATE(m, ST_B, FSM_TIMEOUT_EV, ST_A)
        FSM_TRANSITIONS_END()
        void run(void) {
            fsm_t obj;
            fsm_init(&obj, FSM_TRANSITIONS_GET(m), FSM_TRANSITIONS_SIZE(m), 4, 1, &FSM_STATE_GET(m, ST_NOPE), NULL);
            fsm_timed_event_set(&obj, &FSM_STATE_GET(m, ST_A), 10);
            fsm_timed_event_set(&obj, &FSM_STATE_GET(m, ST_B), 10);
            fsm_timed_event_set(&obj, &FSM_STATE_GET(m, ST_BAD), 10);
            fsm_actor_link(&obj, a1, 1);
            fsm_actor_link(&obj, a2, 1);
            fsm_dispatch(&obj, FSM_EV_NONE, NULL);
            fsm_dispatch(&obj, EV_UNHANDLED, NULL);
        }
        """
        cfg = fsm_check.LintConfig(max_timed_states=1, max_actors=1)
        codes = codes_for(src, cfg)
        self.assertIn("E013", codes)
        self.assertIn("E014", codes)
        self.assertIn("E015", codes)
        self.assertIn("E016", codes)
        self.assertIn("E017", codes)
        self.assertIn("W012", codes)


class TestFsmCheckWarnings(unittest.TestCase):
    def test_w001_unreachable_and_w002_dead_end_and_w005_unreferenced(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_ROOT, FSM_ST_NONE, ST_A, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_A, ST_ROOT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_DEAD, ST_ROOT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_ISLAND, ST_ROOT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, EV_GO, ST_DEAD)
        FSM_TRANSITIONS_END()
        """
        codes = codes_for(src)
        self.assertIn("W001", codes)  # ST_ISLAND
        self.assertIn("W002", codes)  # ST_DEAD
        self.assertIn("W005", codes)  # ST_ISLAND

    def test_terminating_leaf_is_not_dead_end(self):
        src = """
        static void enter_fatal(fsm_t *self, void *data) {
            (void)data;
            fsm_terminate(self, 99);
        }
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_FATAL, FSM_ST_NONE, FSM_ST_NONE, enter_fatal, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, EV_ERR, ST_FATAL)
        FSM_TRANSITIONS_END()
        void f(void) {
            fsm_t obj;
            fsm_init(&obj, FSM_TRANSITIONS_GET(m), FSM_TRANSITIONS_SIZE(m), 2, 0, &FSM_STATE_GET(m, ST_A), NULL);
        }
        """
        self.assertEqual(codes_for(src), [])

    def test_w003_w004_unused_enums_ignore_sentinels(self):
        src = """
        enum {
            ST_A = FSM_ST_FIRST,
            ST_UNUSED,
            ST_LAST,
        };
        enum {
            EV_1 = FSM_EV_FIRST,
            EV_UNUSED,
            EV_LAST,
            EV_COUNT,
        };
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, EV_1, ST_A)
        FSM_TRANSITIONS_END()
        """
        diags = fsm_check.lint_source(src)
        msgs = [(d.code, d.message) for d in diags]
        self.assertEqual(len(diags), 2)
        self.assertTrue(any(c == "W003" and "ST_UNUSED" in m for c, m in msgs))
        self.assertTrue(any(c == "W004" and "EV_UNUSED" in m for c, m in msgs))

    def test_w006_w007_timers_and_w008_orphan_end_and_w009_composite_run_and_w010_actor(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_ROOT, FSM_ST_NONE, ST_A, NULL, run_never, NULL)
        FSM_CREATE_STATE(m, ST_A, ST_ROOT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_B, ST_ROOT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, EV_GO, ST_B)
        FSM_TRANSITION_CREATE(m, ST_B, FSM_TIMEOUT_EV, ST_A)
        FSM_TRANSITION_CREATE(m, ST_ROOT, FSM_END_EV, ST_A)
        FSM_TRANSITIONS_END()
        FSM_ACTOR_INIT(act)
        FSM_ACTOR_CREATE(ST_GHOST, NULL, NULL, NULL)
        FSM_ACTOR_END()
        void f(void) {
            fsm_t obj;
            fsm_init(&obj, FSM_TRANSITIONS_GET(m), FSM_TRANSITIONS_SIZE(m), 4, 1, &FSM_STATE_GET(m, ST_ROOT), NULL);
            fsm_timed_event_set(&obj, &FSM_STATE_GET(m, ST_A), 50);
        }
        """
        codes = codes_for(src)
        self.assertIn("W006", codes)  # ST_A timed with no timeout transition
        self.assertIn("W007", codes)  # ST_B has FSM_TIMEOUT_EV with no timed_event_set
        self.assertIn("W008", codes)  # ST_ROOT has FSM_END_EV with no FSM_ST_END in subtree
        self.assertIn("W009", codes)  # ST_ROOT has default_substate and run_action
        self.assertIn("W010", codes)  # actor references ST_GHOST

    def test_end_ev_makes_target_reachable(self):
        src = """
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_ROOT, FSM_ST_NONE, ST_COMP, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_COMP, ST_ROOT, ST_SUB, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_SUB, ST_COMP, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(m, ST_AFTER, ST_ROOT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_SUB, EV_DONE, FSM_ST_END)
        FSM_TRANSITION_CREATE(m, ST_COMP, FSM_END_EV, ST_AFTER)
        FSM_TRANSITION_CREATE(m, ST_AFTER, EV_RESTART, ST_COMP)
        FSM_TRANSITIONS_END()
        """
        self.assertEqual(codes_for(src), [])

    def test_inline_ignore_and_cli_formats(self):
        src = """
        enum {
            EV_1 = FSM_EV_FIRST,
            EV_PLACEHOLDER, // fsm-check: ignore W004
            EV_LAST,
        };
        FSM_STATES_INIT(m)
        FSM_CREATE_STATE(m, ST_A, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()
        FSM_TRANSITIONS_INIT(m)
        FSM_TRANSITION_CREATE(m, ST_A, EV_1, ST_A)
        FSM_TRANSITIONS_END()
        """
        self.assertEqual(codes_for(src), [])

        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "warn.c"
            p.write_text(src.replace("// fsm-check: ignore W004", ""), encoding="utf-8")

            buf = io.StringIO()
            with redirect_stdout(buf):
                rc_normal = fsm_check.main([str(p), "--format", "json"])
            self.assertEqual(rc_normal, 0)
            parsed_json = json.loads(buf.getvalue())
            self.assertEqual(len(parsed_json), 1)
            self.assertEqual(parsed_json[0]["code"], "W004")

            buf2 = io.StringIO()
            with redirect_stdout(buf2):
                rc_strict = fsm_check.main([str(p), "--strict", "--format", "github"])
            self.assertEqual(rc_strict, 1)
            self.assertIn("::warning file=", buf2.getvalue())

            buf3 = io.StringIO()
            with redirect_stdout(buf3):
                rc_ignored = fsm_check.main([str(p), "--strict", "--ignore", "W004"])
            self.assertEqual(rc_ignored, 0)

    def test_all_repo_examples_pass_strict(self):
        examples_dir = ROOT / "example"
        paths = sorted(examples_dir.rglob("*.c"))
        self.assertGreaterEqual(len(paths), 7)
        diags = fsm_check.lint_files(paths, fsm_check.LintConfig())
        self.assertEqual(diags, [], msg="\n".join(d.format_text() for d in diags))


if __name__ == "__main__":
    unittest.main()
