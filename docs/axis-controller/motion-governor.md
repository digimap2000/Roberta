# Motion Governor

Every motion an axis controller makes passes through one layer: the motion
governor (`firmware/axiscontroller/app/axis_motion.c`). Moves, jogs, homing,
reversals, stops and limit reactions are all requests to it. It decides the
motion actually sent to the motor, and it never exceeds the axis's
commissioned limits whatever is asked of it. A heavy arm swinging at speed is
brought round gradually, never yanked. A jog flipped from forward to reverse
ramps through zero instead of stopping dead.

Only **kill** bypasses it.

## Limits

Each axis has five limits, in motor steps:

| Limit | Meaning |
| --- | --- |
| Max velocity | Top speed |
| Max accel | Hardest acceleration when speeding up |
| Max decel | Hardest deceleration in normal motion and normal stops |
| Max jerk | Fastest change of acceleration, i.e. how quickly torque may change |
| Quick-stop decel | Deceleration for quick stops, at least max decel |

Together these describe what the joint can take: motor torque, gearbox
rating, and the inertia of the arm beyond it. They are commissioned per axis
(see [Commissioning](#commissioning)) and stored on the board. Requests carry
their own velocity and acceleration, which may be gentler than the limits but
are clamped to them.

## How It Works

The governor runs every 1 ms in its own interrupt, below the 50 µs step tick.
It has two stages, chosen so its guarantees hold by construction rather than
by tuning:

1. **Planner.** Velocity and acceleration are limited, but jerk is not. With
   no jerk limit, braking can be computed exactly in discrete time, so the
   plan lands exactly on targets and never crosses a soft limit. If a
   request's own deceleration cannot stop the plan before a soft limit, the
   planner escalates to max decel, then to quick-stop decel.
2. **Smoothing.** The output is the moving average of the plan's positions
   over a window `T = 2 * max(max accel, quick-stop decel) / max jerk`.
   - An average of positions cannot leave the range they stayed in, so there
     is no overshoot and no soft-limit crossing.
   - Its velocity and acceleration are averages too, so they stay within the
     limits.
   - Its jerk is the plan's acceleration now minus its acceleration `T` ago,
     divided by `T`. The worst case is a swing from full braking one way to
     full braking the other, which is exactly the jerk limit.

The step tick then follows the output, interpolating each 1 ms segment so
steps stay evenly spaced.

The price is lag. Motion trails requests by 1 ms plus `T / 2` at steady
speed, so every stop travels roughly `speed * T / 2` further than an
unsmoothed one would. `MOTION_SMOOTHING_MS` reports `T`. A stiffer jerk limit
shortens it.

New limits never apply to a moving axis: they wait until it is next at rest.
Changing limits mid-motion would force either an overspeed or a step in
torque.

Host tests (`software/tests/unit/axis_motion_tests.cpp`) drive the governor
directly, including thousands of random request streams under random limits.
Every update is checked against the velocity, acceleration and jerk limits,
and targets and soft limits must hold to rounding error. Run a long soak
with:

```sh
ROBERTA_MOTION_FUZZ_SCENARIOS=10000 build/software/release/tests/unit/roberta_core_tests --gtest_filter='AxisMotion*'
```

## Stop Levels

| Level | How | What happens | For |
| --- | --- | --- | --- |
| **Stop** | `CONTROL_WORD` bit 5, `stop`, app STOP / Stop All (⌘.), releasing hold-to-jog | Ramp down at the move's own deceleration, then hold | Normal use |
| **Quick stop** | Bit 6, `qstop`, app Quick stop / Quick Stop All (⇧⌘.) | Ramp down at the quick-stop deceleration, then hold. Latches `Motion aborted` (error 108) | Development e-stop, link watchdog, limit switches |
| **Kill** | Bit 12, `kill`, app Kill / Kill All (⌥⌘.) | Motor enable off at once. Homed is cleared. Latches `Killed` (error 109) | Real emergencies |

Quick stop is the stop to reach for during development. It is as fast as
the axis is commissioned to stop, and still within its limits.

Kill takes holding torque away. A gravity-loaded joint will fall unless it
has a brake, and the step count no longer matches the arm, so the axis must
be homed or zeroed again. Kill is a software command over WiFi. **It is not
an emergency stop.** A real e-stop must remain a hardware chain that removes
drive power regardless of any controller or host. Kill is for when software
is the quickest way to cut torque.

Other reactions use these levels too:

- **Disabling the drive while moving** quick-stops first, and drops the
  enable once the axis is at rest.
- **Link watchdog** trips (silent host, disconnect, WiFi loss) quick-stop,
  with the drive left on to hold position. So does a scheduled start that is
  still waiting when the link goes.
- **Main-loop deadman**: the link watchdog and limit reporting run in the
  firmware's main loop. If it goes silent for 500 ms while the axis moves, the
  governor quick-stops by itself and latches `Motion aborted` (error 112).
- **Limit switches**: moving into an active switch quick-stops and latches
  `Unexpected limit hit`.
- **Homing** latches the position when the home switch closes, then
  quick-stops a little past it. Switches are sampled every 1 ms, so the latch
  is within one millisecond of travel, and a limit-switch stop overruns the
  quick-stop distance by about two milliseconds of travel. Leave mechanical
  margin beyond each switch for that plus `speed * T / 2`.
- **Soft limits** are respected by braking before them, so they cause no
  fault during motion. If a soft limit is tightened under a moving axis that
  is already past it, the axis quick-stops and returns inside.

## Commissioning

An uncommissioned axis uses deliberately gentle defaults: 2000 steps/s,
4000 steps/s² acceleration and deceleration, 8000 steps/s² quick stop, and
80,000 steps/s³ jerk. With these, `T` is 200 ms. `MOTION_LIMITS_STATUS`
bit 0 is clear until limits have been saved.

To commission a joint, use the Motion limits panel in Roberta Control, or the
`LIMIT_*` registers:

1. Start from the defaults, with the arm clear and someone at the e-stop.
2. Raise max velocity to the speed the joint needs, within the motor and
   gearbox ratings.
3. Raise accel and decel in steps, with moves loaded as the arm will be
   (worst-case reach and payload). Back off at the first sign of stalls,
   following errors from the closed-loop driver, or gearbox complaint.
4. Set jerk so `T` is short enough to feel responsive but long enough that
   starts and stops are smooth. Around 100 to 200 ms suits a heavy joint.
5. Set quick-stop decel to the hardest stop the joint can take without
   damage, then test it with Quick stop at full speed.
6. **Save to axis** (`CONTROL_WORD` bit 10) stores the limits, soft limits
   and home offset in flash, in their own sector, apart from the network
   settings.

Limits are in motor steps, so they change if the driver's microstepping
changes.
