# Docs

This directory holds project documentation that should stay stable even as the
implementation moves around.

Current documents:

- `axis-controller/README.md`: index for the split axis-controller
  specification.
- `axis-controller/design.md`: controller behavior, synchronization, and safety
  model.
- `axis-controller/registers.md`: canonical 32-bit register definitions shared
  by all transports.
- `axis-controller/implementation-plan.md`: planned firmware rollout and
  explicitly deferred work.
- `axis-controller/rs485-framed-protocol.md`: framed RS485 wire protocol over
  the register model.
