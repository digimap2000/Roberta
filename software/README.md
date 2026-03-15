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

Build with CMake:

```sh
cmake -S software -B build/software
cmake --build build/software
ctest --test-dir build/software --output-on-failure
```

The workspace imports DTS from `/opt/dts`.

Developer helper scripts live under `software/tools/dev/`.
