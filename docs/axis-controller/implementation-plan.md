# Axis Controller Implementation Plan

This document records the intended implementation sequence for the `v1`
single-axis controller firmware.

The purpose of the plan is to make scope, deferrals, and future work explicit
before the register-bank implementation starts.

## Immediate Goal

Implement a single-axis register-bank firmware that can be exercised entirely
through the USB console before RS485 is introduced.

The first usable milestone is:

- the register bank exists in firmware
- the console reads and writes registers rather than calling motion commands
  directly
- staged command registers and `CONTROL_WORD` actions can move one real axis
- status registers reflect the live controller state during motion

## `v1` Console-Driven Scope

The first implementation pass should include:

- register metadata and read/write dispatch
- `RO` identity, status, diagnostics, and input/output snapshot registers
- `RW` staged motion registers for position, velocity, acceleration, and homing
- `CONTROL_WORD` handling for outputs enable, motor enable, move, home, stop,
  zero, save config, and reload config
- `MODE_WORD`, `STATUS_WORD`, `FAULT_WORD`, and `LAST_ERROR_CODE` plumbing
- console commands for register access and inspection
- translation from register actions into the existing motion-service backend

The first implementation pass does not need to include RS485 framing or bus
transport.

## Implementation Phases

### Phase 1: Register Bank Skeleton

Implement:

- a register-bank module with address lookup and typed accessors
- storage for writable staged/configuration registers
- computed accessors for live read-only registers
- a small result/error model for invalid address, invalid write, busy, and
  range errors

Acceptance criteria:

- `reg read <addr>`
- `reg write <addr> <value>`
- `reg dump`
- live values can be inspected without crashing or corrupting motion state

### Phase 2: Control-Word Execution

Implement:

- level semantics for output-enable and motor-enable bits
- edge-triggered semantics for action bits
- staged-command snapshot into active execution state
- `ACTIVE_COMMAND_COUNTER` update on accepted commands
- `FAULT_WORD` and `LAST_ERROR_CODE` updates on rejected commands

Acceptance criteria:

- console-driven register writes can start absolute moves, relative moves,
  homing, stop, and zero through `CONTROL_WORD`
- write-one action bits are consumed once and do not retrigger continuously

### Phase 3: Console As Register Client

Implement:

- console commands that manipulate the register bank rather than
  `axis_service_submit(...)`
- readable formatting for register names, addresses, and decoded bitfields
- a compatibility layer so simple developer commands such as `status` can still
  exist as register-backed helpers

Acceptance criteria:

- the console is exercising the same API surface that RS485 and Wi-Fi will use
- motion can be tested without any direct command-only backdoor

### Phase 4: Configuration And Defaults

Implement:

- startup population of writable configuration registers with defaults
- save/reload of persistent configuration registers
- clear definition of which registers participate in persistence

Acceptance criteria:

- persistent settings survive reboot
- non-persistent live telemetry and staged command state do not get written to
  flash

### Phase 5: Auxiliary Outputs And Safety Policy

Implement:

- `FAN_LEVEL` handling, initially as on/off or fixed PWM if needed
- holding-brake sequencing under `BRAKE_HOLD_ENABLE`
- watchdog-driven controlled stop behavior
- `SYNC_STATUS_WORD` and watchdog fault bits

Acceptance criteria:

- auxiliary outputs are reflected in `OUTPUT_WORD`
- watchdog fault path drives a controlled stop policy rather than a blind power
  drop

## Explicit Deferrals

The following items are intentionally deferred from the first register-bank
milestone:

| Item | Deferred To | Reason |
| --- | --- | --- |
| RS485 framed transport | Later `v1` milestone | Register contract should be proven locally first. |
| Wi-Fi control surface | Later `v1` or `v2` milestone | Depends on the register bank being stable. |
| Queued segment FIFO | Future revision | `v1` uses a single staged command segment only. |
| Backlash compensation | Future revision | Backlash is descriptive/configurational only for now. |
| Advanced homing backoff/re-approach | Later `v1` milestone | Basic homing is enough to validate the register model first. |
| Joint-space move execution | Later `v1` milestone | Step-space motion is the fastest path to hardware validation. |
| Full fan PWM tuning | Later `v1` milestone | Not required to prove the register architecture. |
| Full brake sequencing | Later `v1` milestone | Needs policy work and hardware testing. |
| Distributed time-discipline implementation | Later `v1` milestone | Depends on RS485 transport. |

## Non-Blocking Clarifications To Capture During Implementation

These are worth deciding, but they do not block the initial register-bank
implementation:

- exact set of registers included in `save configuration`
- whether `MOVE_TO` in step space should require homing, while `MOVE_BY` does
  not
- whether soft limits are enforced only after homing in `v1`
- exact homing offset application rules
- whether `SEGMENT_DURATION_US` is ignored, validated, or lightly reported in
  the single-segment implementation

## Proposed First Firmware Slice

The first coding pass should produce:

- `axis_registers.[ch]`
- a register-aware console layer
- an adapter between register actions and `axis_service`
- enough decoded status output to verify:
  - enable state
  - homing state
  - current position
  - target position
  - velocity and acceleration limits
  - limit inputs
  - fault reporting

This is the smallest slice that validates the architecture without committing
to RS485 transport details yet.
