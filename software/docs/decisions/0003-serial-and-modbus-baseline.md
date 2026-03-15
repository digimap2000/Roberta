# Serial And Modbus Baseline

## Context

The host-side `software/` slice needs:

- direct USB CDC access to the current single-axis firmware for development
- a deployable RS485 transport that does not invent a bespoke wire protocol
- a seam that keeps register packing and transport compromises below the axis-driver boundary

`/opt/dts` provides useful generic support but does not provide Modbus RTU. No installed
Modbus dependency was available in the current environment during implementation.

## Decision

- Use standard POSIX serial APIs for the real serial transport adapters.
- Use a small local Modbus RTU codec in the protocol layer.
- Treat Modbus RTU as a transport/protocol implementation detail only.
- Keep the logical controller register model 32-bit.
- Map each logical 32-bit register to two 16-bit Modbus holding registers at
  `logical_address * 2`, with low word first.
- Keep the current USB CDC console path available through a text register transport
  so the existing firmware can be exercised before RS485 firmware exists.

## Alternatives considered

- `libmodbus`
  Not adopted in this slice because it was not installed in the active environment.
- Bespoke framed RS485 protocol
  Rejected. This violates the project rule to avoid reinventing line protocol
  mechanics unless forced.
- DTS-owned serial/Modbus stack
  DTS inventory did not show a Modbus RTU implementation.

## Consequences

- The Modbus codec is intentionally narrow and replaceable.
- Real hardware dogfooding can start immediately over the console register path.
- A later switch to `libmodbus` or a different transport only affects transport/protocol adapters.
