# Hierarchical Finite State Machine (FSM) Library

## Overview

A lightweight, embeddable Hierarchical Finite State Machine (HSM) library in C11 designed for microcontrollers (ESP-IDF / FreeRTOS or bare-metal) and host C projects. State, transition, and actor tables are immutable (`const`) so they can reside in Flash/RODATA and be shared across multiple independent `fsm_t` instances.

## Features

- Hierarchical states with default substates, LCA-based exit/entry ordering, event bubbling, and symmetric self-transitions
- Entry, run, exit, and transition work callbacks
- Optional transition guard predicates (`FSM_TRANSITION_GUARD_CREATE`)
- Composite completion states (`FSM_ST_END` / `FSM_END_EV`) including chained completions
- Per-instance timed state transitions (`fsm_timed_event_set`, `fsm_ticks_hook`, `FSM_TIMEOUT_EV`)
- Decoupled observer "Actors" (`fsm_actor_link`) triggered along the hierarchy chain
- Dual event queue backend: built-in zero-allocation ring buffer or statically allocated FreeRTOS queue (`CONFIG_FSM_USE_FREERTOS_API`)
- Code-to-Mermaid diagram generator (`tools/fsm_2_mermaid.py`)

## Files

- `include/fsm.h` / `fsm.c`: Core HSM declarations and implementation
- `include/ring_buff.h` / `ring_buff.c`: Generic fixed-element-size ring buffer (bare-metal backend)
- `Kconfig` / `CMakeLists.txt`: ESP-IDF component configuration and build rules
- `tools/fsm_2_mermaid.py`: Mermaid `stateDiagram-v2` generator
- `tests/`: Host unit test suite (C tests with ASan/UBSan + Python tests for the Mermaid tool)
- `example/`: Runnable examples (`1_led_onoff`, `2_led_blinky`, `3_button`, `4_timed_event`, `others/fsm_music.c`)

## Quick Start

### 1. Define States, Transitions, and Actors

```c
#include "fsm.h"

enum {
    ST_ROOT = FSM_ST_FIRST,
    ST_OFF,
    ST_ON,
};

enum {
    EV_ON = FSM_EV_FIRST,
    EV_OFF,
    EV_LAST,
};

// State table (const / Flash-resident)
FSM_STATES_INIT(my_fsm)
FSM_CREATE_STATE(my_fsm, ST_ROOT, FSM_ST_NONE, ST_OFF,      NULL,      NULL,   NULL)
FSM_CREATE_STATE(my_fsm, ST_OFF,  ST_ROOT,     FSM_ST_NONE, enter_off, NULL,   exit_off)
FSM_CREATE_STATE(my_fsm, ST_ON,   ST_ROOT,     FSM_ST_NONE, enter_on,  run_on, exit_on)
FSM_STATES_END()

// Transition table (const / Flash-resident)
FSM_TRANSITIONS_INIT(my_fsm)
FSM_TRANSITION_CREATE(my_fsm, ST_OFF, EV_ON,  ST_ON)
FSM_TRANSITION_WORK_CREATE(my_fsm, ST_ON, EV_OFF, ST_OFF, on_power_off)
// Optional guarded transition:
// FSM_TRANSITION_GUARD_CREATE(my_fsm, ST_OFF, EV_ON, ST_ON, can_turn_on, on_turn_on)
FSM_TRANSITIONS_END()

// Optional Actor table (const / Flash-resident)
FSM_ACTOR_INIT(my_actor)
FSM_ACTOR_CREATE(ST_OFF, actor_enter_off, NULL, NULL)
FSM_ACTOR_CREATE(ST_ON,  actor_enter_on,  actor_run_on, actor_exit_on)
FSM_ACTOR_END()
```

### 2. Initialize, Link Actors, Configure Timers, and Run

```c
fsm_t my_fsm = {0};
my_ctx_t ctx = {0};

fsm_init(&my_fsm,
         FSM_TRANSITIONS_GET(my_fsm),
         FSM_TRANSITIONS_SIZE(my_fsm),
         EV_LAST,
         1,                                // ticks per ms (or FSM_NO_TICKS)
         &FSM_STATE_GET(my_fsm, ST_ROOT),
         &ctx);

fsm_actor_link(&my_fsm, FSM_ACTOR_GET(my_actor), FSM_ACTOR_SIZE(my_actor));

// Optional per-instance state timeout (can be set before or after entering the state)
fsm_timed_event_set(&my_fsm, &FSM_STATE_GET(my_fsm, ST_ON), FSM_MS_2_TICKS(&my_fsm, 500));

// Dispatch events (safe to call with optional per-event payload)
fsm_dispatch(&my_fsm, EV_ON, NULL);

// Process pending events and execute current state's run_action once
int rc = fsm_run(&my_fsm);

// Cleanup when finished
fsm_deinit(&my_fsm);
```

### 3. Accessing Instance Context vs. Event Payload

- State `run_action` and `FSM_TIMEOUT_EV` callbacks receive `fsm->current_data` (`initial_data`).
- Entry, exit, guard, and transition work callbacks receive the `data` pointer passed to `fsm_dispatch()`.
- Any callback can always read or update the instance context via `fsm_data_get(self)` / `fsm_data_set(self, data)`.

## API Reference

| Function | Purpose |
| :--- | :--- |
| `fsm_init(fsm, transitions, num_transitions, num_events, ticks_per_ms, initial_state, initial_data)` | Initialize an FSM instance and enter `initial_state` (including default substates) |
| `fsm_deinit(fsm)` | Flush/delete queue and reset the FSM instance |
| `fsm_dispatch(fsm, event, data)` | Enqueue an event to be processed on the next `fsm_run()` |
| `fsm_run(fsm)` | Process all queued events and run the active state + actors once; returns `0` or `terminate_val` |
| `fsm_state_get(fsm)` | Return the active leaf state ID (or `FSM_ST_NONE`) |
| `fsm_actor_link(fsm, actor, size)` | Attach a `const` actor table to an FSM instance |
| `fsm_timed_event_set(fsm, state, ticks)` | Set per-instance timeout ticks for `state` (`0` disables); immediately arms if active |
| `fsm_ticks_hook(fsm)` | Decrement active timer by 1 tick and enqueue priority `FSM_TIMEOUT_EV` on expiry |
| `fsm_data_get(fsm)` / `fsm_data_set(fsm, data)` | Get or update the FSM instance's context pointer |
| `fsm_has_pending_events(fsm)` | Return `1` if events are queued, `0` if empty, `-1` on error |
| `fsm_flush_events(fsm)` | Discard all queued events (do not call from ISR in FreeRTOS mode) |
| `fsm_terminate(fsm, val)` | Stop execution immediately and return `val` from `fsm_run()` |

### Composite End States (`FSM_ST_END` / `FSM_END_EV`)

A substate can transition to `FSM_ST_END` to signal completion of its enclosing composite state. The FSM searches upward for the closest ancestor with an `FSM_END_EV` transition (supporting chained `FSM_ST_END` completions). If no ancestor handles `FSM_END_EV`, the FSM terminates with `FSM_END_EV`.

## Configuration

All options can be set via `idf.py menuconfig` (`FSM Configuration`) or compile-time `-D` defines:

| Macro | Kconfig | Default | Description |
| :--- | :--- | :--- | :--- |
| `CONFIG_FSM_USE_FREERTOS_API` | `FSM_USE_FREERTOS_API` | `n` | Use a statically allocated FreeRTOS queue instead of `ring_buff` |
| `CONFIG_RUN_ON_TIMER_HOOK` | `FSM_RUN_ON_TIMER_HOOK` | `0` (`n`) | Call `fsm_run()` inside `fsm_ticks_hook()` (enable **only** if strictly serialized with `fsm_run()`) |
| `FSM_MAX_EVENTS` | `FSM_MAX_EVENTS` | `64` | Event queue slots (bare-metal capacity is `FSM_MAX_EVENTS - 1`) |
| `MAX_HIERARCHY_DEPTH` | `FSM_MAX_HIERARCHY_DEPTH` | `8` | Maximum state nesting depth |
| `FSM_MAX_ACTORS` | `FSM_MAX_ACTORS` | `10` | Maximum linked actors per FSM instance |
| `FSM_MAX_TIMED_STATES` | `FSM_MAX_TIMED_STATES` | `8` | Maximum distinct timed states per FSM instance |

> **Concurrency Note:** In bare-metal mode, `ring_buff` does not disable interrupts internally. If `fsm_dispatch()` or `fsm_ticks_hook()` is called from an ISR while `fsm_run()` runs in the main loop, either enable `CONFIG_FSM_USE_FREERTOS_API` or wrap queue calls in your platform's critical section.

## Generating Mermaid Diagrams

Generate nested `stateDiagram-v2` diagrams directly from your C source files:

```bash
python3 tools/fsm_2_mermaid.py example/others/fsm_music.c -o docs/
# Or print directly to stdout:
python3 tools/fsm_2_mermaid.py example/2_led_blinky/fsm_led.c --stdout
```

## Running Unit Tests

Build and run the full host verification suite (bare-metal & FreeRTOS-mocked C unit tests with AddressSanitizer/UndefinedBehaviorSanitizer, all 5 examples compiled & executed as integration tests, and Mermaid generator tests):

```bash
make -C tests run
```

Individual targets are also available: `run-unit`, `run-freertos`, `run-examples`, and `run-mermaid`.

## Project Evolution & AI-Assisted Development

This repository is also a snapshot of how AI-assisted embedded engineering has evolved over a couple of years:

- **v1.0 (2024–2025) — Conversational AI Assistance (Claude):** When this library was first built, Claude was used as an interactive design and coding partner — helping reason through tricky logic like the two-pointer Lowest Common Ancestor (`find_lca`) algorithm, double-checking hierarchical state machine architecture decisions, drafting docs, and generating Mermaid diagrams from state tables. The original README from that era is preserved in [README_LEGACY.md](README_LEGACY.md).
- **v2.0 (2026) — Agentic Engineering (Antigravity):** Returning to this repo with a new generation of agentic AI fundamentally changed the development methodology. Instead of pasting isolated functions into a chat window, the workflow shifted to pointing the agent (Google Antigravity) at the whole codebase to perform a full architectural review, surface memory-safety/HSM/concurrency bugs across C, Kconfig, CMake, examples, and Python tooling, and execute a multi-commit overhaul — shrinking per-instance RAM by >90%, fixing every example, building an ASan/UBSan test suite to validate each change along the way, and handling the git branch, push, and PR workflow end-to-end.

## License

MIT License — Copyright (c) 2024 Mauro Medina
