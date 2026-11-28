#!/usr/bin/env python3
"""Shared line-aware C parser for the FSM library macros and runtime calls.

Used by both tools/fsm_2_mermaid.py and tools/fsm_check.py.
"""

from dataclasses import dataclass, field
from pathlib import Path
import re
from typing import Dict, Iterator, List, Optional, Set, Tuple


FSM_ST_NONE = "FSM_ST_NONE"
FSM_ST_END = "FSM_ST_END"
FSM_ST_FIRST = "FSM_ST_FIRST"
FSM_EV_NONE = "FSM_EV_NONE"
FSM_END_EV = "FSM_END_EV"
FSM_TIMEOUT_EV = "FSM_TIMEOUT_EV"
FSM_EV_FIRST = "FSM_EV_FIRST"

SENTINEL_RE = re.compile(r"^(?:.*_(?:LAST|COUNT|MAX|NUM)|(?:LAST|MAX|NUM)_[A-Z0-9_]+)$")

_COMMENT_OR_STR_RE = re.compile(
    r'"(?:\\.|[^"\\])*"'
    r"|'(?:\\.|[^'\\])*'"
    r"|/\*.*?\*/"
    r"|//[^\n]*",
    flags=re.DOTALL,
)

_INLINE_IGNORE_RE = re.compile(
    r"(?://|/\*)\s*fsm-check:\s*(?:ignore|disable)\s*[=:]?\s*([A-Za-z0-9_,\-\s]+)",
    flags=re.IGNORECASE,
)


@dataclass
class SymbolDecl:
    name: str
    line: int
    is_sentinel: bool = False


@dataclass
class StateNode:
    name: str
    parent: str
    default_substate: str
    entry_action: Optional[str] = None
    run_action: Optional[str] = None
    exit_action: Optional[str] = None
    line: int = 0
    children: List[str] = field(default_factory=list)


@dataclass
class Transition:
    source: str
    event: str
    target: str
    guard: Optional[str] = None
    work: Optional[str] = None
    macro: str = "FSM_TRANSITION_CREATE"
    line: int = 0


@dataclass
class ActorEntry:
    actor_name: str
    state_id: str
    entry_action: Optional[str] = None
    run_action: Optional[str] = None
    exit_action: Optional[str] = None
    line: int = 0


@dataclass
class InitCall:
    fsm_var: str
    fsm_name: str
    initial_state: str
    line: int


@dataclass
class TimedEventCall:
    fsm_var: str
    fsm_name: Optional[str]
    state_id: str
    ticks_expr: str
    is_disable: bool
    line: int


@dataclass
class DispatchCall:
    fsm_var: str
    event: str
    line: int


@dataclass
class FsmDefinition:
    name: str
    line: int = 0
    states: Dict[str, StateNode] = field(default_factory=dict)
    duplicate_states: List[StateNode] = field(default_factory=list)
    transitions: List[Transition] = field(default_factory=list)
    init_calls: List[InitCall] = field(default_factory=list)
    timed_calls: List[TimedEventCall] = field(default_factory=list)
    actor_link_count: int = 0


@dataclass
class ParsedFile:
    fsms: Dict[str, FsmDefinition] = field(default_factory=dict)
    state_enums: List[SymbolDecl] = field(default_factory=list)
    event_enums: List[SymbolDecl] = field(default_factory=list)
    actors: Dict[str, List[ActorEntry]] = field(default_factory=dict)
    dispatch_calls: List[DispatchCall] = field(default_factory=list)
    var_to_fsm: Dict[str, str] = field(default_factory=dict)
    terminating_functions: Set[str] = field(default_factory=set)
    inline_ignores: Dict[int, Set[str]] = field(default_factory=dict)


def extract_inline_ignores(source: str) -> Dict[int, Set[str]]:
    """Extract per-line suppression rules from comments like `// fsm-check: ignore W004`."""
    ignores: Dict[int, Set[str]] = {}
    for line_no, line in enumerate(source.splitlines(), start=1):
        match = _INLINE_IGNORE_RE.search(line)
        if not match:
            continue
        raw = match.group(1).split("*/")[0]
        tokens = {tok.strip() for tok in re.split(r"[,\s]+", raw) if tok.strip()}
        if tokens:
            ignores.setdefault(line_no, set()).update(tokens)
    return ignores


def strip_c_comments(source: str) -> str:
    """Remove C comments while preserving string literals and exact line numbers."""
    def _replace(match: re.Match) -> str:
        text = match.group(0)
        if text.startswith('"') or text.startswith("'"):
            return text
        if text.startswith("/*"):
            return "\n" * text.count("\n")
        return " " * len(text)

    return _COMMENT_OR_STR_RE.sub(_replace, source)


def _line_number_at(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def _split_top_level_args(raw: str) -> List[str]:
    """Split a comma-separated C argument string while respecting nested (), [], {}, and strings."""
    args: List[str] = []
    current: List[str] = []
    depth_paren = 0
    depth_bracket = 0
    depth_brace = 0
    in_str: Optional[str] = None
    escaped = False

    for ch in raw:
        if in_str is not None:
            current.append(ch)
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == in_str:
                in_str = None
            continue

        if ch in ('"', "'"):
            in_str = ch
            current.append(ch)
        elif ch == "(":
            depth_paren += 1
            current.append(ch)
        elif ch == ")":
            depth_paren = max(0, depth_paren - 1)
            current.append(ch)
        elif ch == "[":
            depth_bracket += 1
            current.append(ch)
        elif ch == "]":
            depth_bracket = max(0, depth_bracket - 1)
            current.append(ch)
        elif ch == "{":
            depth_brace += 1
            current.append(ch)
        elif ch == "}":
            depth_brace = max(0, depth_brace - 1)
            current.append(ch)
        elif ch == "," and depth_paren == 0 and depth_bracket == 0 and depth_brace == 0:
            args.append("".join(current).strip())
            current = []
        else:
            current.append(ch)

    tail = "".join(current).strip()
    if tail or args:
        args.append(tail)
    return args


def iter_macro_calls(clean_source: str, name_pattern: str) -> Iterator[Tuple[str, int, List[str], int, int]]:
    """Yield (matched_name, line_no, args, start_pos, end_pos) with balanced parentheses."""
    pattern = re.compile(rf"\b({name_pattern})\s*\(")
    pos = 0
    length = len(clean_source)

    while pos < length:
        match = pattern.search(clean_source, pos)
        if not match:
            break

        matched_name = match.group(1)
        open_paren = match.end() - 1
        depth = 1
        idx = open_paren + 1
        in_str: Optional[str] = None
        escaped = False

        while idx < length and depth > 0:
            ch = clean_source[idx]
            if in_str is not None:
                if escaped:
                    escaped = False
                elif ch == "\\":
                    escaped = True
                elif ch == in_str:
                    in_str = None
            else:
                if ch in ('"', "'"):
                    in_str = ch
                elif ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
            idx += 1

        if depth != 0:
            pos = match.end()
            continue

        raw_args = clean_source[open_paren + 1 : idx - 1]
        args = _split_top_level_args(raw_args) if raw_args.strip() else []
        line_no = _line_number_at(clean_source, match.start())
        yield matched_name, line_no, args, match.start(), idx
        pos = idx


def _norm_callback(expr: str) -> Optional[str]:
    cleaned = expr.strip()
    if not cleaned or cleaned in ("NULL", "0", "((void*)0)", "((void *)0)"):
        return None
    return cleaned


def _norm_fsm_var(expr: str) -> str:
    s = expr.strip()
    while s.startswith("(") and s.endswith(")"):
        s = s[1:-1].strip()
    if s.startswith("&"):
        s = s[1:].strip()
    return s


def _extract_state_get(expr: str) -> Tuple[Optional[str], Optional[str]]:
    match = re.search(r"FSM_STATE_GET\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", expr)
    if not match:
        return None, None
    return match.group(1), match.group(2)


def _extract_transitions_get(expr: str) -> Optional[str]:
    match = re.search(r"FSM_TRANSITIONS_GET\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", expr)
    if match:
        return match.group(1)
    match_arr = re.match(r"^\s*([A-Za-z_][A-Za-z0-9_]*)_transitions\s*$", expr)
    if match_arr:
        return match_arr.group(1)
    return None


def _parse_enums(clean_source: str, parsed: ParsedFile) -> None:
    enum_pat = re.compile(r"\benum\b[^{;]*\{(.*?)\}", flags=re.DOTALL)
    for match in enum_pat.finditer(clean_source):
        body = match.group(1)
        body_start = match.start(1)
        is_state_enum = bool(re.search(r"\bFSM_ST_FIRST\b", body))
        is_event_enum = bool(re.search(r"\bFSM_EV_FIRST\b", body))
        if not is_state_enum and not is_event_enum:
            continue

        target_list = parsed.state_enums if is_state_enum else parsed.event_enums
        cursor = 0
        for part in body.split(","):
            entry = part.strip()
            entry_offset = body.find(part, cursor)
            cursor = entry_offset + len(part) + 1
            if not entry:
                continue
            ident_match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)", entry)
            if not ident_match:
                continue
            sym = ident_match.group(1)
            leading_ws = len(part) - len(part.lstrip())
            line_no = _line_number_at(clean_source, body_start + max(0, entry_offset) + leading_ws)
            target_list.append(
                SymbolDecl(name=sym, line=line_no, is_sentinel=bool(SENTINEL_RE.match(sym)))
            )


def _parse_actors(clean_source: str, parsed: ParsedFile) -> None:
    init_calls = list(iter_macro_calls(clean_source, "FSM_ACTOR_INIT"))
    end_positions = [m.start() for m in re.finditer(r"\bFSM_ACTOR_END\s*\(\s*\)", clean_source)]

    for idx, (_, _, args, start_pos, end_pos) in enumerate(init_calls):
        if not args:
            continue
        actor_name = args[0]
        next_end = next((p for p in end_positions if p > end_pos), len(clean_source))
        block = clean_source[end_pos:next_end]
        entries: List[ActorEntry] = []
        for _, rel_line, c_args, _, _ in iter_macro_calls(block, "FSM_ACTOR_CREATE"):
            if len(c_args) < 4:
                continue
            abs_line = _line_number_at(clean_source, end_pos) + rel_line - 1
            entries.append(
                ActorEntry(
                    actor_name=actor_name,
                    state_id=c_args[0],
                    entry_action=_norm_callback(c_args[1]),
                    run_action=_norm_callback(c_args[2]),
                    exit_action=_norm_callback(c_args[3]),
                    line=abs_line,
                )
            )
        parsed.actors[actor_name] = entries


def _parse_terminating_functions(clean_source: str) -> Set[str]:
    """Identify C functions whose body invokes fsm_terminate(...)."""
    terminating: Set[str] = set()
    fn_pat = re.compile(
        r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\bfsm_t\s*\*[^;{}]*\)\s*\{",
        flags=re.DOTALL,
    )
    length = len(clean_source)
    for match in fn_pat.finditer(clean_source):
        fn_name = match.group(1)
        if fn_name in ("if", "while", "for", "switch"):
            continue
        brace_open = match.end() - 1
        depth = 1
        idx = brace_open + 1
        in_str: Optional[str] = None
        escaped = False
        while idx < length and depth > 0:
            ch = clean_source[idx]
            if in_str is not None:
                if escaped:
                    escaped = False
                elif ch == "\\":
                    escaped = True
                elif ch == in_str:
                    in_str = None
            else:
                if ch in ('"', "'"):
                    in_str = ch
                elif ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
            idx += 1
        body = clean_source[brace_open:idx]
        if re.search(r"\bfsm_terminate\s*\(", body):
            terminating.add(fn_name)
    return terminating


def parse_file(c_source: str) -> ParsedFile:
    """Parse all FSM definitions, enums, actors, and runtime calls from C source text."""
    parsed = ParsedFile(inline_ignores=extract_inline_ignores(c_source))
    clean = strip_c_comments(c_source)

    _parse_enums(clean, parsed)
    _parse_actors(clean, parsed)
    parsed.terminating_functions = _parse_terminating_functions(clean)

    # 1. Discover FSM definitions via FSM_STATES_INIT
    for _, line_no, args, _, _ in iter_macro_calls(clean, "FSM_STATES_INIT"):
        if not args:
            continue
        fsm_name = args[0]
        if fsm_name not in parsed.fsms:
            parsed.fsms[fsm_name] = FsmDefinition(name=fsm_name, line=line_no)

    # 2. Parse states
    for _, line_no, args, _, _ in iter_macro_calls(clean, "FSM_CREATE_STATE"):
        if len(args) < 4:
            continue
        fsm_name = args[0]
        fsm = parsed.fsms.setdefault(fsm_name, FsmDefinition(name=fsm_name, line=line_no))
        node = StateNode(
            name=args[1],
            parent=args[2],
            default_substate=args[3],
            entry_action=_norm_callback(args[4]) if len(args) > 4 else None,
            run_action=_norm_callback(args[5]) if len(args) > 5 else None,
            exit_action=_norm_callback(args[6]) if len(args) > 6 else None,
            line=line_no,
        )
        if node.name in fsm.states:
            fsm.duplicate_states.append(node)
        else:
            fsm.states[node.name] = node

    # Populate parent -> children adjacency lists
    for fsm in parsed.fsms.values():
        for st in fsm.states.values():
            if st.parent != FSM_ST_NONE and st.parent in fsm.states:
                fsm.states[st.parent].children.append(st.name)

    # 3. Parse transitions (preserving exact source order)
    tr_names = "FSM_TRANSITION_CREATE|FSM_TRANSITION_WORK_CREATE|FSM_TRANSITION_GUARD_CREATE"
    for macro, line_no, args, _, _ in iter_macro_calls(clean, tr_names):
        if len(args) < 4:
            continue
        fsm_name = args[0]
        fsm = parsed.fsms.setdefault(fsm_name, FsmDefinition(name=fsm_name, line=line_no))
        if macro == "FSM_TRANSITION_CREATE":
            fsm.transitions.append(
                Transition(source=args[1], event=args[2], target=args[3], macro=macro, line=line_no)
            )
        elif macro == "FSM_TRANSITION_WORK_CREATE" and len(args) >= 5:
            fsm.transitions.append(
                Transition(
                    source=args[1],
                    event=args[2],
                    target=args[3],
                    work=_norm_callback(args[4]),
                    macro=macro,
                    line=line_no,
                )
            )
        elif macro == "FSM_TRANSITION_GUARD_CREATE" and len(args) >= 6:
            fsm.transitions.append(
                Transition(
                    source=args[1],
                    event=args[2],
                    target=args[3],
                    guard=_norm_callback(args[4]),
                    work=_norm_callback(args[5]),
                    macro=macro,
                    line=line_no,
                )
            )

    # 4. Parse fsm_init(...) calls to map variable names -> FSM definitions & initial states
    var_to_fsm = parsed.var_to_fsm
    for _, line_no, args, _, _ in iter_macro_calls(clean, "fsm_init"):
        if len(args) < 6:
            continue
        fsm_var = _norm_fsm_var(args[0])
        tr_fsm = _extract_transitions_get(args[1])
        st_fsm, init_state = _extract_state_get(args[5])
        fsm_name = st_fsm or tr_fsm
        if fsm_name and fsm_name in parsed.fsms and init_state:
            var_to_fsm[fsm_var] = fsm_name
            parsed.fsms[fsm_name].init_calls.append(
                InitCall(fsm_var=fsm_var, fsm_name=fsm_name, initial_state=init_state, line=line_no)
            )

    # 5. Parse fsm_timed_event_set(fsm, state, ticks)
    for _, line_no, args, _, _ in iter_macro_calls(clean, "fsm_timed_event_set"):
        if len(args) < 3:
            continue
        fsm_var = _norm_fsm_var(args[0])
        st_fsm, state_id = _extract_state_get(args[1])
        ticks_expr = args[2].strip()
        is_disable = ticks_expr in ("0", "0U", "0u", "FSM_NO_TICKS")
        fsm_name = st_fsm or var_to_fsm.get(fsm_var)
        if fsm_name and fsm_name in parsed.fsms and state_id:
            parsed.fsms[fsm_name].timed_calls.append(
                TimedEventCall(
                    fsm_var=fsm_var,
                    fsm_name=fsm_name,
                    state_id=state_id,
                    ticks_expr=ticks_expr,
                    is_disable=is_disable,
                    line=line_no,
                )
            )

    # 6. Parse fsm_actor_link(fsm, actor, size)
    for _, _, args, _, _ in iter_macro_calls(clean, "fsm_actor_link"):
        if len(args) < 2:
            continue
        fsm_var = _norm_fsm_var(args[0])
        fsm_name = var_to_fsm.get(fsm_var)
        if fsm_name and fsm_name in parsed.fsms:
            parsed.fsms[fsm_name].actor_link_count += 1

    # 7. Parse fsm_dispatch(fsm, event, data)
    for _, line_no, args, _, _ in iter_macro_calls(clean, "fsm_dispatch"):
        if len(args) < 2:
            continue
        parsed.dispatch_calls.append(
            DispatchCall(fsm_var=_norm_fsm_var(args[0]), event=args[1].strip(), line=line_no)
        )

    return parsed


def resolve_input_path(file_arg: str) -> Path:
    """Resolve a C source path passed with or without the .c extension."""
    path = Path(file_arg)
    if path.is_file():
        return path
    with_ext = Path(f"{file_arg}.c")
    if with_ext.is_file():
        return with_ext
    raise FileNotFoundError(f"Could not find input C file: '{file_arg}' (or '{with_ext}')")
