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
- `axis-controller/rs485-framed-protocol.md`: framed RS485 wire protocol over
  the register model.
- `axis-controller-register-map.md`: compatibility entry point that links to
  the split documents.
