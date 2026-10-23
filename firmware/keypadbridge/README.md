# Keypad Bridge Firmware

Firmware target for a Raspberry Pi Pico 2 acting as a keypad slave. The initial
implementation scans eight direct GPIO button inputs, exposes their state over
I2C slave mode, and drives an attention output when the debounced key state
changes.

## Power Profile

- USB stdio is enabled by default for bring-up, so debounced button transitions
  are printed to the Pico 2 console.
- The RP2350 system clock is reduced to `12 MHz`.
- Button detection is interrupt-driven, so the CPU sleeps until either a button
  edge, an I2C transaction, or a debounce deadline wakes it.
- The firmware does not use dormant mode because the Pico must remain awake
  enough to answer as an I2C slave.

## Assumptions

- Each button closes its GPIO input to a shared common line tied to ground.
- Pico internal pull-ups hold the inputs high when idle, so buttons are
  interpreted as active-low.
- The attention output is active-low by default.
- Pin assignments are placeholders and live in `config/board_config.h`.

## Default Pin Map

- Button common: `GND` on physical pin `13`
- Buttons: `GP10` through `GP17`
- Attention: `GP18`
- I2C slave SDA: `GP20`
- I2C slave SCL: `GP21`
- I2C address: `0x20`

For the current pendant bring-up, wire the nine-way button tail as:

| Pendant | Pico GPIO | Pico physical pin |
| --- | --- | --- |
| Common | `GND` | `13` |
| Button 0 | `GP10` | `14` |
| Button 1 | `GP11` | `15` |
| Button 2 | `GP12` | `16` |
| Button 3 | `GP13` | `17` |
| Button 4 | `GP14` | `19` |
| Button 5 | `GP15` | `20` |
| Button 6 | `GP16` | `22` |
| Button 7 | `GP17` | `23` |

## Register Map

Reads use a simple register pointer scheme: write one byte to select the
register, then issue an I2C read.

| Address | Name | Description |
| --- | --- | --- |
| `0x00` | `PROTOCOL_VERSION` | Protocol version, currently `1` |
| `0x01` | `BUTTON_COUNT` | Number of scanned buttons, currently `8` |
| `0x02` | `BUTTON_STATE` | Bitmask of debounced button state, `1 = pressed` |
| `0x03` | `CHANGED_STATE` | Bitmask of buttons changed since last acknowledge |
| `0x04` | `STATUS` | Bit 0 = attention asserted, bit 1 = changes pending |
| `0x05` | `EVENT_COUNTER` | Increments on each debounced state change |
| `0x10` | `CONTROL` | Write bit 0 to clear `CHANGED_STATE` and release attention |

## Build

```sh
cmake -S firmware/keypadbridge -B build/firmware/keypadbridge -G Ninja
cmake --build build/firmware/keypadbridge
```

The project defaults to the SDK installed at `/opt/pico/pico-sdk`. Override
`KEYPADBRIDGE_PICO_SDK_PATH` at configure time if needed.

To restore the low-power I2C-only image, configure with
`-DKEYPADBRIDGE_USB_CONSOLE=OFF`.
