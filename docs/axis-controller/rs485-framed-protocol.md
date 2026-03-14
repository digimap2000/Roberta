# Axis Controller RS485 Framed Protocol

This document defines the `v1` RS485 wire protocol used to carry register-
oriented operations for a single Roberta axis controller.

The register bank remains the canonical external data model. RS485 adds
framing, addressing, batching, sequencing, and broadcast support.

## Design Goals

- carry reads and writes against the shared 32-bit register bank
- support explicit node addressing and bus-wide broadcast
- support efficient batched staged-command writes
- support periodic broadcast time synchronization
- provide CRC protection and request correlation
- leave room for future queued-segment or diagnostic extensions

## Link Assumptions

- half-duplex RS485 multi-drop bus
- one higher-level coordinator normally acts as bus master
- axis controllers are individually addressed slaves
- broadcast is used for time synchronization and, where safe, simultaneous arm
  or stop requests

## Value Encoding

- multi-byte integer fields are little-endian
- register addresses are 16-bit
- register values are 32-bit
- signed register values use two's complement

## Frame Format

| Field | Size | Description |
| --- | --- | --- |
| `SOF` | `1 byte` | Start-of-frame marker, initial value `0xA5` |
| `VERSION` | `1 byte` | Framing version, initial value `1` |
| `FLAGS` | `1 byte` | Per-frame behavior flags |
| `DEST` | `1 byte` | Destination node ID, `0xFF` for broadcast |
| `SRC` | `1 byte` | Source node ID |
| `OPCODE` | `1 byte` | Operation type |
| `SEQ` | `1 byte` | Request sequence or correlation ID |
| `PAYLOAD_LEN` | `2 bytes` | Payload length in bytes |
| `PAYLOAD` | `N bytes` | Operation-specific payload |
| `CRC16` | `2 bytes` | CRC over all prior frame bytes except `SOF` handling rules in implementation |

`AXIS_ID` remains the logical robot axis identity. `DEST` is the bus address.
These may be the same in simple deployments, but they are not required to be.

## Flags

| Bit | Meaning |
| --- | --- |
| `0` | Ack requested |
| `1` | Response frame |
| `2` | Error frame |
| `3` | Broadcast frame |
| `4..7` | Reserved |

## Opcodes

| Opcode | Name | Description |
| --- | --- | --- |
| `0x01` | `READ_REGISTERS` | Read one or more 32-bit registers |
| `0x02` | `WRITE_REGISTERS` | Write one or more 32-bit registers |
| `0x03` | `WRITE_AND_ARM` | Write staged registers, then apply control bits atomically |
| `0x04` | `READ_STATUS_SNAPSHOT` | Return compact status without a multi-register read round trip |
| `0x05` | `SYNC_TIME` | Broadcast coordinator time information |
| `0x06` | `PING` | Liveness and addressing check |
| `0x7F` | `ERROR` | Explicit protocol-level error response |

## Payloads

### `READ_REGISTERS`

Request payload:

| Field | Size | Description |
| --- | --- | --- |
| `START_ADDR` | `2 bytes` | First register address |
| `COUNT` | `2 bytes` | Number of consecutive 32-bit registers to read |

Response payload:

| Field | Size | Description |
| --- | --- | --- |
| `START_ADDR` | `2 bytes` | First returned register address |
| `COUNT` | `2 bytes` | Number of returned registers |
| `VALUES` | `COUNT * 4 bytes` | Consecutive register values |

### `WRITE_REGISTERS`

Request payload:

| Field | Size | Description |
| --- | --- | --- |
| `START_ADDR` | `2 bytes` | First register address |
| `COUNT` | `2 bytes` | Number of consecutive 32-bit registers to write |
| `VALUES` | `COUNT * 4 bytes` | Consecutive register values |

Response payload:

| Field | Size | Description |
| --- | --- | --- |
| `START_ADDR` | `2 bytes` | First written register address |
| `COUNT` | `2 bytes` | Number of written registers |

### `WRITE_AND_ARM`

This opcode exists to reduce bus traffic and eliminate races between staged
writes and the final control-word action.

Request payload:

| Field | Size | Description |
| --- | --- | --- |
| `START_ADDR` | `2 bytes` | First staged register address |
| `COUNT` | `2 bytes` | Number of consecutive staged registers included |
| `VALUES` | `COUNT * 4 bytes` | Consecutive staged register values |
| `CONTROL_SET_MASK` | `4 bytes` | Action bits to set in `CONTROL_WORD` after staged values are committed |

The target controller should:

1. validate the write set
2. apply the staged register values
3. atomically snapshot staged values into active command state if the control
   mask contains motion action bits
4. update `ACTIVE_COMMAND_COUNTER`
5. begin execution immediately or wait for `EXECUTE_AT_TICK_US`

Response payload:

| Field | Size | Description |
| --- | --- | --- |
| `ACTIVE_COMMAND_COUNTER` | `4 bytes` | Accepted command counter after commit |
| `STATUS_WORD` | `4 bytes` | Post-commit status snapshot |

### `READ_STATUS_SNAPSHOT`

Response payload:

| Field | Size | Description |
| --- | --- | --- |
| `STATUS_WORD` | `4 bytes` | Main controller status bits |
| `FAULT_WORD` | `4 bytes` | Fault bits |
| `CURRENT_POSITION_STEPS` | `4 bytes` | Current motor-space position |
| `ACTIVE_TARGET_POSITION_STEPS` | `4 bytes` | Active target position |
| `CURRENT_VELOCITY_STEPS_S` | `4 bytes` | Current velocity estimate |
| `LOCAL_TICK_US` | `4 bytes` | Local disciplined tick |

### `SYNC_TIME`

`SYNC_TIME` is normally broadcast to `DEST = 0xFF`.

Payload:

| Field | Size | Description |
| --- | --- | --- |
| `MASTER_TICK_US` | `4 bytes` | Coordinator reference tick |
| `EFFECTIVE_AT_TICK_US` | `4 bytes` | Optional future local epoch for slewed application, or `0` for immediate discipline |
| `SYNC_COUNTER` | `4 bytes` | Monotonic counter for sync freshness |

Receivers should update their local disciplined clock state and refresh the
communications watchdog when a valid sync packet is accepted.

### `PING`

Request payload may be empty.

Response payload:

| Field | Size | Description |
| --- | --- | --- |
| `DEVICE_TYPE` | `4 bytes` | Device type register value |
| `AXIS_ID` | `4 bytes` | Logical axis identifier |
| `PROTOCOL_VERSION` | `4 bytes` | Register-map version |

## Error Handling

An error response should set:

- `FLAGS.bit1 = 1` for response
- `FLAGS.bit2 = 1` for error
- `OPCODE = 0x7F`

Error payload:

| Field | Size | Description |
| --- | --- | --- |
| `FAILED_OPCODE` | `1 byte` | Opcode that failed |
| `ERROR_CODE` | `1 byte` | Protocol error code |
| `DETAIL` | `2 bytes` | Optional register address or reason-specific detail |

Suggested protocol-level error codes:

| Code | Meaning |
| --- | --- |
| `1` | Unknown opcode |
| `2` | Invalid payload length |
| `3` | Invalid register address |
| `4` | Invalid register count |
| `5` | Write not permitted |
| `6` | CRC failure |
| `7` | Busy or not ready |

Register-level or motion-level command rejection should still surface through
`FAULT_WORD`, `STATUS_WORD`, and `LAST_ERROR_CODE`.

## Broadcast Behavior

- `SYNC_TIME` is expected to use broadcast addressing
- broadcast writes should be limited to operations that are safe to execute
  without per-node acknowledgement
- normal configuration writes should use unicast
- simultaneous motion should usually be achieved by per-axis staged writes plus
  a shared `EXECUTE_AT_TICK_US`, not by relying on identical packet arrival
  time

## Relationship To The Register Map

The RS485 protocol does not replace the register model.

Instead:

- the register map remains the canonical external contract
- USB console and Wi-Fi should conceptually manipulate the same registers
- RS485 frames provide a compact and reliable way to read, write, and arm
  register-backed commands

## `v1` Boundaries

The initial framed protocol does not yet define:

- queued motion-segment streaming
- bulk flash configuration transfer
- firmware update transport
- safety-rated emergency-stop behavior
