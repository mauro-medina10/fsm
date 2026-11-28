#!/usr/bin/env python3
"""Integration tests that compile and run all C examples and verify Mermaid generation."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
CC = os.environ.get("CC", "gcc")
CFLAGS = [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Werror",
    "-g",
    "-O1",
    "-fsanitize=address,undefined",
    f"-I{ROOT / 'include'}",
]
CORE_SRCS = [str(ROOT / "fsm.c"), str(ROOT / "ring_buff.c")]
MERMAID_TOOL = str(ROOT / "tools" / "fsm_2_mermaid.py")


def compile_and_run_example(rel_path: str) -> str:
    src = str(ROOT / rel_path)
    with tempfile.TemporaryDirectory() as tmp:
        exe = str(Path(tmp) / "example_bin")
        cmd = [CC, *CFLAGS, src, *CORE_SRCS, "-o", exe, "-fsanitize=address,undefined"]
        build = subprocess.run(cmd, capture_output=True, text=True)
        if build.returncode != 0:
            raise AssertionError(
                f"Failed to compile {rel_path} with {CC}:\nSTDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
            )
        run = subprocess.run([exe], capture_output=True, text=True, timeout=10)
        if run.returncode != 0:
            raise AssertionError(
                f"Example {rel_path} exited with {run.returncode}:\nSTDOUT:\n{run.stdout}\nSTDERR:\n{run.stderr}"
            )
        return run.stdout


def run_mermaid_on_example(rel_path: str) -> str:
    src = str(ROOT / rel_path)
    proc = subprocess.run(
        [sys.executable, MERMAID_TOOL, "--stdout", src],
        capture_output=True,
        text=True,
        timeout=10,
    )
    if proc.returncode != 0:
        raise AssertionError(
            f"fsm_2_mermaid.py failed on {rel_path}:\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )
    return proc.stdout


class TestExamples(unittest.TestCase):
    def test_1_led_onoff(self):
        out = compile_and_run_example("example/1_led_onoff/fsm_led.c")
        lines = [line.strip() for line in out.splitlines()]
        self.assertEqual(
            [l for l in lines if l.startswith("LED (pin 2)")],
            ["LED (pin 2) OFF", "LED (pin 2) ON", "LED (pin 2) OFF"],
        )
        self.assertIn("End 0", lines)

        md = run_mermaid_on_example("example/1_led_onoff/fsm_led.c")
        self.assertIn("state ST_ROOT {", md)
        self.assertIn("ST_OFF --> ST_ON : EV_ON", md)

    def test_2_led_blinky(self):
        out = compile_and_run_example("example/2_led_blinky/fsm_led.c")
        led_lines = [l.strip() for l in out.splitlines() if l.strip() in ("LED ON", "LED OFF")]
        # Initial OFF -> EV_ON (ON) -> EV_BLINK default ST_BLINK_OFF (OFF) -> 4 toggles over 2000ms
        self.assertEqual(
            led_lines,
            ["LED OFF", "LED ON", "LED OFF", "LED ON", "LED OFF", "LED ON", "LED OFF"],
        )
        self.assertIn("End 0", out)

        md = run_mermaid_on_example("example/2_led_blinky/fsm_led.c")
        self.assertIn("state ST_ON {", md)
        self.assertIn("state ST_BLINK {", md)

    def test_3_button(self):
        out = compile_and_run_example("example/3_button/fsm_button.c")
        # 20ms glitch must be ignored; 60ms press must confirm once and unpress once
        self.assertEqual(out.count("Button pressed (debounced)"), 1)
        self.assertEqual(out.count("Button unpressed"), 1)

        md = run_mermaid_on_example("example/3_button/fsm_button.c")
        self.assertIn("state ST_PRESSED {", md)
        self.assertIn("ST_ANTIBOUNCE --> ST_PRESSED_CONFIRM : FSM_TIMEOUT_EV", md)

    def test_4_timed_event(self):
        out = compile_and_run_example("example/4_timed_event/timed_event_example.c")
        lines = [l.strip() for l in out.splitlines() if l.strip().startswith(("Blinker ", "ON state timeout"))]
        self.assertEqual(
            lines,
            [
                "Blinker OFF",
                "Blinker ON",
                "ON state timeout -> turning OFF",
                "Blinker OFF",
                "Blinker ON",
                "ON state timeout -> turning OFF",
                "Blinker OFF",
            ],
        )

        md = run_mermaid_on_example("example/4_timed_event/timed_event_example.c")
        self.assertIn("ON_ST --> OFF_ST : FSM_TIMEOUT_EV / timeout_work()", md)

    def test_5_music_player(self):
        out = compile_and_run_example("example/others/fsm_music.c")
        self.assertNotIn("ERROR", out)
        self.assertIn("Entering ROOT state", out)
        self.assertIn("Speaker's LED is on", out)
        self.assertIn("Speaker's LED is dancing", out)
        self.assertIn("Speaker's LED is off", out)
        self.assertIn("--- End of Complex Music Player Simulation OK ---", out)

        md = run_mermaid_on_example("example/others/fsm_music.c")
        self.assertIn("state ST_PLAYING {", md)
        self.assertIn("state ST_MENU {", md)


if __name__ == "__main__":
    unittest.main()
