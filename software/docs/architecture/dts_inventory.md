# DTS And Existing Asset Inventory

Status: `T01` deliverable  
Scope: `software/` workspace only

## Purpose

This document records the reusable DTS components and in-repo software assets
available before host-side architecture is hardened.

Selection rule remains:

1. standard library
2. DTS
3. established third-party
4. home-rolled

## DTS Inventory

| Area | DTS Component | Recommendation | Notes |
| --- | --- | --- | --- |
| Platform/runtime | `dts::version()`, `dts::current_platform()` | Use | Good fit for runtime introspection and diagnostics. |
| Structured logging | `dts::log` | Use | Strong fit for host-side logging. Synchronous, explicit, thread-safe, custom sinks supported. |
| Signals/events | `dts::signal`, `dts::connection`, `dts::scoped_connection` | Use | Good fit for service/event surfaces and UI notifications. |
| Explicit scheduling | `dts::run_loop` | Use | Good fit for single-thread orchestration and tests where a deterministic executor is useful. |
| Cross-thread message handoff | `dts::channel<T>` | Use | Good fit for producer/consumer boundaries and background transport workers. |
| Callback marshalling | `dts::dispatch_to`, `dts::enqueue_to` | Use | Good fit for adapting DTS monitor callbacks onto app/UI threads. |
| Config loading | `dts::doc`, `dts::config` | Use | Good fit for JSON/YAML parsing and typed config loading. |
| Serial enumeration/monitoring | `dts::serial::enumerate_ports`, `dts::serial::monitor` | Use | Good fit for device discovery and connection selection. |
| Serial I/O | `dts::serial` port API | Investigate | Looks promising for direct USB CDC access and possibly RS485 via USB serial adapters. Needs concrete API evaluation during T05. |
| Networking | `dts::net`, `dts::http` | Investigate | Useful later for a dev service/API surface, but not required for Phase 0. |
| Terminal emulation | `dts::term` | Investigate | Could be useful for an embedded Linux console/pseudo-terminal UX later. Not needed for initial robot core. |
| mDNS discovery | `dts::mdns` | Investigate | Potentially useful if controllers or future services advertise on IP networks. Not relevant for current serial/RS485 work. |
| Wi-Fi discovery | `dts::wifi` | Reject for now | Out of scope for the current wired controller architecture. |
| AI API layer | `dts::ai` | Reject for now | Explicitly outside the real-time control path. May be useful for tooling later, not core runtime. |
| Naming utilities | `dts::naming` | Reject for now | Nice-to-have only; no architectural need today. |

## Key DTS Architectural Constraints

From DTS documentation and headers:

- DTS is not an application framework.
- DTS prefers explicit composition over hidden orchestration.
- `dts::signal` is synchronous.
- `dts::run_loop` is single-thread FIFO and caller-owned.
- `dts::serial` monitoring may emit on backend threads and should be marshalled explicitly.
- `dts::log` is synchronous and suitable for process-level host logging.

These constraints align well with the intended Roberta architecture.

## Transport-Specific Observations

### What DTS clearly helps with

- serial device enumeration
- serial device monitoring
- explicit threading primitives
- structured logging
- config parsing

### What DTS does not appear to provide directly

- Modbus RTU implementation
- controller-specific protocol mapping
- robot kinematics or motion planning
- unit test framework

Conclusion:

- use DTS as infrastructure around transport and runtime orchestration
- do not expect DTS to solve the Modbus or robot-domain layers directly

## Existing `software/` Assets

| Asset | Recommendation | Notes |
| --- | --- | --- |
| `software/urdf/ns16.urdf` | Use | Primary robot description candidate for model and simulation work. |
| `software/noddy.urdf` | Investigate | Possibly a reduced or earlier robot description. Keep until model decisions are clearer. |
| `software/urdf/meshes/*` | Use | Useful for simulation/visualization once host-side model tooling exists. |
| `software/3dmodels/*` | Investigate | Potential source CAD for geometry validation and future UX/sim tooling. Not needed for initial core runtime. |
| `software/kinematics/test.py` | Reject as runtime dependency | Useful only as historical reference. The host software stack is C++23, not Python. |
| `software/kinematics/venv/*` | Reject | Environment artifact, not a maintained dependency. |
| current `software/` CMake scaffold | Use | Good baseline for the `software/` slice and should be evolved rather than replaced. |

## Immediate Recommendations

### Use now

- `dts::log`
- `dts::signal`
- `dts::run_loop`
- `dts::channel`
- `dts::dispatch_*`
- `dts::config`
- `dts::doc`
- `dts::serial` discovery/monitoring

### Investigate next

- concrete `dts::serial` port I/O suitability for USB CDC and RS485 adapters
- math / URDF / kinematics library choices outside DTS
- HTTP/service surface only after the core library and CLI are stable

### Do not build around

- DTS AI layer
- DTS Wi-Fi layer
- legacy Python kinematics artifacts

## Reinvention Risks Identified

- building a bespoke logging layer on top of DTS would be wasteful
- building custom callback/signal infrastructure would be wasteful
- building custom config parsing would be wasteful
- building custom serial-device discovery would be wasteful
- expecting DTS to provide Modbus RTU would be incorrect
- expecting DTS to provide robot-domain math would be incorrect
