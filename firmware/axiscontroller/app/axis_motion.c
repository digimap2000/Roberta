#include "axis_motion.h"

#include <math.h>

// Update interval the smoothing window is sized for.
static const double k_update_seconds = 0.001;

// Rounding allowance when the plan lands on a target.
static const double k_landing_steps = 1e-9;

static float clampf(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

static double clampd(double value, double low, double high)
{
    return value < low ? low : (value > high ? high : value);
}

static double sign(double value)
{
    return value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0);
}

static float positive_or(float value, float fallback)
{
    return (isfinite(value) && value > 0.0f) ? value : fallback;
}

// Updates the window needs so its jerk, 2 * max(accel, quick stop) / T,
// stays within the limit.
static double window_needed(const AxisMotionLimits *limits)
{
    const double hardest = fmax(limits->max_accel, limits->quick_stop_decel);
    return 2.0 * hardest / (limits->max_jerk * k_update_seconds);
}

static int window_for(const AxisMotionLimits *limits)
{
    return (int)clampd(ceil(window_needed(limits)), 1.0, AXIS_MOTION_FILTER_MAX);
}

bool axis_motion_sanitise_limits(AxisMotionLimits *limits, float max_step_rate)
{
    const AxisMotionLimits original = *limits;

    limits->max_velocity = clampf(positive_or(limits->max_velocity, 1.0f), 1.0f, max_step_rate);
    limits->max_accel = positive_or(limits->max_accel, 1.0f);
    limits->max_decel = positive_or(limits->max_decel, 1.0f);
    limits->max_jerk = positive_or(limits->max_jerk, 1.0f);
    limits->quick_stop_decel = positive_or(limits->quick_stop_decel, limits->max_decel);
    if (limits->quick_stop_decel < limits->max_decel) {
        limits->quick_stop_decel = limits->max_decel;
    }

    // If the jerk limit's window would not fit, ease acceleration and
    // deceleration down rather than break the jerk limit.
    const double needed = window_needed(limits);
    if (needed > AXIS_MOTION_FILTER_MAX) {
        const float scale = (float)(AXIS_MOTION_FILTER_MAX / needed);
        limits->max_accel *= scale;
        limits->max_decel *= scale;
        limits->quick_stop_decel *= scale;
    }

    return original.max_velocity == limits->max_velocity &&
           original.max_accel == limits->max_accel &&
           original.max_decel == limits->max_decel &&
           original.max_jerk == limits->max_jerk &&
           original.quick_stop_decel == limits->quick_stop_decel;
}

static void fill_history(AxisMotion *motion, double position)
{
    for (int i = 0; i <= motion->window; ++i) {
        motion->history[i] = position;
    }
    motion->head = 0;
    motion->sum = position * motion->window;
    motion->fresh_sum = 0.0;
}

static void apply_limits(AxisMotion *motion, const AxisMotionLimits *limits)
{
    motion->limits = *limits;
    motion->limits_pending = false;
    motion->window = window_for(limits);
    fill_history(motion, motion->plan_position);
}

// Fastest speed the plan may move at this update towards a point `distance`
// ahead and still stop on it, braking at `decel` from the next update.
//
// The plan steps position by velocity * dt, so braking from u covers
// (u - D dt) dt + (u - 2 D dt) dt + ... until zero. Moving at u this update
// and braking after needs u dt plus that, which for u in
// [m D dt, (m + 1) D dt) is (m + 1) u dt - D dt^2 m (m + 1) / 2. Solving for
// u gives the exact bound, so a plan that keeps to it lands exactly.
static double reach_speed(double distance, double decel, double dt)
{
    if (distance <= 0.0) {
        return 0.0;
    }
    const double unit = decel * dt * dt;
    double m = floor((sqrt(1.0 + 8.0 * distance / unit) - 1.0) * 0.5);
    while (m > 0.0 && unit * m * (m + 1.0) * 0.5 > distance) {
        m -= 1.0;
    }
    while (unit * (m + 1.0) * (m + 2.0) * 0.5 <= distance) {
        m += 1.0;
    }
    return distance / ((m + 1.0) * dt) + decel * dt * m * 0.5;
}

// Caps the plan's velocity goal so it never crosses a soft limit, braking
// harder than the request's rate (up to the axis maximum, then the quick
// stop) only if that rate can no longer stop in time. Returns the
// deceleration the plan may use for slowing.
static double guard_soft_limits(const AxisMotion *motion, double *goal, double decel, double dt)
{
    if (!motion->soft_limits_enabled) {
        return decel;
    }

    const double v = motion->plan_velocity;
    const double edges[2] = {motion->soft_max, motion->soft_min};
    const double directions[2] = {1.0, -1.0};
    double allowed = decel;

    for (int i = 0; i < 2; ++i) {
        const double towards = directions[i];
        if (*goal * towards <= 0.0 && v * towards <= 0.0) {
            continue;
        }

        const double room = (edges[i] - motion->plan_position) * towards;
        const double speed = v * towards;
        double rate = decel;
        if (speed - rate * dt > reach_speed(room, rate, dt)) {
            rate = fmax(rate, motion->limits.max_decel);
        }
        if (speed - rate * dt > reach_speed(room, rate, dt)) {
            rate = fmax(rate, motion->limits.quick_stop_decel);
        }

        const double bound = reach_speed(room, rate, dt);
        if (*goal * towards > bound) {
            *goal = bound * towards;
        }
        allowed = fmax(allowed, rate);
    }
    return allowed;
}

// Stage 1: acceleration-limited plan. Returns true while the plan is at rest
// where the request wants it.
static bool update_plan(AxisMotion *motion, double dt)
{
    const double x = motion->plan_position;
    const double v = motion->plan_velocity;
    double decel = motion->request_decel;
    double goal = 0.0;

    if (motion->mode == AXIS_MOTION_POSITION) {
        const double error = motion->target_position - x;
        goal = sign(error) * fmin(motion->cruise_velocity, reach_speed(fabs(error), decel, dt));
    } else if (motion->mode == AXIS_MOTION_VELOCITY) {
        goal = motion->target_velocity;
    }

    decel = guard_soft_limits(motion, &goal, decel, dt);

    // Slow down at up to `decel`, speed up at up to the request's
    // acceleration. Slowing stops at zero; the next update speeds up the
    // other way.
    const double change = goal - v;
    const bool slowing = v * change < 0.0;
    const double limit = slowing ? decel : (double)motion->request_accel;
    double next = v + clampd(change, -limit * dt, limit * dt);
    if (slowing && sign(next) == -sign(v)) {
        next = 0.0;
    }

    double next_position = x + next * dt;
    if (motion->mode == AXIS_MOTION_POSITION &&
        fabs(motion->target_position - next_position) <= k_landing_steps) {
        next_position = motion->target_position;
    }

    motion->plan_position = next_position;
    motion->plan_velocity = next;

    if (next != 0.0) {
        return false;
    }
    return motion->mode != AXIS_MOTION_VELOCITY &&
           (motion->mode != AXIS_MOTION_POSITION || next_position == motion->target_position);
}

// Stage 2: moving average of the plan. The ring holds the last window + 1
// plan positions: the newest `window` are averaged, and the extra one gives
// the plan's velocity as it left the window.
static void update_output(AxisMotion *motion, double dt)
{
    const int length = motion->window + 1;
    const double n = motion->window;
    const double oldest = motion->history[motion->head];
    const double leaving = motion->history[(motion->head + 1) % length];
    const double previous = motion->history[(motion->head + length - 1) % length];
    const double newest = motion->plan_position;

    // Each pass writes every slot once; by the end of it the newest
    // `window` are all slots but the first written, which is the oldest.
    motion->fresh_sum = motion->head == 0 ? 0.0 : motion->fresh_sum + newest;
    motion->history[motion->head] = newest;
    motion->head = (motion->head + 1) % length;
    motion->sum = motion->head == 0 ? motion->fresh_sum : motion->sum + newest - leaving;

    motion->position = motion->sum / n;
    motion->velocity = (float)((newest - leaving) / (n * dt));
    motion->accel = (float)(((newest - previous) - (leaving - oldest)) / (n * dt * dt));
}

void axis_motion_init(AxisMotion *motion, const AxisMotionLimits *limits, double position)
{
    motion->mode = AXIS_MOTION_IDLE;
    motion->limits_pending = false;
    motion->position = position;
    motion->velocity = 0.0f;
    motion->accel = 0.0f;
    motion->target_position = position;
    motion->target_velocity = 0.0f;
    motion->cruise_velocity = 0.0f;
    motion->request_accel = 0.0f;
    motion->request_decel = 0.0f;
    motion->soft_limits_enabled = false;
    motion->soft_min = 0.0;
    motion->soft_max = 0.0;
    motion->plan_position = position;
    motion->plan_velocity = 0.0;
    motion->settled_updates = 0;
    apply_limits(motion, limits);
}

void axis_motion_set_limits(AxisMotion *motion, const AxisMotionLimits *limits)
{
    if (motion->mode == AXIS_MOTION_IDLE) {
        apply_limits(motion, limits);
    } else {
        motion->pending_limits = *limits;
        motion->limits_pending = true;
    }
}

static double clamp_to_soft_limits(const AxisMotion *motion, double position)
{
    if (!motion->soft_limits_enabled) {
        return position;
    }
    return clampd(position, motion->soft_min, motion->soft_max);
}

void axis_motion_set_soft_limits(AxisMotion *motion, bool enabled, double min_position, double max_position)
{
    motion->soft_limits_enabled = enabled && min_position < max_position;
    motion->soft_min = min_position;
    motion->soft_max = max_position;
    if (motion->mode == AXIS_MOTION_POSITION) {
        motion->target_position = clamp_to_soft_limits(motion, motion->target_position);
    }
}

static void set_request_profile(AxisMotion *motion, float velocity, float accel, float decel)
{
    motion->cruise_velocity = clampf(positive_or(fabsf(velocity), 1.0f), 1.0f, motion->limits.max_velocity);
    motion->request_accel = clampf(positive_or(accel, 1.0f), 1.0f, motion->limits.max_accel);
    motion->request_decel = clampf(positive_or(decel, 1.0f), 1.0f, motion->limits.max_decel);
    motion->settled_updates = 0;
}

void axis_motion_move_to(AxisMotion *motion, double target, float velocity, float accel, float decel)
{
    set_request_profile(motion, velocity, accel, decel);
    motion->target_position = clamp_to_soft_limits(motion, target);
    motion->mode = AXIS_MOTION_POSITION;
}

void axis_motion_run(AxisMotion *motion, float velocity, float accel, float decel)
{
    set_request_profile(motion, velocity, accel, decel);
    motion->target_velocity = (float)sign(velocity) * motion->cruise_velocity;
    motion->mode = AXIS_MOTION_VELOCITY;
}

void axis_motion_stop(AxisMotion *motion, float decel)
{
    if (motion->mode == AXIS_MOTION_IDLE) {
        return;
    }

    const float rate = clampf(positive_or(decel, motion->limits.max_decel), 1.0f, motion->limits.quick_stop_decel);
    // A stop never softens one already in progress.
    if (motion->mode == AXIS_MOTION_STOPPING && rate <= motion->request_decel) {
        return;
    }
    motion->request_decel = rate;
    motion->settled_updates = 0;
    motion->mode = AXIS_MOTION_STOPPING;
}

void axis_motion_halt(AxisMotion *motion)
{
    motion->plan_position = motion->position;
    motion->plan_velocity = 0.0;
    motion->velocity = 0.0f;
    motion->accel = 0.0f;
    motion->target_position = motion->position;
    motion->mode = AXIS_MOTION_IDLE;
    if (motion->limits_pending) {
        apply_limits(motion, &motion->pending_limits);
    } else {
        fill_history(motion, motion->position);
    }
}

void axis_motion_redefine_position(AxisMotion *motion, double position)
{
    const double shift = position - motion->position;
    motion->position = position;
    motion->plan_position += shift;
    motion->target_position += shift;
    for (int i = 0; i <= motion->window; ++i) {
        motion->history[i] += shift;
    }
    motion->sum += shift * motion->window;
    motion->fresh_sum += shift * (motion->head == 0 ? 0 : motion->head - 1);
}

void axis_motion_update(AxisMotion *motion, float dt)
{
    if (motion->mode == AXIS_MOTION_IDLE) {
        motion->velocity = 0.0f;
        motion->accel = 0.0f;
        return;
    }

    const bool plan_at_rest = update_plan(motion, dt);
    update_output(motion, dt);

    // Once the plan has rested for the whole ring, every sample is the same
    // and the output has arrived exactly.
    motion->settled_updates = plan_at_rest ? motion->settled_updates + 1 : 0;
    if (motion->settled_updates > motion->window) {
        motion->sum = motion->plan_position * motion->window;
        motion->position = motion->plan_position;
        motion->velocity = 0.0f;
        motion->accel = 0.0f;
        motion->target_position = motion->plan_position;
        motion->mode = AXIS_MOTION_IDLE;
        if (motion->limits_pending) {
            apply_limits(motion, &motion->pending_limits);
        }
    }
}

bool axis_motion_is_idle(const AxisMotion *motion)
{
    return motion->mode == AXIS_MOTION_IDLE;
}
