# Roberta Control

A native SwiftUI Mac app for driving WiFi axis controllers directly. It finds
axes advertising `_roberta-axis._tcp` over Bonjour, connects to as many as you
like, and talks to each through the shared C++ core
(`software/core`, via the C API in `roberta/ffi/axis_client.h`).

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

- **Sidebar**: axes appear as they are discovered. The field at the bottom
  adds one by hand: a hostname, an IP, `tcp://host:port`, or a USB serial
  device path such as `/dev/cu.usbmodem101`.
- **Drive**: enable or disable, home, set zero, clear faults, and STOP.
- **Jog**: step-size jogs, hold-to-jog (moves while the button is held), and
  absolute go-to, which needs the axis homed or zeroed.
- **Motion profile**: velocity, acceleration and home velocity in steps.
- **Console**: any firmware console command, such as `net status`,
  `reg dump` or `help`.
- **⌘.**, or **Stop All** in the toolbar, stops every connected axis.

## Without Hardware

`software/tools/dev/fake_axis.py` simulates an axis controller, including
Bonjour advertisement and the watchdog:

```sh
python3 software/tools/dev/fake_axis.py --name sim-j1
python3 software/tools/dev/fake_axis.py --name sim-j2 --port 7301
```

## Development Switches

| Variable | Effect |
| --- | --- |
| `ROBERTA_CONTROL_AUTOCONNECT=1` | Connect to every advertised axis |
| `ROBERTA_CONTROL_TRACE=1` | Print connection state changes and errors to stderr |
| `ROBERTA_CONTROL_CAPTURE=/path.png` | Render the window to a PNG after `ROBERTA_CONTROL_CAPTURE_DELAY` seconds (default 4) |
| `ROBERTA_FFI_LIB_DIR` | Build-time override for where to find `libroberta_desktop_ffi` |

The older Tauri app in `../macos` uses the same C API, so it should accept
`tcp://` endpoints too (untested). It needs the shell packages from
`ALP/projects/foundry` to build.
