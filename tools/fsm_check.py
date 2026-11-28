#!/usr/bin/env python3
"""Static FSM Linter & Validator for C files using the FSM library.

Checks structural, hierarchical, reachability, timer, actor, and unused-symbol
invariants at build time before flashing.

Usage:
    python3 tools/fsm_check.py path/to/file1.c [file2.c ...] [--strict] [--format text|json|github]
"""

import argparse
from collections import deque
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import re
import sys
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

_TOOLS_DIR = str(Path(__file__).resolve().parent)
if _TOOLS_DIR not in sys.path:
    sys.path.insert(0, _TOOLS_DIR)

from fsm_parser import (  # noqa: E402
    FSM_END_EV,
    FSM_EV_NONE,
    FSM_ST_END,
    FSM_ST_NONE,
    FSM_TIMEOUT_EV,
    FsmDefinition,
    ParsedFile,
    StateNode,
    Transition,
    parse_file,
    resolve_input_path,
)

DEFAULT_MAX_HIERARCHY_DEPTH = 8
DEFAULT_MAX_TIMED_STATES = 8
DEFAULT_MAX_ACTORS = 10

RULE_DESCRIPTIONS: Dict[str, Tuple[str, str]] = {
    # Errors (structural / guaranteed bugs)
    "E001": ("error", "Duplicate state definition in FSM_STATES_INIT"),
    "E002": ("error", "State parent is not a defined state"),
    "E003": ("error", "State default_substate is not a defined state"),
    "E004": ("error", "State default_substate does not declare this state as its parent"),
    "E005": ("error", "Cycle detected in state parent hierarchy"),
    "E006": ("error", "Cycle detected in default_substate chain"),
    "E007": ("error", "State hierarchy depth exceeds MAX_HIERARCHY_DEPTH"),
    "E008": ("error", "Transition source_state is not a defined state"),
    "E009": ("error", "Transition target_state is not a defined state or FSM_ST_END"),
    "E010": ("error", "Transition event is FSM_EV_NONE"),
    "E011": ("error", "Duplicate/shadowed transition on same (source_state, event) without guard"),
    "E012": ("error", "FSM has states defined but zero transitions"),
    "E013": ("error", "fsm_init() references an undefined initial state"),
    "E014": ("error", "Distinct timed states exceed FSM_MAX_TIMED_STATES"),
    "E015": ("error", "Linked actors exceed FSM_MAX_ACTORS"),
    "E016": ("error", "fsm_dispatch() dispatches invalid event FSM_EV_NONE"),
    "E017": ("error", "fsm_timed_event_set() references an undefined state"),
    # Warnings (design smells / likely bugs)
    "W001": ("warning", "State is unreachable from the initial state"),
    "W002": ("warning", "Reachable leaf state is a dead end (no outgoing transitions and does not terminate)"),
    "W003": ("warning", "Declared state enum symbol is never defined via FSM_CREATE_STATE"),
    "W004": ("warning", "Declared event enum symbol is never referenced in any transition"),
    "W005": ("warning", "Defined state is never referenced in any transition"),
    "W006": ("warning", "Timed state has no matching FSM_TIMEOUT_EV transition"),
    "W007": ("warning", "FSM_TIMEOUT_EV transition has no fsm_timed_event_set() configured in this file"),
    "W008": ("warning", "FSM_END_EV transition on state whose subtree never transitions to FSM_ST_END"),
    "W009": ("warning", "Composite state with default_substate has a run_action that will never execute"),
    "W010": ("warning", "Actor references a state ID not defined in any FSM in this file"),
    "W011": ("warning", "Duplicate guarded transition with identical (source, event, target, guard)"),
    "W012": ("warning", "fsm_dispatch() dispatches an event not handled by any transition in the FSM"),
}


@dataclass
class Diagnostic:
    file: str
    line: int
    severity: str  # "error" or "warning"
    code: str
    fsm: Optional[str]
    message: str

    def format_text(self) -> str:
        scope = f"[{self.fsm}] " if self.fsm else ""
        return f"{self.file}:{self.line}: {self.severity} [{self.code}]: {scope}{self.message}"

    def format_github(self) -> str:
        scope = f"[{self.fsm}] " if self.fsm else ""
        return f"::{self.severity} file={self.file},line={self.line},title={self.code}::{scope}{self.message}"


@dataclass
class LintConfig:
    max_depth: int = DEFAULT_MAX_HIERARCHY_DEPTH
    max_timed_states: int = DEFAULT_MAX_TIMED_STATES
    max_actors: int = DEFAULT_MAX_ACTORS
    ignored_rules: Set[str] = None  # type: ignore[assignment]

    def __post_init__(self) -> None:
        if self.ignored_rules is None:
            self.ignored_rules = set()


def _ancestors_safe(state_name: str, states: Dict[str, StateNode]) -> Tuple[List[str], bool]:
    """Return (ancestors_from_parent_up, has_cycle)."""
    chain: List[str] = []
    seen: Set[str] = {state_name}
    cur = state_name
    while cur in states:
        parent = states[cur].parent
        if parent == FSM_ST_NONE or parent not in states:
            return chain, False
        if parent in seen:
            return chain, True
        seen.add(parent)
        chain.append(parent)
        cur = parent
    return chain, False


def _default_substate_chain(state_name: str, states: Dict[str, StateNode]) -> Tuple[List[str], bool]:
    """Return (chain_starting_at_state, has_cycle)."""
    chain: List[str] = [state_name]
    seen: Set[str] = {state_name}
    cur = state_name
    while cur in states:
        sub = states[cur].default_substate
        if sub == FSM_ST_NONE or sub not in states:
            return chain, False
        if sub in seen:
            return chain, True
        seen.add(sub)
        chain.append(sub)
        cur = sub
    return chain, False


def _subtree_states(root_name: str, states: Dict[str, StateNode]) -> Set[str]:
    """Return root_name and all descendants reachable via parent->children."""
    visited: Set[str] = set()
    queue: deque[str] = deque([root_name])
    while queue:
        cur = queue.popleft()
        if cur in visited or cur not in states:
            continue
        visited.add(cur)
        for child in states[cur].children:
            if child not in visited:
                queue.append(child)
    return visited


def _is_terminating_state(st: StateNode, parsed: ParsedFile) -> bool:
    for cb in (st.entry_action, st.run_action, st.exit_action):
        if cb and cb in parsed.terminating_functions:
            return True
    for entries in parsed.actors.values():
        for entry in entries:
            if entry.state_id != st.name:
                continue
            for cb in (entry.entry_action, entry.run_action, entry.exit_action):
                if cb and cb in parsed.terminating_functions:
                    return True
    return False


def _resolve_end_targets(start_state: str, fsm: FsmDefinition) -> List[str]:
    """Resolve all reachable target states from transitioning to FSM_ST_END at start_state."""
    targets: List[str] = []
    states = fsm.states
    visited_search: Set[str] = set()
    queue: deque[str] = deque([start_state])

    while queue:
        search = queue.popleft()
        if search in visited_search or search not in states:
            continue
        visited_search.add(search)

        anc_chain, _ = _ancestors_safe(search, states)
        candidates = [search] + anc_chain

        # Closest ancestor level that has at least one FSM_END_EV transition
        for level_state in candidates:
            level_trs = [
                t for t in fsm.transitions if t.event == FSM_END_EV and t.source == level_state
            ]
            if not level_trs:
                continue
            for t in level_trs:
                if t.target == FSM_ST_END:
                    parent = states[level_state].parent
                    if parent != FSM_ST_NONE and parent in states:
                        queue.append(parent)
                elif t.target in states:
                    targets.append(t.target)
            # Only continue to outer ancestor if all handlers at this level are guarded
            if any(t.guard is None for t in level_trs):
                break

    return targets


def _compute_reachable_states(fsm: FsmDefinition) -> Set[str]:
    """HSM-aware reachability from initial state(s), including bubbling and FSM_END_EV chains."""
    states = fsm.states
    if not states:
        return set()

    seeds: List[str] = []
    for init in fsm.init_calls:
        if init.initial_state in states and init.initial_state not in seeds:
            seeds.append(init.initial_state)

    if not seeds:
        # Fall back to top-level states (parent == FSM_ST_NONE) when fsm_init() is in another file
        seeds = [st.name for st in states.values() if st.parent == FSM_ST_NONE]
        if not seeds:
            seeds = [next(iter(states.keys()))]

    reachable: Set[str] = set()
    active_leaves: Set[str] = set()
    worklist: deque[str] = deque()

    def enter(target: str) -> None:
        if target not in states:
            return
        # Entering target marks its ancestors and drills down default_substate chain
        ancs, _ = _ancestors_safe(target, states)
        for a in ancs:
            if a not in reachable:
                reachable.add(a)
                worklist.append(a)
        sub_chain, _ = _default_substate_chain(target, states)
        for s in sub_chain:
            if s not in reachable:
                reachable.add(s)
                worklist.append(s)
        leaf = sub_chain[-1]
        active_leaves.add(leaf)

    for seed in seeds:
        enter(seed)

    while worklist:
        st_name = worklist.popleft()
        ancs, _ = _ancestors_safe(st_name, states)
        # An active state can trigger transitions on itself or any of its ancestors (event bubbling)
        handlers = {st_name} | set(ancs)
        for tr in fsm.transitions:
            if tr.event == FSM_END_EV:
                continue
            if tr.source not in handlers:
                continue
            if tr.target == FSM_ST_END:
                for end_tgt in _resolve_end_targets(tr.source, fsm):
                    enter(end_tgt)
            elif tr.target in states:
                enter(tr.target)

    return reachable


def _is_suppressed(code: str, line: int, parsed: ParsedFile, cfg: LintConfig) -> bool:
    if code in cfg.ignored_rules or "ALL" in cfg.ignored_rules:
        return True
    line_ignores = parsed.inline_ignores.get(line)
    if line_ignores and (code in line_ignores or "ALL" in line_ignores):
        return True
    return False


def lint_source(
    c_source: str,
    file_label: str = "<string>",
    config: Optional[LintConfig] = None,
) -> List[Diagnostic]:
    """Run all static FSM checks on C source text and return sorted diagnostics."""
    cfg = config or LintConfig()
    parsed = parse_file(c_source)
    diagnostics: List[Diagnostic] = []

    def emit(code: str, line: int, message: str, fsm_name: Optional[str] = None) -> None:
        if _is_suppressed(code, line, parsed, cfg):
            return
        severity = RULE_DESCRIPTIONS[code][0]
        diagnostics.append(
            Diagnostic(
                file=file_label,
                line=line,
                severity=severity,
                code=code,
                fsm=fsm_name,
                message=message,
            )
        )

    all_defined_states: Set[str] = set()
    all_transition_events: Set[str] = set()

    for fsm_name, fsm in parsed.fsms.items():
        states = fsm.states
        all_defined_states.update(states.keys())

        # E001: Duplicate state definitions
        for dup in fsm.duplicate_states:
            orig_line = states[dup.name].line
            emit(
                "E001",
                dup.line,
                f"duplicate state '{dup.name}' (first defined at line {orig_line})",
                fsm_name,
            )

        # E012: Empty transition table
        if states and not fsm.transitions:
            emit("E012", fsm.line, "FSM defines states but has no transitions", fsm_name)

        # E013: Invalid initial state in fsm_init()
        for init in fsm.init_calls:
            if init.initial_state not in states:
                emit(
                    "E013",
                    init.line,
                    f"fsm_init() initial state '{init.initial_state}' is not defined in FSM '{fsm_name}'",
                    fsm_name,
                )

        # E015: Too many linked actors
        if fsm.actor_link_count > cfg.max_actors:
            emit(
                "E015",
                fsm.line,
                f"FSM links {fsm.actor_link_count} actors, exceeding FSM_MAX_ACTORS ({cfg.max_actors})",
                fsm_name,
            )

        # E002..E007 & W009: State hierarchy & structure checks
        reported_parent_cycles: Set[str] = set()
        reported_sub_cycles: Set[str] = set()

        for st in states.values():
            if st.parent != FSM_ST_NONE and st.parent not in states:
                emit("E002", st.line, f"state '{st.name}' references undefined parent '{st.parent}'", fsm_name)

            if st.default_substate != FSM_ST_NONE:
                if st.default_substate not in states:
                    emit(
                        "E003",
                        st.line,
                        f"state '{st.name}' references undefined default_substate '{st.default_substate}'",
                        fsm_name,
                    )
                else:
                    sub_node = states[st.default_substate]
                    if sub_node.parent != st.name:
                        emit(
                            "E004",
                            st.line,
                            f"state '{st.name}' sets default_substate '{st.default_substate}', "
                            f"whose parent is '{sub_node.parent}' instead of '{st.name}'",
                            fsm_name,
                        )
                # W009: Composite state with default_substate has unreachable run_action
                if st.run_action is not None:
                    emit(
                        "W009",
                        st.line,
                        f"composite state '{st.name}' has default_substate '{st.default_substate}', "
                        f"so its run_action '{st.run_action}' will never be executed",
                        fsm_name,
                    )

            ancs, has_p_cycle = _ancestors_safe(st.name, states)
            if has_p_cycle and st.name not in reported_parent_cycles:
                reported_parent_cycles.add(st.name)
                emit("E005", st.line, f"cycle detected in parent hierarchy involving '{st.name}'", fsm_name)
            elif not has_p_cycle:
                depth = len(ancs) + 1
                if depth > cfg.max_depth:
                    emit(
                        "E007",
                        st.line,
                        f"state '{st.name}' has hierarchy depth {depth}, exceeding MAX_HIERARCHY_DEPTH ({cfg.max_depth})",
                        fsm_name,
                    )

            _, has_s_cycle = _default_substate_chain(st.name, states)
            if has_s_cycle and st.name not in reported_sub_cycles:
                reported_sub_cycles.add(st.name)
                emit(
                    "E006",
                    st.line,
                    f"cycle detected in default_substate chain involving '{st.name}'",
                    fsm_name,
                )

        # E008..E011 & W011: Transition validation & duplicate/shadowing detection
        seen_by_src_ev: Dict[Tuple[str, str], List[Transition]] = {}
        referenced_in_transitions: Set[str] = set()

        for tr in fsm.transitions:
            all_transition_events.add(tr.event)

            if tr.source not in states:
                emit("E008", tr.line, f"transition source '{tr.source}' is not a defined state", fsm_name)
            else:
                referenced_in_transitions.add(tr.source)

            if tr.target != FSM_ST_END and tr.target not in states:
                emit(
                    "E009",
                    tr.line,
                    f"transition target '{tr.target}' is not a defined state or FSM_ST_END",
                    fsm_name,
                )
            elif tr.target in states:
                referenced_in_transitions.add(tr.target)

            if tr.event == FSM_EV_NONE:
                emit("E010", tr.line, f"transition from '{tr.source}' uses invalid event FSM_EV_NONE", fsm_name)

            key = (tr.source, tr.event)
            prev_list = seen_by_src_ev.setdefault(key, [])
            if prev_list:
                earlier_unguarded = next((p for p in prev_list if p.guard is None), None)
                if earlier_unguarded is not None:
                    emit(
                        "E011",
                        tr.line,
                        f"transition ({tr.source}, {tr.event}) -> {tr.target} is shadowed by earlier "
                        f"unguarded transition at line {earlier_unguarded.line}",
                        fsm_name,
                    )
                elif tr.guard is not None:
                    exact_dup = next(
                        (p for p in prev_list if p.target == tr.target and p.guard == tr.guard),
                        None,
                    )
                    if exact_dup is not None:
                        emit(
                            "W011",
                            tr.line,
                            f"duplicate guarded transition ({tr.source}, {tr.event}) [{tr.guard}] -> {tr.target} "
                            f"(first defined at line {exact_dup.line})",
                            fsm_name,
                        )
            prev_list.append(tr)

        # W001: Unreachable states
        reachable = _compute_reachable_states(fsm)
        for st in states.values():
            if st.name not in reachable:
                emit("W001", st.line, f"state '{st.name}' is unreachable from the initial state", fsm_name)

        # W002: Dead-end reachable leaf states
        # A leaf state can exit if it or any ancestor has an outgoing transition (non-FSM_END_EV),
        # or if it (or an ancestor entered with it) calls fsm_terminate() in its callbacks.
        states_with_outgoing: Set[str] = {
            tr.source for tr in fsm.transitions if tr.event != FSM_END_EV and tr.source in states
        }
        for st in states.values():
            if st.name not in reachable:
                continue
            if st.children:
                continue  # composite state
            if _is_terminating_state(st, parsed):
                continue
            ancs, _ = _ancestors_safe(st.name, states)
            hierarchy = [st.name] + ancs
            has_exit = any(s in states_with_outgoing for s in hierarchy)
            if not has_exit:
                emit(
                    "W002",
                    st.line,
                    f"reachable leaf state '{st.name}' is a dead end (no outgoing transitions on state or ancestors)",
                    fsm_name,
                )

        # W005: Defined leaf/standalone states never referenced in any transition
        structural_roles: Set[str] = set()
        for st in states.values():
            if st.parent == FSM_ST_NONE or st.children:
                structural_roles.add(st.name)
            if st.default_substate != FSM_ST_NONE:
                structural_roles.add(st.default_substate)
        for st in states.values():
            if st.name not in referenced_in_transitions and st.name not in structural_roles:
                emit(
                    "W005",
                    st.line,
                    f"state '{st.name}' is never referenced in any transition",
                    fsm_name,
                )

        # Timer checks: E014, E017, W006, W007
        active_timed_states: Dict[str, int] = {}
        for tc in fsm.timed_calls:
            if tc.state_id not in states:
                emit(
                    "E017",
                    tc.line,
                    f"fsm_timed_event_set() references undefined state '{tc.state_id}' in FSM '{fsm_name}'",
                    fsm_name,
                )
                continue
            if not tc.is_disable and tc.state_id not in active_timed_states:
                active_timed_states[tc.state_id] = tc.line

        if len(active_timed_states) > cfg.max_timed_states:
            emit(
                "E014",
                fsm.line,
                f"FSM configures {len(active_timed_states)} distinct timed states, "
                f"exceeding FSM_MAX_TIMED_STATES ({cfg.max_timed_states})",
                fsm_name,
            )

        timeout_sources: Set[str] = {
            tr.source for tr in fsm.transitions if tr.event == FSM_TIMEOUT_EV and tr.source in states
        }

        for timed_st, line_no in active_timed_states.items():
            if timed_st not in states:
                continue
            ancs, _ = _ancestors_safe(timed_st, states)
            desc = _subtree_states(timed_st, states)
            related = set(ancs) | desc
            if not (related & timeout_sources):
                emit(
                    "W006",
                    line_no,
                    f"timed state '{timed_st}' has fsm_timed_event_set() but no FSM_TIMEOUT_EV transition",
                    fsm_name,
                )

        if fsm.init_calls:
            for tr in fsm.transitions:
                if tr.event != FSM_TIMEOUT_EV or tr.source not in states:
                    continue
                ancs, _ = _ancestors_safe(tr.source, states)
                desc = _subtree_states(tr.source, states)
                related = set(ancs) | desc
                if not (related & set(active_timed_states.keys())):
                    emit(
                        "W007",
                        tr.line,
                        f"FSM_TIMEOUT_EV transition on '{tr.source}' has no fsm_timed_event_set() in this file",
                        fsm_name,
                    )

        # W008: Orphan FSM_END_EV transitions (subtree never transitions to FSM_ST_END)
        end_sources: Set[str] = {
            tr.source for tr in fsm.transitions if tr.target == FSM_ST_END and tr.source in states
        }
        for tr in fsm.transitions:
            if tr.event != FSM_END_EV or tr.source not in states:
                continue
            sub = _subtree_states(tr.source, states)
            if not (sub & end_sources):
                emit(
                    "W008",
                    tr.line,
                    f"FSM_END_EV transition on '{tr.source}' is never triggered (no state in its subtree transitions to FSM_ST_END)",
                    fsm_name,
                )

    # File-level enum checks: W003, W004
    if parsed.fsms:
        for sym in parsed.state_enums:
            if sym.is_sentinel:
                continue
            if sym.name not in all_defined_states:
                emit(
                    "W003",
                    sym.line,
                    f"declared state enum '{sym.name}' is never defined with FSM_CREATE_STATE",
                )

        for sym in parsed.event_enums:
            if sym.is_sentinel:
                continue
            if sym.name not in all_transition_events:
                emit(
                    "W004",
                    sym.line,
                    f"declared event enum '{sym.name}' is never referenced in any transition",
                )

        # W010: Actors referencing undefined state IDs
        for actor_name, entries in parsed.actors.items():
            for entry in entries:
                if entry.state_id not in all_defined_states:
                    emit(
                        "W010",
                        entry.line,
                        f"actor '{actor_name}' references state '{entry.state_id}' not defined in this file",
                    )

        # E016 / W012: Check fsm_dispatch() event identifiers
        ident_re = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
        for dc in parsed.dispatch_calls:
            if dc.event == FSM_EV_NONE:
                emit("E016", dc.line, "fsm_dispatch() called with invalid event FSM_EV_NONE")
                continue
            if not ident_re.match(dc.event):
                continue
            target_fsm_name = parsed.var_to_fsm.get(dc.fsm_var)
            if target_fsm_name and target_fsm_name in parsed.fsms:
                fsm_events = {t.event for t in parsed.fsms[target_fsm_name].transitions}
                if dc.event not in fsm_events:
                    emit(
                        "W012",
                        dc.line,
                        f"fsm_dispatch() dispatches '{dc.event}', which has no matching transition",
                        target_fsm_name,
                    )

    diagnostics.sort(key=lambda d: (d.line, d.severity != "error", d.code))
    return diagnostics


def lint_files(paths: Sequence[Path], config: LintConfig) -> List[Diagnostic]:
    all_diags: List[Diagnostic] = []
    for path in paths:
        source = path.read_text(encoding="utf-8")
        all_diags.extend(lint_source(source, file_label=str(path), config=config))
    return all_diags


def _expand_inputs(file_args: Iterable[str]) -> List[Path]:
    paths: List[Path] = []
    for arg in file_args:
        p = Path(arg)
        if p.is_dir():
            paths.extend(sorted(p.rglob("*.c")))
        else:
            paths.append(resolve_input_path(arg))
    return paths


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Static linter and validator for C files using the FSM library."
    )
    parser.add_argument(
        "files",
        nargs="+",
        help="C source files (with or without .c) or directories to scan recursively.",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Treat warnings as errors (non-zero exit code if any warning is emitted).",
    )
    parser.add_argument(
        "--max-depth",
        type=int,
        default=DEFAULT_MAX_HIERARCHY_DEPTH,
        help=f"Maximum allowed state hierarchy depth (default: {DEFAULT_MAX_HIERARCHY_DEPTH}).",
    )
    parser.add_argument(
        "--max-timed-states",
        type=int,
        default=DEFAULT_MAX_TIMED_STATES,
        help=f"Maximum allowed distinct timed states per FSM (default: {DEFAULT_MAX_TIMED_STATES}).",
    )
    parser.add_argument(
        "--max-actors",
        type=int,
        default=DEFAULT_MAX_ACTORS,
        help=f"Maximum allowed linked actors per FSM (default: {DEFAULT_MAX_ACTORS}).",
    )
    parser.add_argument(
        "--ignore",
        action="append",
        default=[],
        help="Comma-separated rule codes to ignore (e.g. --ignore W004,W009).",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json", "github"),
        default="text",
        help="Output format (default: text).",
    )
    parser.add_argument(
        "--list-rules",
        action="store_true",
        help="List all lint rule codes and exit.",
    )

    if argv is None:
        raw_argv = sys.argv[1:]
    else:
        raw_argv = argv

    if "--list-rules" in raw_argv:
        for code, (sev, desc) in sorted(RULE_DESCRIPTIONS.items()):
            print(f"{code} ({sev:7s}): {desc}")
        return 0

    args = parser.parse_args(raw_argv)

    ignored: Set[str] = set()
    for item in args.ignore:
        ignored.update(tok.strip() for tok in item.split(",") if tok.strip())

    config = LintConfig(
        max_depth=args.max_depth,
        max_timed_states=args.max_timed_states,
        max_actors=args.max_actors,
        ignored_rules=ignored,
    )

    paths = _expand_inputs(args.files)
    diagnostics = lint_files(paths, config)

    if args.format == "json":
        print(json.dumps([asdict(d) for d in diagnostics], indent=2))
    elif args.format == "github":
        for d in diagnostics:
            print(d.format_github())
    else:
        for d in diagnostics:
            print(d.format_text())
        errors = sum(1 for d in diagnostics if d.severity == "error")
        warnings = sum(1 for d in diagnostics if d.severity == "warning")
        print(f"Checked {len(paths)} file(s): {errors} error(s), {warnings} warning(s)")

    has_errors = any(d.severity == "error" for d in diagnostics)
    has_warnings = any(d.severity == "warning" for d in diagnostics)
    if has_errors or (args.strict and has_warnings):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
