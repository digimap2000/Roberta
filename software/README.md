# Roberta Software

This directory is the host-side software workspace for Roberta.

The workspace is intentionally structured around a reusable C++ core library,
with thin frontends on top:

- `core/`: transport-agnostic logic, typed register definitions, and shared
  runtime helpers
- `apps/cli/`: CLI developer console and dogfooding entrypoint
- `apps/macos/`: future macOS UX application scaffold
- `apps/ios/`: future iOS UX application scaffold
- `apps/embedded_linux/`: future embedded Linux UX application scaffold

Build with CMake presets:

```sh
cd software
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The workspace imports DTS from `/opt/dts`.

Developer helper scripts live under `software/tools/dev/`. With no arguments they
use the `dev` preset; with an explicit build directory they still support
one-off out-of-tree builds and force the Ninja generator.

Useful smoke checks:

```sh
/tmp/roberta-software-check/apps/cli/roberta_cli --runtime
/tmp/roberta-software-check/apps/cli/roberta_cli --registers
/tmp/roberta-software-check/apps/cli/roberta_cli --sim-demo
```

Decision records and architecture notes live under `software/docs/`.
