# Simulation Clock Model

## Context

Simulation is a first-class backend. The first useful simulation model needs to:

- run deterministically in tests
- support scheduled axis starts
- support simple fault injection and replay-friendly progression

## Decision

- Use an explicit stepped simulation clock driven by `advance(delta)`.
- Keep the clock under caller control instead of hiding it behind wall-clock threads.
- Store scheduled axis execution times as microseconds on each simulated axis.

## Consequences

- Unit and integration tests remain deterministic.
- Coordinated starts can be tested without real hardware.
- UI or service layers can later wrap this with a wall-clock scheduler if needed.
