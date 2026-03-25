# Axis Controller Registers

This document defines the canonical external register bank for a single
Roberta axis controller.

## Register Model

- register addresses are 16-bit
- register values are 32-bit
- signed integers use two's complement
- bit numbering uses `bit 0` as the least-significant bit
- positions are expressed in motor steps unless otherwise stated
- joint-space angular registers use signed `Q12.20` degrees unless otherwise
  stated
- velocity is expressed in steps per second
- acceleration is expressed in steps per second squared
- mechanical ratios are represented as integer numerator and denominator
  registers
- registers marked `RW` are readable and writable
- registers marked `RO` are read-only status or identity values
- action bits are write-one command bits unless noted

## Fixed-Point Angular Format

Joint-space angle registers use signed `Q12.20` degrees stored in `i32`.

| Quantity | Value |
| --- | --- |
| Integer range | approximately `-2048` to `+2048` degrees |
| Resolution | `1 / 2^20` degrees |
| Resolution in degrees | approximately `0.00000095` degrees |
| `1.0` degree | `1048576` |
| `360.0` degrees | `377487360` |

## Register Summary

| Address | Name | Type | Access | Description |
| --- | --- | --- | --- | --- |
| `0x0000` | `PROTOCOL_VERSION` | `u32` | `RO` | Register map version. Initial value `1`. |
| `0x0001` | `DEVICE_TYPE` | `u32` | `RO` | Device type. Initial value `1` for axis controller. |
| `0x0002` | `AXIS_ID` | `u32` | `RW` | Logical robot axis identifier. |
| `0x0003` | `CAPABILITY_WORD` | `u32` | `RO` | Bitfield of supported features. |
| `0x0010` | `CONTROL_WORD` | `u32` | `RW` | Main command and state-request bits. |
| `0x0011` | `STATUS_WORD` | `u32` | `RO` | Main controller state bits. |
| `0x0012` | `FAULT_WORD` | `u32` | `RO` | Fault flags. |
| `0x0013` | `MODE_WORD` | `u32` | `RW` | Motion/control mode selection. |
| `0x0014` | `INPUT_WORD` | `u32` | `RO` | Snapshot of home and limit inputs. |
| `0x0015` | `OUTPUT_WORD` | `u32` | `RO` | Snapshot of output states. |
| `0x0016` | `STAGED_COMMAND_COUNTER` | `u32` | `RW` | Host-assigned sequence number for the staged command bundle. |
| `0x0017` | `ACTIVE_COMMAND_COUNTER` | `u32` | `RO` | Last staged command sequence number accepted by the controller. |
| `0x0018` | `EXECUTE_AT_TICK_US` | `u32` | `RW` | Scheduled local execution tick in microseconds. |
| `0x0019` | `SEGMENT_DURATION_US` | `u32` | `RW` | Optional nominal duration for the single staged command segment in `v1`. |
| `0x0020` | `STAGED_TARGET_POSITION_STEPS` | `i32` | `RW` | Staged absolute motor-space target position. |
| `0x0021` | `ACTIVE_TARGET_POSITION_STEPS` | `i32` | `RO` | Active target currently being executed. |
| `0x0022` | `CURRENT_POSITION_STEPS` | `i32` | `RO` | Current inferred motor-space position. |
| `0x0023` | `STAGED_TARGET_VELOCITY_STEPS_S` | `u32` | `RW` | Staged velocity limit for the next command. |
| `0x0024` | `ACTIVE_TARGET_VELOCITY_STEPS_S` | `u32` | `RO` | Velocity limit of the active command. |
| `0x0025` | `CURRENT_VELOCITY_STEPS_S` | `u32` | `RO` | Current motion estimate. |
| `0x0026` | `STAGED_TARGET_ACCEL_STEPS_S2` | `u32` | `RW` | Staged acceleration limit for the next command. |
| `0x0027` | `ACTIVE_TARGET_ACCEL_STEPS_S2` | `u32` | `RO` | Acceleration limit of the active command. |
| `0x0028` | `STAGED_HOME_VELOCITY_STEPS_S` | `u32` | `RW` | Staged homing seek velocity. |
| `0x0029` | `ACTIVE_HOME_VELOCITY_STEPS_S` | `u32` | `RO` | Active homing seek velocity. |
| `0x002A` | `HOME_OFFSET_STEPS` | `i32` | `RW` | Position assigned after home is found. |
| `0x002B` | `STAGED_RELATIVE_MOVE_STEPS` | `i32` | `RW` | Relative move distance used for the next relative move command. |
| `0x0030` | `FAN_LEVEL` | `u32` | `RW` | Fan drive command from `0` to `1000`. |
| `0x0031` | `BRAKE_HOLD_ENABLE` | `u32` | `RW` | Enable automatic holding-brake sequencing for this axis. |
| `0x0032` | `BRAKE_RELEASE_DELAY_MS` | `u32` | `RW` | Delay between brake release and motion start. |
| `0x0033` | `BRAKE_APPLY_DELAY_MS` | `u32` | `RW` | Delay between motion stop and brake apply. |
| `0x0040` | `STEPS_PER_MOTOR_REV` | `u32` | `RW` | Effective motor steps per motor revolution. |
| `0x0041` | `MOTOR_REV_PER_JOINT_REV_NUMERATOR` | `u32` | `RW` | Motor revolutions per joint revolution numerator. |
| `0x0042` | `MOTOR_REV_PER_JOINT_REV_DENOMINATOR` | `u32` | `RW` | Motor revolutions per joint revolution denominator. |
| `0x0043` | `JOINT_DEGREES_PER_REV_Q12_20` | `i32` | `RW` | Joint degrees per joint revolution in `Q12.20`. Normally `360.0 deg`. |
| `0x0044` | `JOINT_BACKLASH_DEG_Q12_20` | `i32` | `RW` | Estimated mechanical backlash in `Q12.20` degrees. |
| `0x0045` | `SOFT_LIMIT_MIN_STEPS` | `i32` | `RW` | Minimum travel limit in motor steps. |
| `0x0046` | `SOFT_LIMIT_MAX_STEPS` | `i32` | `RW` | Maximum travel limit in motor steps. |
| `0x0047` | `JOINT_POSITION_DEG_Q12_20` | `i32` | `RO` | Current inferred joint position in `Q12.20` degrees. |
| `0x0048` | `STAGED_JOINT_TARGET_DEG_Q12_20` | `i32` | `RW` | Staged joint target in `Q12.20` degrees. |
| `0x0049` | `ACTIVE_JOINT_TARGET_DEG_Q12_20` | `i32` | `RO` | Active joint target being executed in `Q12.20` degrees. |
| `0x0050` | `STEP_COUNT` | `u32` | `RO` | Emitted step count since boot or reset. |
| `0x0051` | `UPTIME_MS` | `u32` | `RO` | Milliseconds since boot. |
| `0x0052` | `LOCAL_TICK_US` | `u32` | `RO` | Current local execution tick in microseconds. |
| `0x0053` | `LAST_ERROR_CODE` | `u32` | `RO` | Last nonzero fault or rejection code. |
| `0x0054` | `LAST_SYNC_RX_TICK_US` | `u32` | `RO` | Local tick when the last sync packet was accepted. |
| `0x0055` | `SYNC_WATCHDOG_TIMEOUT_MS` | `u32` | `RW` | Timeout for sync-loss shutdown. Default `100`. |
| `0x0056` | `SYNC_STATUS_WORD` | `u32` | `RO` | Distributed time-sync state bits. |

## Identity And Capability

| Register | Type | Access | Description |
| --- | --- | --- | --- |
| `PROTOCOL_VERSION` | `u32` | `RO` | Initial value `1`. Increment when the logical register contract changes incompatibly. |
| `DEVICE_TYPE` | `u32` | `RO` | Initial value `1`. Reserved type code for a single-axis stepper controller. |
| `AXIS_ID` | `u32` | `RW` | Logical axis identity in the six-axis robot. Suggested values: `0` unassigned, `1..6` robot axis number. |
| `CAPABILITY_WORD` | `u32` | `RO` | Bitfield describing supported features. |

### `CAPABILITY_WORD` Bits

| Bit | Meaning |
| --- | --- |
| `0` | Absolute position mode supported |
| `1` | Relative move supported |
| `2` | Homing supported |
| `3` | Software limits supported |
| `4` | Fan output supported |
| `5` | Holding brake supported |
| `6` | Joint-space degree representation supported |
| `7` | Persistent configuration supported |

## Command And State Registers

| Register | Type | Access | Description |
| --- | --- | --- | --- |
| `CONTROL_WORD` | `u32` | `RW` | Main command and state-request bits. Bits `2..11` are write-one action bits. Bits `0..1` remain level-representative. Action bits consume staged registers atomically. |
| `STATUS_WORD` | `u32` | `RO` | Main controller state bits. |
| `FAULT_WORD` | `u32` | `RO` | Fault and rejection bits. |
| `MODE_WORD` | `u32` | `RW` | Motion/control mode selection. |
| `INPUT_WORD` | `u32` | `RO` | Snapshot of home and limit inputs. |
| `OUTPUT_WORD` | `u32` | `RO` | Snapshot of output states. |

### `CONTROL_WORD` Bits

| Bit | Meaning |
| --- | --- |
| `0` | Request output enable |
| `1` | Request motor enable |
| `2` | Start absolute move using `STAGED_TARGET_POSITION_STEPS` |
| `3` | Start relative move using `STAGED_RELATIVE_MOVE_STEPS` |
| `4` | Start homing |
| `5` | Stop motion |
| `6` | Emergency stop |
| `7` | Clear fault |
| `8` | Set current position to zero |
| `9` | Apply `STAGED_JOINT_TARGET_DEG_Q12_20` by converting into step space |
| `10` | Persist current writable configuration registers to non-volatile storage |
| `11` | Reload saved configuration into the live writable configuration registers |

### `STATUS_WORD` Bits

| Bit | Meaning |
| --- | --- |
| `0` | Outputs enabled |
| `1` | Motor enabled |
| `2` | Moving |
| `3` | Homing |
| `4` | Homed |
| `5` | At target |
| `6` | Fault active |
| `7` | Home input active |
| `8` | Limit A active |
| `9` | Limit B active |
| `10` | Software limit active |
| `11` | Brake currently applied |
| `12` | Brake released for motion |
| `13` | Fan output active |

### `FAULT_WORD` Bits

| Bit | Meaning |
| --- | --- |
| `0` | Invalid command |
| `1` | Motion requested while disabled |
| `2` | Move requested before homing |
| `3` | Software limit violation |
| `4` | Unexpected limit hit |
| `5` | Homing timeout |
| `6` | Brake interlock violation |
| `7` | Motion aborted |

### `MODE_WORD` Values

| Value | Meaning |
| --- | --- |
| `0` | Idle |
| `1` | Step-space position mode |
| `2` | Homing mode |
| `3` | Joint-space position mode |
| `4` | Reserved for velocity mode |
| `5` | Reserved for jog mode |

### `INPUT_WORD` Bits

| Bit | Meaning |
| --- | --- |
| `0` | Home input active |
| `1` | Limit A active |
| `2` | Limit B active |

### `OUTPUT_WORD` Bits

| Bit | Meaning |
| --- | --- |
| `0` | Output buffer enabled |
| `1` | Motor enabled |
| `2` | Direction positive |
| `3` | Step output currently active |
| `4` | Brake output active |
| `5` | Fan output active |

## Motion Registers

| Register | Type | Access | Description |
| --- | --- | --- | --- |
| `STAGED_COMMAND_COUNTER` | `u32` | `RW` | Sequence number written by the host for the next staged command bundle. |
| `ACTIVE_COMMAND_COUNTER` | `u32` | `RO` | Last staged command sequence number accepted by the controller. |
| `EXECUTE_AT_TICK_US` | `u32` | `RW` | Local execution tick for the staged command. If zero or in the past, firmware may execute immediately. |
| `SEGMENT_DURATION_US` | `u32` | `RW` | Optional nominal duration for the single staged command segment in `v1`. Can be ignored for simple point-to-point moves. |
| `STAGED_TARGET_POSITION_STEPS` | `i32` | `RW` | Absolute target for the next staged command in motor steps. |
| `ACTIVE_TARGET_POSITION_STEPS` | `i32` | `RO` | Absolute target currently being executed. |
| `CURRENT_POSITION_STEPS` | `i32` | `RO` | Current inferred motor position in steps. |
| `STAGED_TARGET_VELOCITY_STEPS_S` | `u32` | `RW` | Velocity limit for the next staged command. |
| `ACTIVE_TARGET_VELOCITY_STEPS_S` | `u32` | `RO` | Velocity limit of the active command. |
| `CURRENT_VELOCITY_STEPS_S` | `u32` | `RO` | Current estimated step-space velocity magnitude. |
| `STAGED_TARGET_ACCEL_STEPS_S2` | `u32` | `RW` | Acceleration limit for the next staged command. |
| `ACTIVE_TARGET_ACCEL_STEPS_S2` | `u32` | `RO` | Acceleration limit of the active command. |
| `STAGED_HOME_VELOCITY_STEPS_S` | `u32` | `RW` | Seek velocity used for the next homing command. |
| `ACTIVE_HOME_VELOCITY_STEPS_S` | `u32` | `RO` | Seek velocity of the active homing command. |
| `HOME_OFFSET_STEPS` | `i32` | `RW` | Position assigned after a successful home event. |
| `STAGED_RELATIVE_MOVE_STEPS` | `i32` | `RW` | Relative move distance used when `CONTROL_WORD.bit3` snapshots a staged relative move command. |

## Auxiliary Outputs

| Register | Type | Access | Description |
| --- | --- | --- | --- |
| `FAN_LEVEL` | `u32` | `RW` | Fan drive command from `0` to `1000`. Intermediate values are intended for PWM duty-cycle control. |
| `BRAKE_HOLD_ENABLE` | `u32` | `RW` | Enable automatic holding-brake sequencing for this axis. `0` means brake logic disabled, `1` means the controller owns brake release/apply timing around motion. |
| `BRAKE_RELEASE_DELAY_MS` | `u32` | `RW` | Delay between brake release and the first motion step. |
| `BRAKE_APPLY_DELAY_MS` | `u32` | `RW` | Delay between motion completion and brake application. |

## Scaling And Axis Configuration

| Register | Type | Access | Description |
| --- | --- | --- | --- |
| `STEPS_PER_MOTOR_REV` | `u32` | `RW` | Effective motor steps per motor revolution after microstepping. Examples: `200` full-step, `1600` for `1/8` microstepping. |
| `MOTOR_REV_PER_JOINT_REV_NUMERATOR` | `u32` | `RW` | Motor revolutions per joint revolution numerator. |
| `MOTOR_REV_PER_JOINT_REV_DENOMINATOR` | `u32` | `RW` | Motor revolutions per joint revolution denominator. Together with `STEPS_PER_MOTOR_REV`, this converts motor steps to joint revolutions. |
| `JOINT_DEGREES_PER_REV_Q12_20` | `i32` | `RW` | Joint degrees per joint revolution in `Q12.20`. Normally set to `360.0 deg`. |
| `JOINT_BACKLASH_DEG_Q12_20` | `i32` | `RW` | Estimated mechanical backlash in `Q12.20` degrees. Initially descriptive/configurational. |
| `SOFT_LIMIT_MIN_STEPS` | `i32` | `RW` | Minimum allowed motor-space position. |
| `SOFT_LIMIT_MAX_STEPS` | `i32` | `RW` | Maximum allowed motor-space position. |
| `JOINT_POSITION_DEG_Q12_20` | `i32` | `RO` | Current inferred joint position in `Q12.20` degrees after mechanical conversion. |
| `STAGED_JOINT_TARGET_DEG_Q12_20` | `i32` | `RW` | Desired joint-space target in `Q12.20` degrees for the next staged command. |
| `ACTIVE_JOINT_TARGET_DEG_Q12_20` | `i32` | `RO` | Active joint-space target currently being executed in `Q12.20` degrees. |

## Diagnostics

| Register | Type | Access | Description |
| --- | --- | --- | --- |
| `STEP_COUNT` | `u32` | `RO` | Total emitted step pulses since boot or reset. |
| `UPTIME_MS` | `u32` | `RO` | Milliseconds since boot. |
| `LOCAL_TICK_US` | `u32` | `RO` | Current local execution tick. This is the clock that `EXECUTE_AT_TICK_US` references. |
| `LAST_ERROR_CODE` | `u32` | `RO` | Last nonzero fault or rejection code. |
| `LAST_SYNC_RX_TICK_US` | `u32` | `RO` | Local execution tick when the last valid time-sync packet was accepted. |
| `SYNC_WATCHDOG_TIMEOUT_MS` | `u32` | `RW` | Communications-watchdog timeout used to detect loss of the expected sync/time packet stream. Suggested default `100`. |
| `SYNC_STATUS_WORD` | `u32` | `RO` | Distributed time-sync state bits. |

### `SYNC_STATUS_WORD` Bits

| Bit | Meaning |
| --- | --- |
| `0` | Sync stream currently healthy |
| `1` | Local clock currently disciplined |
| `2` | Sync watchdog fault active |
| `3` | Execution is waiting for a future `EXECUTE_AT_TICK_US` |

## Expected Defaults

| Register | Suggested Default |
| --- | --- |
| `AXIS_ID` | `0` |
| `STAGED_TARGET_VELOCITY_STEPS_S` | `400` |
| `STAGED_TARGET_ACCEL_STEPS_S2` | `800` |
| `STAGED_HOME_VELOCITY_STEPS_S` | `150` |
| `HOME_OFFSET_STEPS` | `0` |
| `FAN_LEVEL` | `0` |
| `BRAKE_HOLD_ENABLE` | `1` if the axis has a holding brake that should be sequenced automatically |
| `SYNC_WATCHDOG_TIMEOUT_MS` | `100` |
| `STEPS_PER_MOTOR_REV` | `200` |
| `MOTOR_REV_PER_JOINT_REV_NUMERATOR` | `1` |
| `MOTOR_REV_PER_JOINT_REV_DENOMINATOR` | `1` |
| `JOINT_DEGREES_PER_REV_Q12_20` | `360.0 deg` encoded as `377487360` |
| `JOINT_BACKLASH_DEG_Q12_20` | `0` until characterized |

## Console Mapping

| Console Command | Register Mapping |
| --- | --- |
| `enable on|off` | `CONTROL_WORD.bit1` |
| `outputs on|off` | `CONTROL_WORD.bit0` |
| `zero` | `CONTROL_WORD.bit8` |
| `home` | `CONTROL_WORD.bit4` |
| `goto N` | Write `STAGED_TARGET_POSITION_STEPS`, then set `CONTROL_WORD.bit2` |
| `move N` | Write `STAGED_RELATIVE_MOVE_STEPS`, then set `CONTROL_WORD.bit3` |
| `vel N` | Write `STAGED_TARGET_VELOCITY_STEPS_S` |
| `accel N` | Write `STAGED_TARGET_ACCEL_STEPS_S2` |
| Future `gotoj N` | Write `STAGED_JOINT_TARGET_DEG_Q12_20`, then set `CONTROL_WORD.bit9` |
| Future `fan N` | Write `FAN_LEVEL` |
| Future `brake auto on|off` | Write `BRAKE_HOLD_ENABLE` |
| `status` | Read `STATUS_WORD` and associated telemetry |
