# Axis Controller Firmware

Single-axis step/direction controller for the Axis v1 board (Raspberry Pi
Pico 2 W). It exposes the register bank from
[docs/axis-controller](../../docs/axis-controller/README.md) through a text
console over USB and over WiFi. Over WiFi it is
[TCP port 7300 with mDNS discovery](../../docs/axis-controller/wifi-console.md).

## Build

```sh
PICO_SDK_PATH=/opt/pico/pico-sdk cmake -S firmware/axiscontroller -B build/firmware/axiscontroller -G Ninja
cmake --build build/firmware/axiscontroller
```

The image is `build/firmware/axiscontroller/axiscontroller.uf2`. The WiFi
regulatory domain defaults to the UK. Override it with
`-DCMAKE_C_FLAGS=-DAXIS_NET_COUNTRY=CYW43_COUNTRY_...` if needed.

## Flash

Hold BOOTSEL while plugging in USB, then copy the `.uf2` to the `RP2350` drive,
or:

```sh
/opt/pico/picotool load -f -x build/firmware/axiscontroller/axiscontroller.uf2
```

## Pin Map

| Function | GPIO | Notes |
| --- | --- | --- |
| Step (PUL) | `GP7` | active low, 20 µs pulse |
| Direction | `GP8` | high = positive |
| Drive enable (ENA) | `GP9` | active low |
| Output enable (OE) | `GP6` | active low, level-shifter enable |
| Fan | `GP4` | active high |
| Brake | `GP5` | active high |
| Home switch | `GP15` | to GND, internal pull-up |
| Limit A | `GP14` | to GND, internal pull-up |
| Limit B | `GP10` | to GND, internal pull-up |
| RS485 DE/RE, TX, RX | `GP2`, `GP0`, `GP1` | parked |

## Bench Bring-Up

1. **Join WiFi over USB.** Open the USB console with
   `screen /dev/cu.usbmodem* 115200`, then:

   ```text
   net ssid Workshop WiFi
   net pass correct horse battery staple
   net name j1
   net save
   net status
   ```

   The SSID and password run to the end of the line, so spaces are fine.
   Settings survive reflashing. `net forget` clears them. The USB console also
   prints `net connected <ip> as roberta-j1.local port 7300` once the board is
   on the network.

2. **Read the LED.**

   | LED | Meaning |
   | --- | --- |
   | Off | No WiFi settings stored |
   | Fast blink | Joining, or waiting to retry |
   | Short blip each second | On the network, no client |
   | Solid | A client is connected |

3. **Find it from the Mac.**

   ```sh
   dns-sd -B _roberta-axis._tcp
   nc roberta-j1.local 7300
   ```

   Type `status` or `net status` in `nc`. The CLI works too:

   ```sh
   build/software/release/apps/cli/roberta_cli axis --port tcp://roberta-j1.local status
   ```

4. **Drive it from Roberta Control.** See
   [software/apps/macos-control](../../software/apps/macos-control/README.md).
   The axis appears in the sidebar by itself. Choose Connect, then Enable
   drive.

5. **First motion.** With the motor free or decoupled, keep the default
   profile (400 steps/s, 800 steps/s²) and jog ±100 steps. Check that the
   direction matches the joint convention, then try a hold-to-jog. At the
   driver's microstep setting, 400 steps/s may be slow. Raise it in the Motion
   profile panel.

6. **Prove the watchdog.** During a long hold-to-jog or a 10,000-step move,
   quit the app or turn off the Mac's WiFi. The axis should stop within about
   a second, and `net status` should show `trips=1`.

## Notes

- Degree readouts use `STEPS_PER_MOTOR_REV` and the gear-ratio registers. They
  default to 200 steps per revolution at 1:1 and are not yet persisted
  (`CONTROL_SAVE_CONFIG` is unsupported). Set them per session with
  `reg write`.
- The firmware-side HIL suite can run over WiFi. Set
  `ROBERTA_HIL_AXIS_URL=tcp://roberta-j1.local` before running
  `roberta_firmware_hil_tests`.
