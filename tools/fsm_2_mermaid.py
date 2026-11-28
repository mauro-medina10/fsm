#!/usr/bin/env python3
"""Command-line Mermaid stateDiagram-v2 generator for the C FSM library.

Parses C files using the FSM library macros:
    * FSM_STATES_INIT
    * FSM_CREATE_STATE
    * FSM_TRANSITIONS_INIT
    * FSM_TRANSITION_CREATE
    * FSM_TRANSITION_WORK_CREATE
    * FSM_TRANSITION_GUARD_CREATE

Usage:
    python3 tools/fsm_2_mermaid.py path/to/fsm_file.c [-o OUTPUT_DIR] [--stdout]
"""

import argparse
from pathlib import Path
import sys
from typing import Dict, List, Optional, Tuple

# Ensure sibling tools/ modules can be imported whether invoked directly or loaded dynamically
_TOOLS_DIR = str(Path(__file__).resolve().parent)
if _TOOLS_DIR not in sys.path:
    sys.path.insert(0, _TOOLS_DIR)

from fsm_parser import (  # noqa: E402
    FSM_ST_NONE,
    StateNode,
    Transition,
    parse_file,
    resolve_input_path,
    strip_c_comments,
)

FSM_END_MAP = {
    "FSM_END_EV": "[*]",
    "FSM_ST_END": "[*]",
}


def parse_fsms(c_source: str) -> Dict[str, Tuple[Dict[str, StateNode], List[Transition]]]:
    """Parse all FSM definitions in C source text."""
    parsed = parse_file(c_source)
    return {name: (fsm.states, fsm.transitions) for name, fsm in parsed.fsms.items()}


def _format_transition(tr: Transition) -> str:
    src = FSM_END_MAP.get(tr.source, tr.source)
    dst = FSM_END_MAP.get(tr.target, tr.target)
    ev = FSM_END_MAP.get(tr.event, tr.event)
    label = ev
    if tr.guard:
        label += f" [{tr.guard}]"
    if tr.work:
        label += f" / {tr.work}()"
    return f"{src} --> {dst} : {label}"


def _ancestors(state_name: str, states: Dict[str, StateNode]) -> List[str]:
    chain: List[str] = []
    cur = state_name
    seen = set()
    while cur in states and cur not in seen:
        seen.add(cur)
        parent = states[cur].parent
        if parent == FSM_ST_NONE or parent not in states:
            break
        chain.append(parent)
        cur = parent
    return chain


def _enclosing_scope(tr: Transition, states: Dict[str, StateNode]) -> Optional[str]:
    """Return the innermost composite state containing both endpoints (None = top level)."""
    if tr.source not in states or tr.target not in states:
        if tr.source in states and tr.target == "FSM_ST_END":
            parent = states[tr.source].parent
            return parent if parent in states else None
        return None

    src_anc = _ancestors(tr.source, states)
    dst_anc = set(_ancestors(tr.target, states))
    for anc in src_anc:
        if anc in dst_anc:
            return anc
    return None


def render_mermaid(states: Dict[str, StateNode], transitions: List[Transition]) -> str:
    """Render a parsed FSM into a Mermaid stateDiagram-v2 markdown block."""
    scoped_transitions: Dict[Optional[str], List[Transition]] = {}
    for tr in transitions:
        scope = _enclosing_scope(tr, states)
        scoped_transitions.setdefault(scope, []).append(tr)

    lines: List[str] = ["```mermaid", "stateDiagram-v2"]

    def emit_state(st_name: str, indent: int) -> None:
        st = states[st_name]
        pad = "    " * indent
        if st.children:
            lines.append(f"{pad}state {st.name} {{")
            if st.default_substate != FSM_ST_NONE:
                lines.append(f"{pad}    [*] --> {st.default_substate}")
            for child in st.children:
                emit_state(child, indent + 1)
            for tr in scoped_transitions.get(st.name, []):
                lines.append(f"{pad}    {_format_transition(tr)}")
            lines.append(f"{pad}}}")
        else:
            lines.append(f"{pad}{st.name}")

    top_states = [st.name for st in states.values() if st.parent == FSM_ST_NONE or st.parent not in states]
    for top in top_states:
        emit_state(top, 1)

    for tr in scoped_transitions.get(None, []):
        lines.append(f"    {_format_transition(tr)}")

    lines.append("```")
    return "\n".join(lines) + "\n"


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Create Mermaid stateDiagram-v2 diagrams from a C FSM file.")
    parser.add_argument("file_name", type=str, help="Path to the C file (with or without .c extension).")
    parser.add_argument("-o", "--output-dir", type=str, default=".", help="Directory for generated .md files.")
    parser.add_argument(
        "--stdout",
        action="store_true",
        help="Print generated Mermaid diagrams to stdout instead of writing files.",
    )
    args = parser.parse_args(argv)

    input_path = resolve_input_path(args.file_name)
    source = input_path.read_text(encoding="utf-8")
    fsms = parse_fsms(source)

    if not fsms:
        print(f"No FSM_STATES_INIT definitions found in {input_path}", file=sys.stderr)
        return 1

    out_dir = Path(args.output_dir)
    for fsm_name, (states, transitions) in fsms.items():
        diagram = render_mermaid(states, transitions)
        if args.stdout:
            print(diagram, end="")
        else:
            out_dir.mkdir(parents=True, exist_ok=True)
            out_file = out_dir / f"{fsm_name}.md"
            out_file.write_text(diagram, encoding="utf-8")
            print(f"Wrote {out_file}")

    return 0


__all__ = [
    "StateNode",
    "Transition",
    "parse_fsms",
    "render_mermaid",
    "resolve_input_path",
    "strip_c_comments",
    "main",
]

if __name__ == "__main__":
    raise SystemExit(main())
