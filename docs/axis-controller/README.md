# Axis Controller Spec

This section defines the external contract for a single Roberta axis
controller.

Read the documents in this order:

1. [Design](./design.md)
2. [Registers](./registers.md)
3. [Implementation Plan](./implementation-plan.md)
4. [WiFi Console Transport](./wifi-console.md)
5. [RS485 Framed Protocol](./rs485-framed-protocol.md) (parked)

The split is intentional:

- [Design](./design.md) describes controller behavior, timing, synchronization,
  watchdog shutdown, and overall system responsibilities.
- [Registers](./registers.md) defines the canonical 32-bit register bank used
  by all transports.
- [Implementation Plan](./implementation-plan.md) records the intended
  firmware rollout and explicit deferrals for `v1`.
- [WiFi Console Transport](./wifi-console.md) defines the current control
  path: the console over TCP, mDNS discovery, and the link watchdog.
- [RS485 Framed Protocol](./rs485-framed-protocol.md) defines the wire format
  used to carry register-oriented operations over RS485. It is parked in
  favour of devolved WiFi control.
