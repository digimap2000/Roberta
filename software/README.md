# Roberta Software

This directory is the host-side software workspace for Roberta.

The workspace is intentionally structured around a reusable C++ core library,
with thin frontends on top:

- `core/`: transport-agnostic logic, typed register definitions, and shared
  runtime helpers
- `apps/cli/`: CLI developer console and dogfooding entrypoint
- `apps/macos/`: Roberta desktop shell client built on the shared
  `@yourorg/app-shell` package
- `apps/ios/`: future iOS UX application scaffold
- `apps/pendant/`: pendant application with embedded Linux and macOS dev runners
- `cmake/toolchains/`: host and cross-compilation toolchain definitions

Build with CMake presets:

```sh
cd software
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

To build the opt-in hardware-in-the-loop firmware regression suite for a real
Pico DUT:

```sh
cd software
cmake --preset hil
cmake --build --preset hil
ROBERTA_HIL_SERIAL_PORT=/dev/cu.usbmodem101 ctest --preset hil
```

If `ROBERTA_HIL_SERIAL_PORT` is not set, the HIL tests default to
`/dev/cu.usbmodem101` and skip themselves when that port is not present.

The workspace imports DTS from `/opt/dts`.

Developer helper scripts live under `software/tools/dev/`. With no arguments they
use the `dev` preset; with an explicit build directory they still support
one-off out-of-tree builds and force the Ninja generator.

Useful smoke checks:

```sh
build/software/dev/apps/cli/roberta_cli --runtime
build/software/dev/apps/cli/roberta_cli --registers
build/software/dev/apps/cli/roberta_cli --sim-demo
build/software/dev/apps/cli/roberta_cli axis --port /dev/cu.usbmodem1101 status
build/software/dev/apps/cli/roberta_cli axis --port /dev/cu.usbmodem1101 enable
build/software/dev/apps/cli/roberta_cli axis --port /dev/cu.usbmodem1101 jog 200
```

The macOS shell client uses the Node toolchain directly instead of the CMake
workspace:

```sh
cd apps/macos
pnpm install
pnpm dev
```

To run the pendant locally on macOS:

```sh
cd software
cmake --preset pendant-macos-dev
cmake --build --preset pendant-macos-dev
./../build/software/pendant-macos-dev/apps/pendant/roberta_pendant_macos_dev
```

To cross-compile the pendant for embedded Linux `aarch64` with Docker:

```sh
software/apps/pendant/docker/cross-build.sh
```

Decision records and architecture notes live under `software/docs/`.
