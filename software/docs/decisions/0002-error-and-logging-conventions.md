# ADR 0002: Error And Logging Conventions

## Status

Accepted

## Context

The `software/` slice needs one consistent error/result convention and one
consistent logging boundary before transport and protocol code starts to grow.

The design constraints are:

- prefer standard vocabulary types first
- adopt DTS where it already solves the infrastructure problem
- keep robot-domain interfaces explicit
- keep logging injectable rather than hard-wired into every class

## Decision

The `software/` slice uses:

- `std::expected<T, roberta::core::Error>` as the default result type
- `std::expected<void, roberta::core::Error>` as the default status type
- `roberta::core::ErrorCode` plus `roberta::core::Error` as the common error model
- `roberta::core::Logger` as the interface boundary for injectable logging
- `dts::log` as the default concrete logging backend

Exception policy:

- domain, transport, protocol, and service APIs should prefer `Result<T>` or
  `Status` for expected operational failures
- exceptions are acceptable at process-boundary or third-party-boundary setup
  code, but should not be the routine control-flow mechanism for robot-domain
  operations

## Alternatives Considered

### DTS-specific result type

Rejected because DTS does not provide a general result/expected type that is
more appropriate than the C++23 standard library.

### Raw `bool`/error-code output parameters

Rejected because it scales poorly, obscures failure context, and encourages
inconsistent conventions.

### Calling `dts::log` directly everywhere

Rejected as the sole convention because it couples all domain code directly to
the DTS logging API and makes test doubles or alternate sinks awkward.

## Consequences

- the common support layer stays small and explicit
- operational failures have a typed, inspectable representation
- logging can be injected in tests and adapters while still defaulting to DTS
- the rest of the system can depend on stable `roberta::core` conventions
