# Axis Controller Spec

This section defines the external contract for a single Roberta axis
controller.

Read the documents in this order:

1. [Design](./design.md)
2. [Registers](./registers.md)
3. [RS485 Framed Protocol](./rs485-framed-protocol.md)

The split is intentional:

- [Design](./design.md) describes controller behavior, timing, synchronization,
  watchdog shutdown, and overall system responsibilities.
- [Registers](./registers.md) defines the canonical 32-bit register bank used
  by all transports.
- [RS485 Framed Protocol](./rs485-framed-protocol.md) defines the wire format
  used to carry register-oriented operations over RS485.
