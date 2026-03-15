# ADR 0001: Software Tooling Baseline

## Status

Accepted

## Context

The `software/` workspace needs a repeatable build and test baseline that fits
the existing C++23/CMake direction and the dependency policy:

1. standard library
2. DTS
3. established third-party
4. home-rolled

The workspace also needs a test framework and a lightweight local developer
workflow before larger subsystems are implemented.

## Decision

The `software/` slice uses:

- CMake as the build system
- CTest as the test runner
- GoogleTest as the unit test framework
- compiler warnings enabled through shared interface targets
- address and undefined sanitizers enabled in non-Release builds when the
  compiler supports them
- small shell scripts under `software/tools/dev/` for local configure, build,
  and test flows

Formatting and linting are not hard requirements in Phase 0 because neither
`clang-format` nor `clang-tidy` is installed in the current environment.
Their future introduction should be additive rather than implicit.

## Alternatives Considered

### Home-rolled test executables with `assert`

Rejected.

This would satisfy the immediate need for tests, but it would amount to
building a lightweight test framework ourselves.

### Catch2 or doctest

Reasonable alternatives, but GoogleTest is already installed locally and has a
stable CMake integration path.

### No sanitizers

Rejected for the baseline.

Optional sanitizers in non-Release builds are a cheap way to catch memory and
UB issues early during systems development.

## Consequences

- `software/` can now be configured, built, and tested with a predictable flow
- unit tests can grow without changing the test runner strategy
- sanitizers are available during development without affecting Release builds
- formatting/linting policy remains intentionally minimal until the tools are
  actually provisioned
