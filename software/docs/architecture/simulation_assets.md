# Simulation Assets

The current simulation/model baseline uses:

- `software/urdf/ns16.urdf` as the robot description source
- `software/urdf/meshes/*.stl` as the referenced visual assets

The host-side core currently consumes the URDF for kinematic structure and link/joint
metadata. Mesh assets are not rendered in the core library; they are treated as
referenced assets for future desktop tooling.

This keeps the simulation and robot model aligned with a single source of geometry
while leaving visual rendering concerns in future UX applications.
