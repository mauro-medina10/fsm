#!/usr/bin/env python3
"""Unit tests for tools/fsm_2_mermaid.py."""

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
TOOL_PATH = ROOT / "tools" / "fsm_2_mermaid.py"

spec = importlib.util.spec_from_file_location("fsm_2_mermaid", TOOL_PATH)
fsm_2_mermaid = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(fsm_2_mermaid)


class TestFsm2Mermaid(unittest.TestCase):
    def test_resolve_with_and_without_extension(self):
        with_ext = ROOT / "example" / "1_led_onoff" / "fsm_led.c"
        without_ext = str(with_ext)[:-2]
        self.assertEqual(fsm_2_mermaid.resolve_input_path(str(with_ext)), with_ext)
        self.assertEqual(fsm_2_mermaid.resolve_input_path(without_ext), with_ext)

    def test_ignores_comments_and_handles_guards_work_and_nesting(self):
        sample = """
        // FSM_STATES_INIT(commented_out)
        /* FSM_CREATE_STATE(demo, ST_FAKE, FSM_ST_NONE, FSM_ST_NONE, NULL, NULL, NULL) */
        FSM_STATES_INIT(demo)
        FSM_CREATE_STATE(demo, ST_ROOT, FSM_ST_NONE, ST_PARENT, NULL, NULL, NULL)
        FSM_CREATE_STATE(demo, ST_PARENT, ST_ROOT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(demo, ST_CHILD_A, ST_PARENT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_CREATE_STATE(demo, ST_CHILD_B, ST_PARENT, FSM_ST_NONE, NULL, NULL, NULL)
        FSM_STATES_END()

        FSM_TRANSITIONS_INIT(demo)
        FSM_TRANSITION_GUARD_CREATE(demo,
            ST_CHILD_A, EV_NEXT, ST_CHILD_B, guard_ok, work_fn)
        FSM_TRANSITION_CREATE(demo, ST_CHILD_B, EV_END, FSM_ST_END)
        FSM_TRANSITIONS_END()
        """
        fsms = fsm_2_mermaid.parse_fsms(sample)
        self.assertEqual(list(fsms.keys()), ["demo"])
        states, transitions = fsms["demo"]
        self.assertNotIn("ST_FAKE", states)
        # ST_PARENT has children even though default_substate == FSM_ST_NONE
        self.assertEqual(states["ST_PARENT"].children, ["ST_CHILD_A", "ST_CHILD_B"])

        diagram = fsm_2_mermaid.render_mermaid(states, transitions)
        self.assertIn("state ST_ROOT {", diagram)
        self.assertIn("        state ST_PARENT {", diagram)
        self.assertIn("ST_CHILD_A --> ST_CHILD_B : EV_NEXT [guard_ok] / work_fn()", diagram)
        self.assertIn("ST_CHILD_B --> [*] : EV_END", diagram)

    def test_music_player_example_nests_composite_states(self):
        music_c = ROOT / "example" / "others" / "fsm_music.c"
        fsms = fsm_2_mermaid.parse_fsms(music_c.read_text(encoding="utf-8"))
        self.assertIn("music_player", fsms)
        states, transitions = fsms["music_player"]
        diagram = fsm_2_mermaid.render_mermaid(states, transitions)

        # Nested hierarchy: ST_ROOT -> ST_ON -> ST_PLAYING & ST_MENU (no default_substate!)
        self.assertIn("    state ST_ROOT {", diagram)
        self.assertIn("        state ST_ON {", diagram)
        self.assertIn("            state ST_PLAYING {", diagram)
        self.assertIn("            state ST_MENU {", diagram)
        self.assertIn("ST_NORMAL --> ST_SHUFFLE : EV_MODE_CHANGE", diagram)

    def test_cli_output_dir(self):
        blinky_c = ROOT / "example" / "2_led_blinky" / "fsm_led.c"
        with tempfile.TemporaryDirectory() as tmp:
            rc = fsm_2_mermaid.main([str(blinky_c), "-o", tmp])
            self.assertEqual(rc, 0)
            out_md = Path(tmp) / "led.md"
            self.assertTrue(out_md.is_file())
            content = out_md.read_text(encoding="utf-8")
            self.assertIn("state ST_BLINK {", content)


if __name__ == "__main__":
    unittest.main()
