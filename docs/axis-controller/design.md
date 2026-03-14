# Axis Controller Design

This document describes the intended external behavior of a single Roberta axis
controller.

## Status

This is `v1` of the documented controller contract.

## Scope

Each physical controller owns one robot axis, but it still needs enough
metadata and scaling information to participate in a six-axis robot.

The controller is responsible for:

- accepting staged command parameters
- atomically snapshotting them when commanded
- executing local motion planning and step generation
- starting execution at a requested future tick
- reporting status and progress

The controller is not responsible for whole-robot kinematics or path planning.
A higher-level coordinator should decompose robot-space paths into per-axis
commands and schedule those commands against a shared notion of time.

## Conventions

- registers are 32-bit
- register addresses are 16-bit
- all transports map onto the same logical register bank
- RS485 uses a framed packet protocol over that register bank
- joint-space angular values use signed `Q12.20` degrees unless otherwise
  stated

## Staged Command Model

External control should generally follow this sequence:

1. Write staged target and motion-limit registers.
2. Write any staged auxiliary outputs such as fan level if needed.
3. Write `STAGED_COMMAND_COUNTER`.
4. Optionally write `EXECUTE_AT_TICK_US` for a synchronized future start.
5. Set one or more action bits in `CONTROL_WORD`.
6. Firmware atomically snapshots the staged values into active command state.
7. Firmware copies `STAGED_COMMAND_COUNTER` into `ACTIVE_COMMAND_COUNTER`.
8. Firmware executes immediately or waits for the requested tick.
9. Host monitors `STATUS_WORD`, active command state, and telemetry.

This avoids partial-update races without needing `BEGIN/END/ABORT`
transactions.

## Segment Model

In `v1`, the controller exposes one staged command segment at a time. There is
no queued segment FIFO in this revision.

Future revisions may extend this with queued, time-stamped segments, but the
current contract assumes a single active staged bundle.

## Configuration Persistence

`Save configuration` persists the current writable configuration registers
directly to non-volatile storage. There are no separate persistent
configuration pages in `v1`.

## Coordinated Multi-Axis Motion

For synchronized robot motion, the higher-level robot controller should:

1. Plan a trajectory in robot space.
2. Convert that into per-axis targets, velocities, and accelerations.
3. Write a staged command bundle to each axis controller.
4. Use the same `EXECUTE_AT_TICK_US` on every axis involved.
5. Arm all axes.
6. Let each axis begin from its local command snapshot at the same scheduled
   time.

That gives deterministic start synchronization without forcing each axis
controller to understand whole-robot kinematics.

## Distributed Sync

This hardware revision has no dedicated sync line. For private testing, the
controller therefore relies on RS485 time distribution.

The intended `v1` synchronization model is:

1. A higher-level coordinator broadcasts a time packet over RS485 at `10 Hz`.
2. Each axis controller maintains a disciplined local execution tick.
3. Motion commands are staged locally and armed with `EXECUTE_AT_TICK_US`.
4. Each axis starts the move from its own local timer, not from packet
   arrival.

This is not a hard real-time distributed clock, but it is expected to be
adequate for coordinated private-test motion on this revision.

## Communications Watchdog

This hardware revision has no hardware emergency-stop input. Loss of the
expected time-sync stream is therefore treated as a stop condition for private
testing.

The `v1` watchdog policy is:

- expected sync cadence: `10 Hz`
- watchdog timeout: `100 ms`
- watchdog action: controlled shutdown

Controlled shutdown means:

1. stop accepting new motion starts
2. command a controlled deceleration to stop
3. keep drive output active during that deceleration
4. once stationary, disable motor drive output as configured
5. apply the holding brake if present and safe to do so
6. if controlled stop fails or times out, escalate to hard output disable
7. set a communications-loss or watchdog fault bit

This is suitable for private testing only. It must not be treated as a
functional-safety e-stop.

## Future Hardware

`v2` hardware is expected to add:

- dedicated synchronization support
- dual redundant stop lines

When that exists, the communications watchdog should remain as an operational
supervision feature, but hard stop behavior should move to hardware.
