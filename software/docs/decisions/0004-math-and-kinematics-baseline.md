# Math And Kinematics Baseline

## Context

The host-side core needs forward kinematics now and an inverse-kinematics entry
point soon, but the project does not yet have a committed third-party robot math stack.

The repository already contains:

- `software/urdf/ns16.urdf`
- corresponding mesh assets under `software/urdf/meshes`

`tinyxml2` is installed locally and is sufficient to parse the URDF structure. No
pre-installed robot-specific kinematics library was available in the active environment.

## Decision

- Use `tinyxml2` to load the URDF baseline model.
- Keep robot math behind `roberta::model::RobotModel`.
- Implement a small internal pose/math layer sufficient for:
  - URDF origin parsing
  - chain forward kinematics
  - a baseline damped-least-squares IK entry point

## Alternatives considered

- Eigen + external robotics stack
  Deferred until a concrete dependency decision is made and the chosen package is
  available in the development environment.
- Hand-maintained hard-coded robot geometry
  Rejected because the repo already contains a usable URDF asset.

## Consequences

- The math layer is intentionally small and replaceable.
- FK and a baseline IK path are available now for planning and simulation tests.
- A later pivot to Eigen or a stronger robotics stack remains cheap because the
  rest of the workspace only depends on `RobotModel`.
