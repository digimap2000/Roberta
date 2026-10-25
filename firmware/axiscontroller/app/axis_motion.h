#pragma once

// Motion governor: the one layer every motion request passes through.
//
// It owns the axis's planned position, velocity and acceleration, and moves
// them only within the axis's mechanical limits: velocity, acceleration,
// deceleration and jerk. Requests may ask for gentler motion than the limits,
// never harsher. New targets, reversals and stops are all reached by
// jerk-limited ramps, so the drive train never sees a step in torque.
//
// Two stages make the guarantees hold by construction rather than by tuning:
//
// 1. A planner with acceleration limits but no jerk limit. Without jerk its
//    braking can be computed exactly in discrete time, so it never overshoots
//    a target or crosses a soft limit (escalating to the axis's maximum or
//    quick-stop deceleration if a request's own rate would not do).
// 2. A moving average of the planner's position over a window
//    T = 2 * max(max_accel, quick_stop_decel) / max_jerk. The output is an
//    average of planner positions, so it can't leave the range they stayed
//    in (no overshoot, no soft-limit crossing); its velocity and
//    acceleration are averages too, so they stay within the limits. Its
//    jerk is the plan's acceleration now minus T ago, over T: at most the
//    swing from full braking one way to full braking the other, 2 * max /
//    T, which is the jerk limit.
//
// The price is that motion trails the plan by T / 2 at steady speed, and
// every stop travels about speed * T / 2 further than the plan's own.
//
// Pure C with no hardware dependencies: the firmware runs it from a timer
// interrupt, and host tests drive it directly.

#include <stdbool.h>

// Longest smoothing window, in updates. Limits are adjusted if they would
// need longer (see axis_motion_sanitise_limits).
#define AXIS_MOTION_FILTER_MAX 2048

typedef struct {
    float max_velocity;      // steps/s
    float max_accel;         // steps/s^2 while speeding up
    float max_decel;         // steps/s^2 while slowing down in normal motion
    float max_jerk;          // steps/s^3: how fast acceleration may change
    float quick_stop_decel;  // steps/s^2 for quick stops; at least max_decel
} AxisMotionLimits;

typedef enum {
    AXIS_MOTION_IDLE,
    AXIS_MOTION_POSITION,
    AXIS_MOTION_VELOCITY,
    AXIS_MOTION_STOPPING,
} AxisMotionMode;

typedef struct {
    AxisMotionLimits limits;
    AxisMotionMode mode;

    // Limits waiting for the axis to come to rest.
    bool limits_pending;
    AxisMotionLimits pending_limits;

    // Output state: what the axis actually follows.
    double position;
    float velocity;
    float accel;

    // Current request, already clamped to the limits.
    double target_position;
    float target_velocity;
    float cruise_velocity;
    float request_accel;
    float request_decel;

    bool soft_limits_enabled;
    double soft_min;
    double soft_max;

    // Stage 1: the acceleration-limited plan.
    double plan_position;
    double plan_velocity;

    // Stage 2: moving average of the last `window` plan positions. One more
    // is kept so the output's acceleration comes straight from the plan.
    int window;
    int head;
    int settled_updates;
    double sum;
    // The sum rebuilt from scratch over each pass of the ring, replacing
    // `sum` at the end of the pass so rounding never accumulates.
    double fresh_sum;
    double history[AXIS_MOTION_FILTER_MAX + 1];
} AxisMotion;

// Makes limits usable: every value positive, quick_stop_decel at least
// max_decel, velocity within max_step_rate, and acceleration and quick-stop
// deceleration low enough for the jerk limit's smoothing window (at a 1 ms
// update) to fit AXIS_MOTION_FILTER_MAX. Returns false if anything changed.
bool axis_motion_sanitise_limits(AxisMotionLimits *limits, float max_step_rate);

void axis_motion_init(AxisMotion *motion, const AxisMotionLimits *limits, double position);

// New limits apply at once if the axis is at rest, otherwise when it next
// comes to rest: changing them under a moving axis would force either an
// overspeed or a torque step while the motion in progress adjusts.
void axis_motion_set_limits(AxisMotion *motion, const AxisMotionLimits *limits);
void axis_motion_set_soft_limits(AxisMotion *motion, bool enabled, double min_position, double max_position);

// Moves to a position at up to `velocity`, speeding up at up to `accel` and
// slowing at up to `decel`; each is clamped to the limits.
void axis_motion_move_to(AxisMotion *motion, double target, float velocity, float accel, float decel);

// Runs at a signed velocity (jogging, homing) until another request.
void axis_motion_run(AxisMotion *motion, float velocity, float accel, float decel);

// Brings the axis to rest decelerating at up to `decel`, which is clamped to
// quick_stop_decel. Normal stops pass the move's deceleration; quick stops
// pass quick_stop_decel.
void axis_motion_stop(AxisMotion *motion, float decel);

// Abandons all motion instantly. Only for when drive power is being removed:
// with power on, the drive would try to follow this and hit the gears.
void axis_motion_halt(AxisMotion *motion);

// Shifts the coordinate frame so the current position reads `position`,
// carrying any target with it. Safe while moving.
void axis_motion_redefine_position(AxisMotion *motion, double position);

// Advances by dt seconds. The smoothing window is sized for dt = 1 ms.
void axis_motion_update(AxisMotion *motion, float dt);

bool axis_motion_is_idle(const AxisMotion *motion);
