# Roberta Control

A native SwiftUI Mac app for driving Roberta's WiFi axis controllers. It is a
single-robot tool: every board advertising `_roberta-axis._tcp` over Bonjour
is connected automatically and kept connected, and the app talks to each
through the shared C++ core (`software/core`, via the C API in
`roberta/ffi/axis_client.h`).

Each connected axis is polled 5 times a second. That keeps the status live and
also feeds the controller's link watchdog: if the app quits, hangs or loses
WiFi, a moving axis stops within about a second.

## Build and Run

The app links `libroberta_desktop_ffi.dylib` from the `release` preset, so
build that first:

```sh
cd software
cmake --preset release
cmake --build --preset release
```

Then either run it straight from the package:

```sh
cd software/apps/macos-control
swift run -c release
```

or build an app bundle you can keep in the Dock:

```sh
software/apps/macos-control/scripts/make-app.sh
open "software/apps/macos-control/Roberta Control.app"
```

The first launch asks for Local Network access. Allow it, or discovery and
connections will fail. The bundle links the core library from this checkout's
`build/` directory and Homebrew. It is a bench tool for this Mac, not a
distributable app.

## Using It

- **Sidebar**: the robot's joints J1–J6, filled in as boards named `j1`…`j6`
  appear (set with `net name` on each board). Missing joints show as "not
  found"; anything else is listed under Other. The field at the bottom adds an
  axis by hand: a hostname, an IP, `tcp://host:port`, or a USB serial device
  path such as `/dev/cu.usbmodem101`.
- **Staying connected**: a dropped link or a rebooted board is reconnected
  automatically, backing off from 0.5 s to 5 s between attempts. **Pause**
  stops that for one axis, for example to use its USB console. **Resume**
  starts it again.
- **Events**: connections, link losses, board reboots and why (from the
  firmware's `diag`), faults, and link-watchdog trips. They are also appended to
  `~/Library/Logs/RobertaControl/events.log`. A reboot means the drive was
  disabled and the position counter restarted at zero.
- **Drive**: enable or disable, home, set zero, clear faults, and STOP.
- **Jog**: step-size jogs, hold-to-jog (moves while the button is held), and
  absolute go-to, which needs the axis homed or zeroed.
- **Motion profile**: velocity, acceleration and home velocity in steps.
- **Console**: any firmware console command, such as `net status`,
  `reg dump` or `help`.
- **Move Together** (toolbar, or Motion ▸ Move Together… ⇧⌘M, with two or
  more connected axes): one relative move
  per axis, scheduled to start at the same instant a few hundred ms ahead. It
  reports how far each axis's actual start was from the target, and the spread
  between axes.
- Faults are shown in words with a **Clear** button. Jog and Home stay greyed
  out until the drive is enabled.
- **Frère Jacques** (toolbar, or Motion ▸ Play Frère Jacques ⇧⌘J): the two
  lowest-numbered joints with drives enabled sing it as a round, using step
  rate as pitch. Stop All or ⌘. stops the song; each axis's profile is put
  back afterwards.
- **⌘.**, or **Stop All** in the toolbar, stops every connected axis.

## Without Hardware

`software/tools/dev/fake_axis.py` simulates an axis controller, including
Bonjour advertisement and the watchdog:

```sh
python3 software/tools/dev/fake_axis.py --name sim-j1
python3 software/tools/dev/fake_axis.py --name sim-j2 --port 7301
```

Each simulated axis starts its clock at a random offset, as real boards do,
so Move Together exercises the clock sync. `diag panic` on a fake drops its
links, stays away for 3 s, and comes back as a freshly booted board reporting
the panic, which exercises reconnection and the Events log. Add
`--no-bonjour` to keep test axes out of other running copies of the app.

## Development Switches

| Variable | Effect |
| --- | --- |
| `ROBERTA_CONTROL_AXES=tcp://a,tcp://b` | Add and connect these axes at launch |
| `ROBERTA_CONTROL_NO_DISCOVERY=1` | Skip Bonjour, so tests with fake axes leave the real boards alone |
| `ROBERTA_CONTROL_MOVE_TOGETHER=<steps>` | Enable every connected axis and run one coordinated move after launch; results go to the trace |
| `ROBERTA_CONTROL_PLAY_TUNE=1` | Enable every connected axis and play Frère Jacques after launch |
| `ROBERTA_CONTROL_TRACE=1` | Print connection state changes and errors to stderr |
| `ROBERTA_CONTROL_CAPTURE=/path.png` | Render the window to a PNG after `ROBERTA_CONTROL_CAPTURE_DELAY` seconds (default 4) |
| `ROBERTA_FFI_LIB_DIR` | Build-time override for where to find `libroberta_desktop_ffi` |

The older Tauri app in `../macos` uses the same C API, so it should accept
`tcp://` endpoints too (untested). It needs the shell packages from
`ALP/projects/foundry` to build.
