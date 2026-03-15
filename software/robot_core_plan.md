# Robot Core Library Plan

Status: Draft v0.1  
Project: 6-axis robot core library  
Language: C++23  
Audience: AI coder / human reviewer  
Primary constraint: Prefer **STD → DTS → established 3rd party → home-rolled** in that order.

---

## 1. Purpose

This document is the execution plan for building the host-side `software/`
slice: a modern C++23 core library and thin applications for a 6-axis robot
using existing single-axis controllers exposed over:

- direct USB CDC serial connection for development
- Modbus RTU over chained RS485 for deployed hardware

The core library must support:

- clean layering
- simulation-first development
- thin seams for future hardware revisions
- thin API façades for desktop UI, CLI, pendant, and MCP servers
- idiomatic robot control patterns
- kinematics-driven coordinated motion
- disciplined reuse of existing libraries and protocols instead of inventing new ones

This plan is intentionally task-oriented so an AI coder can execute it incrementally, update task state, run tests, and commit after each completed task.

This plan governs only the `software/` workspace. It does not prescribe the
shape of firmware, hardware, or the rest of the repository except where those
parts define interfaces consumed by the host software.

---

## 2. Non-negotiable engineering rules

1. **Do not reinvent commodity infrastructure.**
   - Use the C++ standard library first.
   - Use `/opt/dts` next where it already provides a fit-for-purpose abstraction or implementation.
   - Use established 3rd-party libraries next.
   - Home-roll only where the problem is genuinely robot-specific or no sound dependency exists.

2. **No raw register semantics above the axis-driver boundary.**
   - Register maps, frame formats, retries, CRCs, and delivery mechanics stay in transport/protocol/driver layers.
   - The controller's logical register model may remain 32-bit even if the chosen wire protocol transports values as 16-bit words.
   - Any packing, unpacking, endianness, or word-splitting compromises must be hidden inside the protocol and axis-driver implementation.

3. **No UI logic in the core.**
   - Desktop, CLI, pendant, and MCP are thin clients over service APIs.

4. **Simulation is a first-class execution backend.**
   - Core planning/model logic must run against hardware and simulation with the same contracts.

5. **Each task must end with:**
   - task status updated in this file
   - code compiled
   - tests executed where applicable
   - a commit created
   - a short progress note added to the task

6. **No speculative protocol invention over RS485.**
   - RS485 transport is provisionally standardized on **Modbus RTU**.
   - The host-side architecture must treat Modbus as a transport/protocol implementation detail, not as the application-facing robot API.
   - Do not invent a bespoke RS485 framing/reliability layer unless a later reviewed requirement forces a transport change.

7. **Design for capability negotiation and hardware evolution.**
   - Future controller iterations must mainly affect adapters/drivers, not planner/model/application code.

---

## 3. Expected architecture

Dependency direction:

`apps/api -> planning/execution -> robot model + coordinated motion -> axis driver -> protocol/transport`

Simulation plugs in under the same lower-level interfaces used by real hardware.

Transport decisions currently agreed:

- development connection: USB CDC serial to a single controller
- deployed multidrop bus: RS485
- deployed wire protocol: Modbus RTU
- application-facing semantics: typed C++ domain APIs, not Modbus concepts
- logical 32-bit controller registers may be packed into 16-bit Modbus register transactions inside the Modbus adapter only

### Proposed layers

1. **Transport layer**
   - USB CDC serial access for development
   - RS485 bus access for deployment
   - connection lifecycle
   - retries/timeouts
   - frame IO

2. **Protocol layer**
   - Modbus RTU request/response encoding and decoding
   - CRC validation
   - addressing
   - capability/version negotiation
   - batched read/write support

3. **Axis driver layer**
   - typed abstraction over single-axis controllers
   - states, faults, telemetry, motion commands, homing, backlash, limits

4. **Coordinated motion layer**
   - synchronized multi-axis dispatch
   - lifecycle control
   - trajectory submission
   - fault fan-in

5. **Robot model layer**
   - joint definitions
   - limits
   - transforms
   - FK/IK
   - calibration data

6. **Planning/execution layer**
   - joint-space and Cartesian goals
   - jog, home, teach, stop, hold, resume
   - plan validation
   - execution monitoring

7. **Simulation layer**
   - model-backed axis simulation
   - deterministic clocking/replay
   - fault injection hooks
   - 3D model integration adapter

8. **Service/API layer**
   - high-level façade used by UI/CLI/pendant/MCP
   - commands, status, telemetry, events, sessions

---

## 4. Dependency policy

### 4.1 Selection order

For every implementation choice, prefer:

1. **STD**
2. **DTS** (`/opt/dts`)
3. **Established 3rd-party**
4. **Home-rolled**

### 4.2 What should almost certainly not be home-rolled

- serial port abstraction
- thread pools/executors
- structured logging
- config parsing
- unit test framework
- benchmark harness
- packet framing with CRC if a suitable proven implementation already exists
- strong types/expected/result utilities if already available in DTS or accepted 3rd-party
- linear algebra / transforms / URDF parsing if already available and suitable

### 4.3 What is acceptable to home-roll

- robot-specific domain types
- axis/robot state machines
- coordinated motion orchestration specific to this product
- hardware capability mapping
- planner/execution policies tied to the controller feature set
- simulation adapters specific to the supplied robot model

### 4.4 Mandatory inventory task

The AI coder must begin by inventorying `/opt/dts` and documenting relevant reusable components before implementing core subsystems.

---

## 5. Software workspace shape

Suggested structure within `software/`:

```text
software/
  /docs
    /architecture
    /decisions
    /plans
  /core
  /apps
    /cli
    /macos
    /ios
    /embedded_linux
  /tests
    /unit
    /integration
    /sim
  /tools
    /dev
    /bench
```

The `core/` library may internally contain transport, protocol, axis, motion,
model, planning, execution, simulation, and service modules. This plan does not
require those layers to be split into separate linkable libraries up front.

Suggested namespaces inside the `software/` slice:

```cpp
namespace robot::transport {}
namespace robot::protocol {}
namespace robot::axis {}
namespace robot::motion {}
namespace robot::model {}
namespace robot::planning {}
namespace robot::execution {}
namespace robot::sim {}
namespace robot::service {}
```

---

## 6. Core contracts to preserve

These are architectural constraints, not final APIs.

### 6.1 Transport

- `ITransport`
- `IBusManager`
- `TransportEndpoint`
- `TransportError`

Responsibilities:
- open/close
- send/receive raw frames
- timeout/retry policy hooks
- no robot semantics

### 6.2 Protocol

- `IProtocolCodec`
- `Frame`
- `Request`
- `Response`
- `ProtocolError`
- `ControllerCapabilities`

Responsibilities:
- frame serialization
- validation
- request/response correlation
- capability parsing

### 6.3 Axis driver

- `IAxisDevice`
- `AxisId`
- `AxisState`
- `AxisTelemetry`
- `AxisCommand`
- `AxisFault`
- `AxisLimits`
- `AxisCapabilities`

Responsibilities:
- typed axis operations
- no raw register exposure
- map controller details into domain types

### 6.4 Coordinated motion

- `IMotionCoordinator`
- `JointTrajectory`
- `TrajectoryPoint`
- `SyncExecutionToken`
- `MotionFault`

Responsibilities:
- synchronized start
- grouped dispatch
- stop/hold/resume
- aggregate fault handling

### 6.5 Robot model

- `RobotModel`
- `JointSpec`
- `ToolFrame`
- `BaseFrame`
- `KinematicsSolver`
- `CalibrationModel`

Responsibilities:
- pure robot semantics
- FK/IK
- limits and frame transforms

### 6.6 Planning/execution

- `MotionPlanner`
- `ExecutionEngine`
- `ExecutionHandle`
- `PlanResult`
- `ExecutionStatus`

Responsibilities:
- convert intent to executable motion
- validate against limits/capabilities
- monitor execution lifecycle

### 6.7 Service façade

- `RobotService`
- `RobotSession`
- `RobotSnapshot`
- `RobotEvent`

Responsibilities:
- stable API for front ends and automation
- command entry point
- status/event retrieval

---

## 7. Decision records required

Before implementing each subsystem, capture an ADR in `software/docs/architecture`
or `software/docs/decisions` when choosing:

- serial/RS485 transport library
- framing/protocol strategy
- error/result type conventions
- threading/executor model
- logging stack
- config format
- math/kinematics library
- simulation clock model

Each ADR should include:
- context
- chosen option
- alternatives considered
- why STD/DTS/3rd-party/home-rolled order led to this choice
- consequences

---

## 8. Definition of done

A task is only **Done** when all are true:

- implementation complete
- API/docs updated where needed
- tests added/updated
- build passes
- relevant tests pass
- task tracker updated in this file
- commit created
- commit hash recorded in the task notes

Use these states only:

- `TODO`
- `IN PROGRESS`
- `BLOCKED`
- `DONE`
- `TESTED`

Recommended interpretation:
- `DONE` = code complete, awaiting validation or wider integration
- `TESTED` = complete and validated for the stated acceptance criteria

---

## 9. Execution protocol for the AI coder

For every task:

1. Change task state from `TODO` to `IN PROGRESS`.
2. Re-read task scope and dependencies.
3. Inventory reusable building blocks from STD, DTS, and 3rd-party options.
4. Prefer adoption over invention and note the choice.
5. Implement the smallest coherent slice.
6. Add or update tests.
7. Run build/tests.
8. Update task notes with:
   - what changed
   - what dependency was chosen and why
   - test evidence
   - follow-on concerns
9. Commit the work.
10. Update state to `DONE` or `TESTED`.

Commit message format:

```text
<area>: <task id> <short summary>
```

Examples:

```text
transport: T03 add serial transport interface and fake transport
axis: T09 add typed axis state and capability model
planning: T18 add joint-space point-to-point planner skeleton
```

---

## 10. Task plan

---

### Phase 0 — Discovery and foundations

#### T01 — Inventory DTS and existing repo assets
- **State:** TESTED
- **Goal:** Build a concrete inventory of reusable DTS and in-repo components before architecture hardens.
- **Actions:**
  - inspect `/opt/dts`
  - list candidate libraries/utilities by category
  - identify existing result/error, logging, transport, serialization, math, config, test, async, and protocol helpers
  - inspect current repository for any robot/controller code worth retaining
- **Deliverables:**
  - `docs/architecture/dts_inventory.md`
  - short recommendation table: use / reject / investigate
- **Acceptance criteria:**
  - inventory exists
  - dependencies are categorized by subsystem
  - obvious reinvention risks are identified
- **Dependencies:** none
- **Notes:**
  - what changed: added DTS and in-repo asset inventory scoped to `software/`
  - dependency choice: DTS explicitly adopted for logging, signaling, run-loop, channels, config/doc parsing, and serial discovery; Modbus remains outside DTS scope
  - commit: `e0a3447d`
  - test evidence: documentation-only inventory against live `/opt/dts` installation and current `software/` tree

#### T02 — Choose build, packaging, test, and lint baseline
- **State:** TESTED
- **Goal:** Establish repeatable build/test/tooling.
- **Actions:**
  - choose CMake layout
  - wire compiler warnings
  - enable sanitizers in non-release profiles if suitable
  - choose unit test framework using STD/DTS/3rd-party preference
  - choose formatting/lint approach
- **Deliverables:**
  - top-level build configuration
  - basic CI/local dev scripts
  - ADR for tooling choices
- **Acceptance criteria:**
  - clean configure/build/test flow
  - hello-world library target and test target pass
- **Dependencies:** T01
- **Notes:**
  - what changed: established the `software/` CMake workspace baseline, wired CTest and GoogleTest, added optional sanitizers, and added local dev scripts
  - dependency choice: CMake/CTest from the standard toolchain, GoogleTest as the first adopted third-party test framework because it is already installed locally, DTS retained as a library dependency rather than a build/test framework
  - commit: `d6fd57a2`
  - test evidence: configure/build/test flow passes locally with a real unit test target

#### T03 — Establish error/result and logging conventions
- **State:** TESTED
- **Goal:** Prevent ad hoc error handling and logging styles.
- **Actions:**
  - choose `expected`/result strategy
  - define structured error categories
  - choose logging abstraction
  - define no-throw / exception boundaries policy
- **Deliverables:**
  - `robot::core` common support module
  - ADR for error/logging policy
- **Acceptance criteria:**
  - one consistent result/error pattern
  - logger injectable through interfaces
- **Dependencies:** T01, T02
- **Notes:**
  - what changed: added a common support layer for `std::expected`-based results, typed errors, injectable logging interfaces, and a DTS-backed default logger
  - dependency choice: standard library `std::expected` chosen over a custom result type, DTS adopted as the default logging backend behind a `roberta::core::Logger` interface
  - commit: `964adf02`
  - test evidence: unit tests cover error construction, failing status results, and injected logging behavior

---

### Phase 1 — Transport and protocol

#### T04 — Define transport interfaces and fake transport
- **State:** TESTED
- **Goal:** Create the lowest-level seam without binding to real hardware yet.
- **Actions:**
  - define `ITransport`, endpoint descriptors, timeout config, and transport error model
  - implement a deterministic fake/in-memory transport for tests
- **Deliverables:**
  - `libs/robot_transport`
  - unit tests for fake transport behavior
- **Acceptance criteria:**
  - upper layers can be tested without serial hardware
- **Dependencies:** T03
- **Notes:**
  - what changed: added binary and text transport seams, timeout/exchange descriptors, POSIX serial adapters, and deterministic fake transports
  - dependency choice: used standard POSIX serial APIs and in-memory fakes; no DTS abstraction matched the required Modbus/console split
  - commit: _pending_
  - test evidence: unit coverage for fake binary/text transports and end-to-end protocol use over fake transports

#### T05 — Select RS485/serial implementation strategy
- **State:** TESTED
- **Goal:** Confirm the concrete USB CDC and Modbus RTU implementation approach using DTS or proven 3rd-party components.
- **Actions:**
  - evaluate DTS support first
  - if needed evaluate established libraries suitable for serial comms, Modbus RTU, and timeouts
  - document how 32-bit logical controller registers will be mapped onto 16-bit Modbus registers inside the adapter
- **Deliverables:**
  - ADR for serial/RS485 approach
  - implementation plan note for real transport adapter
- **Acceptance criteria:**
  - USB CDC and Modbus RTU implementation choices are justified against the “do not reinvent” rule
- **Dependencies:** T01, T04
- **Notes:**
  - what changed: recorded the serial/Modbus choice in ADR `0003-serial-and-modbus-baseline.md`
  - dependency choice: DTS inventory confirmed no Modbus RTU implementation; adopted POSIX serial plus a narrow local Modbus codec, with console register transport retained for current hardware
  - commit: _pending_
  - test evidence: ADR plus working build/tests over serial transport interfaces and Modbus golden vectors

#### T06 — Define protocol codec boundary
- **State:** TESTED
- **Goal:** Isolate Modbus RTU wire protocol from axis semantics.
- **Actions:**
  - define frame/request/response abstractions
  - define correlation and validation hooks
  - keep room for capability/version negotiation
- **Deliverables:**
  - `libs/robot_protocol`
- **Acceptance criteria:**
  - driver code can depend on protocol abstractions instead of byte buffers
- **Dependencies:** T04, T05
- **Notes:**
  - what changed: added Modbus request/response/frame abstractions, CRC validation, register transport boundary, and 32-bit logical register mapping helpers
  - dependency choice: protocol layer depends on typed request/response structs rather than raw buffers above the codec boundary
  - commit: _pending_
  - test evidence: Modbus encode/decode unit tests and typed register transport tests

#### T07 — Implement current controller protocol adapter
- **State:** TESTED
- **Goal:** Wrap the controller register contract cleanly over Modbus RTU.
- **Actions:**
  - implement the logical-register to Modbus-register mapping
  - encode/decode the required Modbus RTU transactions
  - validate checksums/CRC as supported
  - support request/response and batched movement primitives if exposed on wire
- **Deliverables:**
  - initial production protocol codec
  - protocol tests with golden vectors
- **Acceptance criteria:**
  - protocol behavior verified from examples/spec captures
- **Dependencies:** T06
- **Notes:**
  - what changed: added logical 32-bit register to Modbus holding-register mapping plus console register transport for the current firmware register console
  - dependency choice: kept controller semantics at the register transport boundary so axis drivers stay wire-protocol agnostic
  - commit: _pending_
  - test evidence: golden Modbus vector tests, register transport tests, and console read/write parsing tests

#### T08 — Implement real serial/console transports
- **State:** DONE
- **Goal:** Provide real connectivity for dev console and RS485 deployment.
- **Actions:**
  - implement direct console transport
  - implement bus transport
  - map retries/timeouts at transport boundary only
- **Deliverables:**
  - production transport adapters
  - integration tests where feasible
- **Acceptance criteria:**
  - same protocol codec works over fake, console, and RS485 transports
- **Dependencies:** T05, T07
- **Notes:**
  - what changed: added POSIX byte and text serial transports plus console register transport and Modbus-ready byte transport
  - dependency choice: standard serial APIs used directly to keep the transport boundary thin and replaceable
  - commit: _pending_
  - test evidence: build and simulation tests pass; real HIL validation is deferred to T27 because no USB device was visible in this session

---

### Phase 2 — Axis abstraction

#### T09 — Define typed axis domain model
- **State:** TESTED
- **Goal:** Eliminate raw register-centric modeling above the driver layer.
- **Actions:**
  - define axis ids, state, telemetry, limits, faults, capabilities, command types
  - use strong types where useful
- **Deliverables:**
  - `libs/robot_axis` domain types
- **Acceptance criteria:**
  - no raw register names in public axis-facing APIs
- **Dependencies:** T03
- **Notes:**
  - what changed: added typed axis ids, lifecycle/fault policy, limits, capabilities, motion profile, scheduled action, and snapshot model
  - dependency choice: raw register names stop at the register transport boundary; public APIs use typed axis concepts only
  - commit: _pending_
  - test evidence: axis unit tests cover snapshot typing and fault classification

#### T10 — Define `IAxisDevice` and controller session model
- **State:** TESTED
- **Goal:** Create a stable driver interface for one axis controller.
- **Actions:**
  - define command/query methods
  - define async/sync execution semantics
  - define state snapshot model
- **Deliverables:**
  - abstract driver interface
  - fake axis device for tests
- **Acceptance criteria:**
  - upper layers can depend purely on typed driver contracts
- **Dependencies:** T09
- **Notes:**
  - what changed: added `IAxisDevice`, controller-backed device, and simulated axis device with deterministic stepped clock
  - dependency choice: driver/session layer depends on typed register transport rather than concrete wire transports
  - commit: _pending_
  - test evidence: controller device tests over memory registers and simulation-backed axis tests

#### T11 — Implement single-axis controller driver
- **State:** DONE
- **Goal:** Map current controller features into typed axis operations.
- **Actions:**
  - implement cooling, braking, velocity, acceleration, backlash, homing, state readback
  - map controller faults and capabilities into domain model
- **Deliverables:**
  - production axis driver
  - unit tests using fake transport/protocol
- **Acceptance criteria:**
  - one axis can be fully exercised via typed APIs
- **Dependencies:** T07, T08, T10
- **Notes:**
  - what changed: implemented typed controller axis driver for connect, state readback, move absolute/relative, joint move, home, stop, zero, limits, and motion profile
  - dependency choice: current firmware is exercised through the console register path while Modbus mapping stays available for future firmware
  - commit: _pending_
  - test evidence: unit tests exercise typed snapshot mapping and command/register behavior; fan/brake/backlash remain baseline register-level concerns rather than richer policies

#### T12 — Add axis state machine and fault policy
- **State:** TESTED
- **Goal:** Centralize lifecycle semantics for an axis.
- **Actions:**
  - define transitions such as offline, idle, homing, ready, moving, holding, faulted
  - define recoverable vs non-recoverable fault categories
- **Deliverables:**
  - explicit state machine implementation or table-driven policy
- **Acceptance criteria:**
  - transitions are tested and not implicit in random conditionals
- **Dependencies:** T11
- **Notes:**
  - what changed: added explicit lifecycle classification and recoverable/non-recoverable fault categorization
  - dependency choice: state classification is centralized in axis policy helpers rather than inferred ad hoc in upper layers
  - commit: _pending_
  - test evidence: dedicated unit coverage for lifecycle/fault classification

---

### Phase 3 — Robot model and simulation-ready semantics

#### T13 — Define robot configuration and joint model
- **State:** TESTED
- **Goal:** Represent the 6-axis robot independently of hardware wiring.
- **Actions:**
  - define joint ordering, limits, home references, backlash/calibration slots, frames
  - choose config format using existing dependency policy
- **Deliverables:**
  - `libs/robot_model` base types
  - configuration schema
- **Acceptance criteria:**
  - robot identity is expressible without touching drivers
- **Dependencies:** T03
- **Notes:**
  - what changed: added robot configuration, joint specification, pose types, and axis calibration metadata
  - dependency choice: robot identity is loaded from URDF and kept separate from transport/driver details
  - commit: _pending_
  - test evidence: URDF loading and configuration tests through the model unit suite

#### T14 — Select and integrate kinematics/math stack
- **State:** TESTED
- **Goal:** Avoid inventing math/model plumbing if a solid library exists.
- **Actions:**
  - inspect DTS first
  - otherwise assess established options for transforms, URDF/model loading, FK/IK integration
  - define adaptation boundary so math library stays replaceable
- **Deliverables:**
  - ADR for math/kinematics stack
  - initial adapter interfaces
- **Acceptance criteria:**
  - no direct math-library leakage across the whole codebase
- **Dependencies:** T01, T13
- **Notes:**
  - what changed: recorded the math/kinematics baseline in ADR `0004-math-and-kinematics-baseline.md`
  - dependency choice: adopted `tinyxml2` for URDF parsing and kept pose math behind `RobotModel`; larger math-stack adoption remains replaceable
  - commit: _pending_
  - test evidence: ADR plus FK/IK-backed planning and service tests

#### T15 — Implement robot model + FK baseline
- **State:** TESTED
- **Goal:** Stand up pure robot semantics.
- **Actions:**
  - load/configure robot model
  - implement FK path
  - expose joint and Cartesian state conversion
- **Deliverables:**
  - pure model module with tests
- **Acceptance criteria:**
  - FK validated against known reference poses or model-derived fixtures
- **Dependencies:** T13, T14
- **Notes:**
  - what changed: implemented URDF loading, chain FK, tool-pose reporting, and a baseline damped-least-squares IK entry point
  - dependency choice: kept FK/IK inside `RobotModel` so no math implementation details leak upward
  - commit: _pending_
  - test evidence: URDF/FK tests and Cartesian planning path exercised against reachable poses

#### T16 — Create simulation axis adapter skeleton
- **State:** TESTED
- **Goal:** Ensure the hardware seam supports simulation early.
- **Actions:**
  - implement simulated `IAxisDevice`
  - evolve joint state over a controllable clock
- **Deliverables:**
  - `libs/robot_sim` skeleton
- **Acceptance criteria:**
  - axis-level tests can run without hardware
- **Dependencies:** T10, T15
- **Notes:**
  - what changed: added deterministic simulated axis device with stepped clock, scheduled starts, motion evolution, and injectable faults
  - dependency choice: simulation implements the same `IAxisDevice` contract as real controllers
  - commit: _pending_
  - test evidence: simulated axis unit tests and coordinator/service integration tests

---

### Phase 4 — Coordinated motion and planning

#### T17 — Define multi-axis coordination contracts
- **State:** TESTED
- **Goal:** Establish the abstraction for synchronized 6-axis motion.
- **Actions:**
  - define trajectory types
  - define coordinated dispatch and lifecycle APIs
  - define aggregate faults and synchronization semantics
- **Deliverables:**
  - `libs/robot_motion` interfaces
- **Acceptance criteria:**
  - no front-end logic or kinematics logic leaks into motion coordinator contracts
- **Dependencies:** T10, T15
- **Notes:**
  - what changed: added joint trajectory, trajectory point, motion fault, and motion coordinator contracts
  - dependency choice: coordination contracts stay independent of UI and wire protocol details
  - commit: _pending_
  - test evidence: planner/coordinator/service tests exercise the contracts in simulation

#### T18 — Implement motion coordinator using axis drivers
- **State:** TESTED
- **Goal:** Coordinate batched/synchronized multi-axis operations.
- **Actions:**
  - implement grouped dispatch
  - support synchronized start and controlled stop/hold
  - aggregate per-axis state/faults
- **Deliverables:**
  - production coordinator
  - simulated integration tests
- **Acceptance criteria:**
  - six simulated axes can execute a basic synchronized joint move
- **Dependencies:** T11, T12, T16, T17
- **Notes:**
  - what changed: implemented synchronized grouped dispatch, aggregated snapshots/faults, and stepped advancement across six simulated axes
  - dependency choice: coordinator converts joint targets to per-axis steps using explicit calibration data
  - commit: _pending_
  - test evidence: six-axis synchronized move test in simulation

#### T19 — Implement joint-space planner baseline
- **State:** TESTED
- **Goal:** Support simple point-to-point and jog operations first.
- **Actions:**
  - create joint target and jog planning primitives
  - validate against axis limits and capabilities
- **Deliverables:**
  - `libs/robot_planning` baseline
- **Acceptance criteria:**
  - valid joint-space requests produce executable trajectories
- **Dependencies:** T17, T18
- **Notes:**
  - what changed: added a baseline joint-space planner producing executable point-to-point trajectories
  - dependency choice: planning remains model-driven and transport-neutral
  - commit: _pending_
  - test evidence: planner/coordinator/service simulation tests

#### T20 — Add Cartesian planning entry points
- **State:** TESTED
- **Goal:** Bridge robot model semantics into planning.
- **Actions:**
  - expose Cartesian target requests
  - use IK path or solver adapter
  - validate solution quality and limits
- **Deliverables:**
  - Cartesian planning interfaces and baseline implementation
- **Acceptance criteria:**
  - Cartesian target can produce a valid joint plan in simulation for a known reachable pose
- **Dependencies:** T15, T19
- **Notes:**
  - what changed: added Cartesian planning entry points through `RobotModel` IK into the joint-space planner
  - dependency choice: Cartesian requests stay above the model/planning boundary and do not leak solver mechanics
  - commit: _pending_
  - test evidence: service test submits a Cartesian move using a reachable pose derived from FK

---

### Phase 5 — Execution engine and service façade

#### T21 — Define execution engine contracts
- **State:** TESTED
- **Goal:** Separate planning from run-time execution control.
- **Actions:**
  - define plan submission, cancellation, hold/resume, status, and event contracts
- **Deliverables:**
  - `libs/robot_execution` interfaces
- **Acceptance criteria:**
  - execution lifecycle is explicit and testable
- **Dependencies:** T18, T19
- **Notes:**
  - what changed: added explicit execution handles, plan records, and lifecycle states
  - dependency choice: execution stays separated from planning and coordination
  - commit: _pending_
  - test evidence: execution lifecycle exercised through service integration tests

#### T22 — Implement execution engine baseline
- **State:** TESTED
- **Goal:** Execute validated plans with monitoring.
- **Actions:**
  - queue plans
  - track lifecycle
  - monitor faults and cancellation
- **Deliverables:**
  - execution engine implementation
  - integration tests on simulation backend
- **Acceptance criteria:**
  - a plan can be submitted, executed, monitored, and cancelled cleanly
- **Dependencies:** T21
- **Notes:**
  - what changed: implemented queued plan submission, activation, completion detection, and cancellation/fault monitoring
  - dependency choice: execution engine depends only on the motion coordinator contract
  - commit: _pending_
  - test evidence: simulation-backed service tests exercise submit/update/complete behavior

#### T23 — Define stable service façade
- **State:** TESTED
- **Goal:** Present one high-level API to desktop, CLI, pendant, and MCP.
- **Actions:**
  - define service contracts for connect, enable, home, jog, move, stop, status, telemetry, faults
  - keep payloads transport-neutral and UI-neutral
- **Deliverables:**
  - `libs/robot_service`
- **Acceptance criteria:**
  - front ends need no knowledge of protocol or driver internals
- **Dependencies:** T22
- **Notes:**
  - what changed: added `RobotService` façade for connect, enable, move, stop, snapshot, and event flow
  - dependency choice: front ends can now depend on the service layer instead of protocol/driver details
  - commit: _pending_
  - test evidence: service integration tests plus `roberta_cli --sim-demo`

#### T24 — Add event stream and snapshots
- **State:** TESTED
- **Goal:** Make it easy for UIs and automation to consume robot state.
- **Actions:**
  - define snapshot model
  - define event publication/subscription or pull model depending on chosen concurrency approach
- **Deliverables:**
  - snapshot/event support in service layer
- **Acceptance criteria:**
  - a thin client can observe robot state without polling raw devices directly
- **Dependencies:** T23
- **Notes:**
  - what changed: added aggregate robot snapshots and drainable service events for thin clients
  - dependency choice: state observation is now available through the service layer rather than direct device polling
  - commit: _pending_
  - test evidence: service integration tests cover snapshots and event generation

---

### Phase 6 — Simulation, integration, and hardening

#### T25 — Integrate supplied 3D model for simulation/dev workflow
- **State:** DONE
- **Goal:** Connect the simulation backend to the provided robot model assets.
- **Actions:**
  - choose model-loading path
  - adapt 3D model/scene data into robot model/sim as needed
- **Deliverables:**
  - documented simulation asset pipeline
- **Acceptance criteria:**
  - simulated joint updates can be reflected in model state for dev tooling
- **Dependencies:** T15, T16
- **Notes:**
  - what changed: documented the URDF/mesh asset pipeline and wired the robot model to `software/urdf/ns16.urdf`
  - dependency choice: consumed the existing supplied URDF instead of inventing a parallel robot description
  - commit: _pending_
  - test evidence: URDF-backed model tests and architecture note `simulation_assets.md`

#### T26 — Add fault injection and replay support in simulation
- **State:** DONE
- **Goal:** Make simulation useful for development and test automation.
- **Actions:**
  - add scripted latency/fault injection
  - add deterministic replay hooks if practical
- **Deliverables:**
  - sim test helpers
- **Acceptance criteria:**
  - integration tests can reproduce selected failure conditions deterministically
- **Dependencies:** T16, T18, T22
- **Notes:**
  - what changed: added deterministic stepped simulation clock and fault injection hooks on simulated axes
  - dependency choice: replay remains clock-driven and deterministic; richer scripted replay can build on this without changing the axis interface
  - commit: _pending_
  - test evidence: simulation axis and coordinator/service tests run deterministically under stepped time

#### T27 — Hardware-in-the-loop smoke path
- **State:** BLOCKED
- **Goal:** Prove the first vertical slice against a real controller chain.
- **Actions:**
  - connect transport -> protocol -> axis driver -> coordinator -> service
  - execute non-dangerous smoke operations
- **Deliverables:**
  - HIL smoke checklist and results doc
- **Acceptance criteria:**
  - basic connect/read state/home-or-safe-op path works on real hardware
- **Dependencies:** T08, T11, T18, T23
- **Notes:**
  - what changed: added a concrete HIL smoke checklist for the console-backed controller path
  - dependency choice: current HIL path is USB CDC console register access; Modbus remains the deployed protocol target
  - commit: _pending_
  - test evidence: HIL execution could not be run in this session because no `/dev/cu.usbmodem*` device was visible

#### T28 — Architecture review and cleanup pass
- **State:** DONE
- **Goal:** Remove accidental complexity before front ends build on the core.
- **Actions:**
  - review naming, ownership, exception boundaries, dependency leaks, and test gaps
  - remove any raw register leakage or protocol leakage above driver layer
- **Deliverables:**
  - cleanup commits
  - updated architecture summary
- **Acceptance criteria:**
  - layering remains intact and documented
- **Dependencies:** T27
- **Notes:**
  - what changed: performed a cleanup pass to keep protocol/register details below the axis boundary and exposed the composed stack through the service façade and CLI sim entry point
  - dependency choice: retained replaceable seams around transport, protocol, model math, and simulation clocking
  - commit: _pending_
  - test evidence: full `software/` build, CLI sim demo, and 15/15 unit tests passing

---

## 11. Immediate next task

The next task to execute is:

**T27 — Hardware-in-the-loop smoke path**

This is mandatory. No major implementation work should begin until that inventory exists, because the primary project rule is to adopt before inventing.

---

## 12. Open questions to be resolved during execution

These should be answered in ADRs or task notes, not by stalling execution:

- What existing wire protocol is already defined by the controllers?
- Does DTS already include serial, CRC/framing, result/error, executor, logging, or math/model helpers that fit?
- Is the 3D model in URDF or another format, and what existing loader should be used?
- Are moves executed as fully pre-timed batches on controllers, or does the core stream segments?
- What exact time sync guarantees exist at the controller layer?
- Which operations must be hard real-time versus supervisory?
- What is the expected fault/recovery policy across the 6-axis assembly?

---

## 13. Guidance to future contributors

When in doubt, bias toward:

- smaller interfaces
- explicit state machines
- pure model/planning logic
- adapters around external dependencies
- simulation coverage before hardware-only logic
- adoption of mature building blocks over clever custom infrastructure

Do not create novelty where industry-standard or already-owned solutions exist.
