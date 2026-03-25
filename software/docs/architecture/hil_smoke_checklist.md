# HIL Smoke Checklist

This checklist is for the first host-to-controller vertical slice.

## Preconditions

- Axis controller firmware with USB console register API is flashed.
- Host has a visible serial device for the controller.
- Axis is safe to energize and move without load damage.

## Smoke path

1. Open the USB CDC console transport.
2. Read `STATUS_WORD`, `FAULT_WORD`, and `CURRENT_POSITION_STEPS`.
3. Enable outputs and motor through the typed axis device.
4. Read back state and confirm the axis transitions to a ready state.
5. Issue a small relative move through the axis driver.
6. Poll snapshots until motion completes.
7. Confirm the reported position changed and faults stayed clear.
8. Issue `stop`.
9. Disconnect cleanly.

## Notes

This uses the console register transport as the current hardware bridge. The
same typed axis/service layers are intended to sit above Modbus RTU once the
controller firmware exposes that protocol.
