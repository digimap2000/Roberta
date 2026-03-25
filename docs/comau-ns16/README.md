# Comau NS16

This section is the working specification set for the Comau NS16 robot used in
this project. It is intended to capture both vendor-sourced facts and
reverse-engineered findings in one place.

## Documents

- `pendant/README.md`: pendant-specific documentation index and working notes.
- `pendant/keypad-connector-mapping.md`: worksheet for tracing keypad signals to
  the internal pendant connectors.

## Status

- Robot family: `Comau NS16`
- Scope: robot arm, controller interfaces, touch pendant, and related hardware
- Maturity: scaffold
- Rule: every technical claim should carry a source and a confidence level

## How To Use This Document

- Record stable facts here once they have been verified.
- Keep raw notes, photos, captures, and measurements out of this file; link to
  them from the evidence register instead.
- Mark anything inferred from reverse engineering as `inferred` until it is
  confirmed.
- Prefer exact values with units.

## Source Register

| ID | Source type | Description | Location | Confidence | Notes |
| --- | --- | --- | --- | --- | --- |
| SRC-001 | pending | Original manuals / brochures | pending | pending | |
| SRC-002 | pending | Physical inspection / measurement | pending | pending | |
| SRC-003 | pending | Reverse engineering notes | pending | pending | |
| SRC-004 | pending | Electrical captures / continuity tests | pending | pending | |

## Robot Identity

| Field | Value | Source | Notes |
| --- | --- | --- | --- |
| Manufacturer | Comau | pending | |
| Model | NS16 | pending | |
| Variant / revision | pending | pending | |
| Serial number | pending | pending | |
| Controller model | pending | pending | |
| Touch pendant model | pending | pending | |

## Specification Register

Use this table for canonical facts once they are known.

| Area | Parameter | Value | Units | Confidence | Source | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| General | Payload | pending | kg | pending | pending | |
| General | Reach | pending | mm | pending | pending | |
| General | Mass | pending | kg | pending | pending | |
| Mechanical | Mounting orientation | pending | | pending | pending | |
| Electrical | Supply voltage | pending | V | pending | pending | |
| Electrical | Supply phase | pending | | pending | pending | |
| Electrical | Power draw | pending | kVA / kW | pending | pending | |
| Control | Axis count | pending | | pending | pending | |
| Control | Feedback type | pending | | pending | pending | |
| Tooling | Flange standard | pending | | pending | pending | |

## General Description

### Intended Use

Pending.

### Known Project Assumptions

- This robot is being adapted for use with Roberta project hardware and
  software.
- Some required information may only be available through measurement or
  reverse engineering.

## Mechanical

### Envelope And Mounting

| Item | Value | Units | Source | Notes |
| --- | --- | --- | --- | --- |
| Base footprint | pending | mm | pending | |
| Overall height | pending | mm | pending | |
| Reach envelope | pending | mm | pending | |
| Base mounting hole pattern | pending | mm | pending | |

### Kinematic Structure

| Joint | Description | Axis direction | Notes | Source |
| --- | --- | --- | --- | --- |
| J1 | pending | pending | | pending |
| J2 | pending | pending | | pending |
| J3 | pending | pending | | pending |
| J4 | pending | pending | | pending |
| J5 | pending | pending | | pending |
| J6 | pending | pending | | pending |

### Travel Limits

| Joint | Negative limit | Positive limit | Units | Source | Notes |
| --- | --- | --- | --- | --- | --- |
| J1 | pending | pending | deg | pending | |
| J2 | pending | pending | deg | pending | |
| J3 | pending | pending | deg | pending | |
| J4 | pending | pending | deg | pending | |
| J5 | pending | pending | deg | pending | |
| J6 | pending | pending | deg | pending | |

### Dynamic Ratings

| Parameter | Value | Units | Source | Notes |
| --- | --- | --- | --- | --- |
| Maximum payload | pending | kg | pending | |
| Maximum TCP speed | pending | mm/s | pending | |
| Repeatability | pending | mm | pending | |

## Electrical

### Power

| Parameter | Value | Units | Source | Notes |
| --- | --- | --- | --- | --- |
| Main supply | pending | | pending | |
| Control supply | pending | | pending | |
| Internal bus voltages | pending | V | pending | |
| Motor brake supply | pending | V | pending | |

### Motors And Feedback

| Axis | Motor type | Feedback device | Brake | Notes | Source |
| --- | --- | --- | --- | --- | --- |
| J1 | pending | pending | pending | | pending |
| J2 | pending | pending | pending | | pending |
| J3 | pending | pending | pending | | pending |
| J4 | pending | pending | pending | | pending |
| J5 | pending | pending | pending | | pending |
| J6 | pending | pending | pending | | pending |

## Controller And Interfaces

### Controller

| Item | Value | Source | Notes |
| --- | --- | --- | --- |
| Controller family | pending | pending | |
| CPU / main board | pending | pending | |
| Fieldbus / comms | pending | pending | |
| Teach mode interface | pending | pending | |

### External Interfaces

| Interface | Connector | Signals | Source | Notes |
| --- | --- | --- | --- | --- |
| Pendant | pending | pending | pending | |
| Tool I/O | pending | pending | pending | |
| Cabinet I/O | pending | pending | pending | |
| Service / debug | pending | pending | pending | |

## Touch Pendant

### Overview

Pending.

### Pendant Documentation

- See `pendant/README.md` for pendant-specific notes and reverse-engineering
  tasks.
- See `pendant/keypad-connector-mapping.md` for the six 9-way connector mapping
  worksheet.

### Physical Connectors

| Connector | Purpose | Pin count | Notes | Source |
| --- | --- | --- | --- | --- |
| pending | pending | pending | | pending |

### Display And Touch

| Item | Value | Source | Notes |
| --- | --- | --- | --- |
| Display type | pending | pending | |
| Touch technology | pending | pending | |
| Touch controller | pending | pending | |

### Keypad Matrix

This subsection is the starting point for the pendant reverse-engineering work.

| Item | Value | Source | Notes |
| --- | --- | --- | --- |
| Matrix dimensions | pending | pending | |
| Row identifiers | pending | pending | |
| Column identifiers | pending | pending | |
| Known key labels | pending | pending | |
| Scan voltage / logic levels | pending | pending | |

#### Matrix Mapping Template

| Row | Col 1 | Col 2 | Col 3 | Col 4 | Col 5 | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| pending | pending | pending | pending | pending | pending | |
| pending | pending | pending | pending | pending | pending | |
| pending | pending | pending | pending | pending | pending | |
| pending | pending | pending | pending | pending | pending | |
| pending | pending | pending | pending | pending | pending | |

## Safety And Interlocks

| Item | Behavior | Source | Notes |
| --- | --- | --- | --- |
| E-stop chain | pending | pending | |
| Enabling switch | pending | pending | |
| Guard interlocks | pending | pending | |
| Brake release behavior | pending | pending | |

## Tool Flange And Utilities

| Item | Value | Units | Source | Notes |
| --- | --- | --- | --- | --- |
| Flange pattern | pending | | pending | |
| Pilot diameter | pending | mm | pending | |
| Pneumatics | pending | | pending | |
| Tool electrical pass-through | pending | | pending | |

## Reverse-Engineered Findings

Use this section for conclusions that are not yet backed by vendor
documentation.

| ID | Finding | Status | Confidence | Source | Notes |
| --- | --- | --- | --- | --- | --- |
| RE-001 | pending | open | pending | pending | |

## Open Questions

- Pending.

## Change Log

| Date | Author | Change |
| --- | --- | --- |
| 2026-03-24 | Codex | Initial documentation scaffold added. |
