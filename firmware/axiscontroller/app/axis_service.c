#include "axis_service.h"

#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/time.h"

#include <math.h>

// Every motion request goes through the motion governor (axis_motion.c),
// which keeps the axis within its commissioned velocity, acceleration and
// jerk limits whatever is asked of it. Two interrupts run motion, so WiFi,
// USB and console work in the main loop cannot disturb it:
//
// - The step tick, every 50 us at the highest priority, follows the
//   governor's path. Each 1 ms governor update is a segment, interpolated
//   across the tick so steps come out evenly spaced.
// - The governor, at the start of each segment, triggered by the tick at a
//   lower priority, so it can take its time without delaying steps. It
//   computes the segment after the one starting, so motion trails requests
//   by one segment.
//
// The step position is kept in raw steps from boot; the position registers
// add an offset that SET_ZERO and homing change, so the governor never has
// to jump.
//
// Main-loop entry points touch shared state with interrupts briefly
// disabled, which holds off both interrupts.
enum {
    AXIS_TICK_US = 50,
    AXIS_TICKS_PER_SEGMENT = 20,
    // CL57T needs at least 2.5 us; leave margin for the buffer and opto.
    AXIS_STEP_PULSE_US = 5,
    AXIS_DEFAULT_MAX_VELOCITY_STEPS_S = 400,
    AXIS_DEFAULT_ACCELERATION_STEPS_S2 = 800,
    AXIS_DEFAULT_HOME_VELOCITY_STEPS_S = 150,
    // Below the step tick, above WiFi, USB and the rest.
    AXIS_GOVERNOR_IRQ_PRIORITY = 0x40,
    // Main-loop silence that stops a moving axis.
    AXIS_DEADMAN_MS = 500,
    // Fixed-point fraction bits for interpolating segments.
    AXIS_Q = 16,
};

static const float k_segment_seconds = 0.001f;

typedef struct {
    // Drive enables as they are, and as requested: dropping one waits for
    // the axis to come to rest.
    bool outputs_enabled;
    bool motor_enabled;
    bool outputs_wanted;
    bool motor_wanted;
    bool homed;
    bool homing;
    bool limit_stop;
    bool deadman_stop;
    AxisDirection direction;
    int32_t raw_steps;
    int32_t offset_steps;
    uint32_t max_velocity_steps_s;
    uint32_t acceleration_steps_s2;
    uint32_t home_velocity_steps_s;
    int32_t home_offset_steps;
    int32_t soft_min_steps;
    int32_t soft_max_steps;
    uint32_t emitted_step_count;
} AxisState;

// One governor update's movement, in raw steps << AXIS_Q.
typedef struct {
    int64_t start;
    int64_t end;
} AxisSegment;

typedef struct {
    AxisSegment segment;
    int tick;
    uint32_t adopted;
    // Steps still owed to the segment after this tick.
    int32_t owed;
} AxisFollower;

typedef struct {
    bool armed;
    bool finished;
    AxisCommand command;
    uint32_t at_tick_us;
    uint32_t started_tick_us;
    AxisResult result;
} AxisScheduledStart;

static AxisState g_axis;
static AxisMotion g_motion;
static AxisFollower g_follow;
static AxisTimingStats g_timing;
static AxisScheduledStart g_scheduled;
// Segments from the governor to the tick: the governor fills the buffer the
// tick isn't reading, then bumps the count.
static AxisSegment g_segments[2];
static volatile uint32_t g_published;
static int64_t g_published_end;
// Set by the tick when a scheduled start is due; the governor applies it.
static volatile bool g_start_pending;
// True while the governor interrupt runs, so the tick never pends it twice
// for one segment.
static volatile bool g_governor_busy;
// Bumped by the main loop. If it stops while the axis moves, the governor
// quick-stops: the link watchdog and limit reporting live in the main loop.
static volatile uint32_t g_heartbeat;
static uint32_t g_heartbeat_seen;
static uint32_t g_heartbeat_age_ms;
static int g_alarm = -1;
static int g_governor_irq = -1;
static uint64_t g_next_tick_us;
static uint64_t g_last_tick_us;

AxisMotionLimits axis_service_default_limits(void)
{
    // Gentle enough for any joint: a 200 ms smoothing window and modest
    // speeds until the axis is commissioned.
    return (AxisMotionLimits){
        .max_velocity = 2000.0f,
        .max_accel = 4000.0f,
        .max_decel = 4000.0f,
        .max_jerk = 80000.0f,
        .quick_stop_decel = 8000.0f,
    };
}

static bool motion_active(void)
{
    return !axis_motion_is_idle(&g_motion) || g_follow.owed != 0;
}

static bool drive_ready(void)
{
    return g_axis.outputs_enabled && g_axis.motor_enabled && g_axis.outputs_wanted && g_axis.motor_wanted;
}

static void update_outputs(void)
{
    axis_hw_set_output_enable(g_axis.outputs_enabled);
    axis_hw_set_motor_enable(g_axis.motor_enabled);
}

static void apply_soft_limits(void)
{
    // Off while homing: the position means nothing until the switch is found.
    const bool enabled = g_axis.soft_min_steps < g_axis.soft_max_steps && !g_axis.homing;
    axis_motion_set_soft_limits(&g_motion, enabled,
                                (double)g_axis.soft_min_steps - g_axis.offset_steps,
                                (double)g_axis.soft_max_steps - g_axis.offset_steps);
}

static void quick_stop(void)
{
    axis_motion_stop(&g_motion, g_motion.limits.quick_stop_decel);
}

static bool quick_stopping(void)
{
    return g_motion.mode == AXIS_MOTION_STOPPING && g_motion.request_decel >= g_motion.limits.quick_stop_decel;
}

// Puts the governor and the step follower at rest where the motor is.
// Main loop only, interrupts disabled.
static void resync_governor(void)
{
    const int64_t here = (int64_t)g_axis.raw_steps * (1 << AXIS_Q);

    g_motion.position = g_axis.raw_steps;
    axis_motion_halt(&g_motion);
    g_published_end = here;
    g_follow.segment = (AxisSegment){.start = here, .end = here};
    g_follow.adopted = g_published;
    g_follow.owed = 0;
}

// Drops a drive enable only once the axis is at rest: taking torque away
// from a moving arm leaves it running on unchecked.
static void request_drive(bool outputs, bool motor)
{
    g_axis.outputs_wanted = outputs;
    g_axis.motor_wanted = motor;
    if ((!outputs || !motor) && motion_active()) {
        g_axis.homing = false;
        apply_soft_limits();
        quick_stop();
        return;
    }
    g_axis.outputs_enabled = outputs;
    g_axis.motor_enabled = motor;
    update_outputs();
}

static void finish_drive_request(void)
{
    if ((g_axis.outputs_enabled != g_axis.outputs_wanted || g_axis.motor_enabled != g_axis.motor_wanted) &&
        !motion_active()) {
        g_axis.outputs_enabled = g_axis.outputs_wanted;
        g_axis.motor_enabled = g_axis.motor_wanted;
        update_outputs();
    }
}

static void move_to_raw(double target)
{
    g_axis.homing = false;
    apply_soft_limits();
    axis_motion_move_to(&g_motion, target, (float)g_axis.max_velocity_steps_s,
                        (float)g_axis.acceleration_steps_s2, (float)g_axis.acceleration_steps_s2);
}

static uint32_t clamp_rate(uint32_t rate)
{
    const uint32_t max_rate = (uint32_t)AXIS_SERVICE_MAX_STEP_RATE;
    return rate < max_rate ? rate : max_rate;
}

// Applies a command to the motion state. Callers hold interrupts off or run
// in the governor interrupt (scheduled motion commands only).
static AxisResult apply_command(const AxisCommand *command)
{
    switch (command->type) {
    case AXIS_COMMAND_GET_STATUS:
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_OUTPUT_ENABLE:
        request_drive(command->flag, g_axis.motor_wanted);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_MOTOR_ENABLE:
        request_drive(g_axis.outputs_wanted, command->flag);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_MAX_VELOCITY:
        if (command->rate == 0) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.max_velocity_steps_s = clamp_rate(command->rate);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_ACCELERATION:
        if (command->rate == 0) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.acceleration_steps_s2 = command->rate;
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_HOME_VELOCITY:
        if (command->rate == 0) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.home_velocity_steps_s = clamp_rate(command->rate);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_HOME_OFFSET:
        g_axis.home_offset_steps = command->steps;
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_MOVE_TO:
        if (!g_axis.homed) {
            return AXIS_RESULT_NOT_HOMED;
        }
        if (!drive_ready()) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        move_to_raw((double)command->steps - g_axis.offset_steps);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_MOVE_BY:
        if (!drive_ready()) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        // From where the plan is heading, not where the motor has got to,
        // so a jog while moving never aims behind the motion.
        move_to_raw(g_motion.plan_position + command->steps);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_HOME:
        if (!drive_ready()) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.homing = true;
        g_axis.homed = false;
        apply_soft_limits();
        axis_motion_run(&g_motion, -(float)g_axis.home_velocity_steps_s,
                        (float)g_axis.acceleration_steps_s2, (float)g_axis.acceleration_steps_s2);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_ZERO:
        // Only the reported position changes, so this is safe even while
        // moving.
        g_axis.offset_steps = -g_axis.raw_steps;
        g_axis.homed = true;
        apply_soft_limits();
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_STOP:
        g_axis.homing = false;
        apply_soft_limits();
        axis_motion_stop(&g_motion, fminf((float)g_axis.acceleration_steps_s2, g_motion.limits.max_decel));
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_QUICK_STOP:
        g_axis.homing = false;
        apply_soft_limits();
        quick_stop();
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_KILL:
        g_axis.motor_wanted = false;
        g_axis.motor_enabled = false;
        update_outputs();
        g_axis.homing = false;
        g_axis.homed = false;
        resync_governor();
        apply_soft_limits();
        return AXIS_RESULT_OK;
    }

    return AXIS_RESULT_INVALID_ARGUMENT;
}

static void watch_limit_switches(void)
{
    const AxisLimitState limits = axis_hw_read_limits();

    if (g_axis.homing) {
        if (limits.home) {
            // The switch position is home_offset; the quick stop carries
            // the axis a little past it.
            g_axis.offset_steps = g_axis.home_offset_steps - g_axis.raw_steps;
            g_axis.homed = true;
            g_axis.homing = false;
            apply_soft_limits();
            quick_stop();
        } else if (limits.limit_a) {
            g_axis.homing = false;
            apply_soft_limits();
            quick_stop();
            g_axis.limit_stop = true;
        }
        return;
    }

    if (axis_motion_is_idle(&g_motion) || quick_stopping()) {
        return;
    }
    const float velocity = g_motion.velocity;
    if ((velocity < 0.0f && (limits.home || limits.limit_a)) || (velocity > 0.0f && limits.limit_b)) {
        quick_stop();
        g_axis.limit_stop = true;
    }
}

static void publish_segment(void)
{
    const uint32_t next = g_published + 1u;
    const int64_t end = llround(g_motion.position * (double)(1 << AXIS_Q));

    g_segments[next & 1u] = (AxisSegment){.start = g_published_end, .end = end};
    g_published_end = end;
    __dmb();
    g_published = next;
}

static void watch_main_loop(void)
{
    const uint32_t heartbeat = g_heartbeat;

    if (heartbeat != g_heartbeat_seen) {
        g_heartbeat_seen = heartbeat;
        g_heartbeat_age_ms = 0;
        return;
    }
    if (++g_heartbeat_age_ms >= AXIS_DEADMAN_MS && motion_active() && !quick_stopping()) {
        g_axis.homing = false;
        apply_soft_limits();
        quick_stop();
        g_axis.deadman_stop = true;
    }
}

static void on_governor(void)
{
    g_governor_busy = true;
    if (g_start_pending) {
        g_scheduled.result = apply_command(&g_scheduled.command);
        g_scheduled.finished = true;
        g_start_pending = false;
    }

    watch_main_loop();
    watch_limit_switches();
    axis_motion_update(&g_motion, k_segment_seconds);
    finish_drive_request();
    publish_segment();
    g_governor_busy = false;
}

static void emit_step(void)
{
    axis_hw_step_pulse_start();
    busy_wait_us_32(AXIS_STEP_PULSE_US);
    axis_hw_step_pulse_end();
    g_axis.raw_steps += (int32_t)g_axis.direction;
    g_axis.emitted_step_count++;
}

static void begin_segment(void)
{
    const uint32_t published = g_published;

    if (published != g_follow.adopted) {
        g_follow.adopted = published;
        g_follow.segment = g_segments[published & 1u];
    } else {
        // The governor missed its update: hold still rather than guess.
        if (g_follow.segment.start != g_follow.segment.end) {
            g_timing.late_segments++;
        }
        g_follow.segment.start = g_follow.segment.end;
    }
    g_follow.tick = 1;
    irq_set_pending((uint)g_governor_irq);
}

// Steps towards where the segment has got to by this tick: at most one step
// per tick, and a tick's pause whenever DIR changes so the driver sees it
// settle before the PUL edge.
static void follow_segment(void)
{
    const AxisSegment segment = g_follow.segment;
    const int64_t position = segment.start + (segment.end - segment.start) * g_follow.tick / AXIS_TICKS_PER_SEGMENT;
    const int32_t wanted = (int32_t)((position + (1 << (AXIS_Q - 1))) >> AXIS_Q);
    int32_t owed = wanted - g_axis.raw_steps;

    if (owed != 0 && g_axis.outputs_enabled && g_axis.motor_enabled) {
        const int64_t lag = position - (int64_t)g_axis.raw_steps * (1 << AXIS_Q);
        const uint32_t backlog = (uint32_t)(((lag < 0 ? -lag : lag) * 1000) >> AXIS_Q);
        const AxisDirection direction = owed > 0 ? AXIS_DIRECTION_POSITIVE : AXIS_DIRECTION_NEGATIVE;

        if (backlog > g_timing.max_backlog_milli_steps) {
            g_timing.max_backlog_milli_steps = backlog;
        }
        if (direction != g_axis.direction) {
            g_axis.direction = direction;
            axis_hw_set_direction(direction);
        } else {
            if (owed >= 2 || owed <= -2) {
                g_timing.late_steps++;
            }
            g_timing.steps++;
            emit_step();
            owed -= (int32_t)direction;
        }
    }
    g_follow.owed = owed;
}

static void start_scheduled_if_due(uint64_t now_us)
{
    if (!g_scheduled.armed || (int32_t)((uint32_t)now_us - g_scheduled.at_tick_us) < 0) {
        return;
    }

    g_scheduled.armed = false;
    g_scheduled.started_tick_us = (uint32_t)now_us;
    g_start_pending = true;
}

static void tick(uint64_t now_us)
{
    const uint64_t elapsed_us = now_us - g_last_tick_us;
    const bool segment_moving = g_follow.segment.start != g_follow.segment.end;

    g_last_tick_us = now_us;
    if ((segment_moving || g_follow.owed != 0) && elapsed_us > g_timing.max_tick_gap_us) {
        g_timing.max_tick_gap_us = (uint32_t)elapsed_us;
    }

    start_scheduled_if_due(now_us);

    // A start from rest begins a segment on its own tick, so every axis
    // starts exactly one segment after its scheduled tick.
    if (++g_follow.tick > AXIS_TICKS_PER_SEGMENT || (g_start_pending && !segment_moving && !g_governor_busy)) {
        begin_segment();
    }
    follow_segment();
}

static void on_tick_alarm(uint alarm_num)
{
    const uint64_t now_us = time_us_64();

    // Absolute targets keep the rate exact; if we ever fall a whole tick
    // behind, skip ahead rather than queue a burst.
    g_next_tick_us += AXIS_TICK_US;
    if (g_next_tick_us <= now_us) {
        g_next_tick_us = now_us + AXIS_TICK_US;
    }
    while (hardware_alarm_set_target(alarm_num, from_us_since_boot(g_next_tick_us))) {
        g_next_tick_us += AXIS_TICK_US;
    }

    tick(now_us);
}

void axis_service_init(uint64_t now_us)
{
    AxisMotionLimits limits = axis_service_default_limits();

    g_axis = (AxisState){
        .outputs_enabled = true,
        .outputs_wanted = true,
        .direction = AXIS_DIRECTION_POSITIVE,
        .max_velocity_steps_s = AXIS_DEFAULT_MAX_VELOCITY_STEPS_S,
        .acceleration_steps_s2 = AXIS_DEFAULT_ACCELERATION_STEPS_S2,
        .home_velocity_steps_s = AXIS_DEFAULT_HOME_VELOCITY_STEPS_S,
    };
    g_follow = (AxisFollower){0};
    g_published = 0;
    g_published_end = 0;
    g_last_tick_us = now_us;
    axis_motion_sanitise_limits(&limits, AXIS_SERVICE_MAX_STEP_RATE);
    axis_motion_init(&g_motion, &limits, 0.0);

    axis_hw_set_direction(g_axis.direction);
    axis_hw_step_pulse_end();
    update_outputs();

    g_governor_irq = user_irq_claim_unused(true);
    irq_set_exclusive_handler((uint)g_governor_irq, on_governor);
    irq_set_priority((uint)g_governor_irq, AXIS_GOVERNOR_IRQ_PRIORITY);
    irq_set_enabled((uint)g_governor_irq, true);

    g_alarm = hardware_alarm_claim_unused(true);
    hardware_alarm_set_callback((uint)g_alarm, on_tick_alarm);
    irq_set_priority(hardware_alarm_get_irq_num((uint)g_alarm), PICO_HIGHEST_IRQ_PRIORITY);
    g_next_tick_us = time_us_64() + AXIS_TICK_US;
    while (hardware_alarm_set_target((uint)g_alarm, from_us_since_boot(g_next_tick_us))) {
        g_next_tick_us += AXIS_TICK_US;
    }
}

AxisResult axis_service_submit(const AxisCommand *command)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    const AxisResult result = apply_command(command);
    restore_interrupts(interrupts);
    return result;
}

bool axis_service_set_limits(AxisMotionLimits *limits)
{
    const bool usable = axis_motion_sanitise_limits(limits, AXIS_SERVICE_MAX_STEP_RATE);
    const uint32_t interrupts = save_and_disable_interrupts();
    axis_motion_set_limits(&g_motion, limits);
    restore_interrupts(interrupts);
    return usable;
}

int axis_service_window_ms(void)
{
    return g_motion.window;
}

void axis_service_set_soft_limits(int32_t min_steps, int32_t max_steps)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    g_axis.soft_min_steps = min_steps;
    g_axis.soft_max_steps = max_steps;
    apply_soft_limits();
    restore_interrupts(interrupts);
}

void axis_service_heartbeat(void)
{
    g_heartbeat++;
}

bool axis_service_busy(void)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    const bool busy = motion_active() || g_axis.homing || g_scheduled.armed || g_start_pending;
    restore_interrupts(interrupts);
    return busy;
}

bool axis_service_take_deadman_stop(void)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    const bool stopped = g_axis.deadman_stop;
    g_axis.deadman_stop = false;
    restore_interrupts(interrupts);
    return stopped;
}

bool axis_service_take_limit_stop(void)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    const bool stopped = g_axis.limit_stop;
    g_axis.limit_stop = false;
    restore_interrupts(interrupts);
    return stopped;
}

void axis_service_schedule(const AxisCommand *command, uint32_t at_tick_us)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    g_scheduled = (AxisScheduledStart){
        .armed = true,
        .command = *command,
        .at_tick_us = at_tick_us,
    };
    g_start_pending = false;
    restore_interrupts(interrupts);
}

void axis_service_cancel_scheduled(void)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    g_scheduled.armed = false;
    g_scheduled.finished = false;
    // A start already due but not yet applied must not outlive a stop.
    g_start_pending = false;
    restore_interrupts(interrupts);
}

bool axis_service_schedule_armed(void)
{
    return g_scheduled.armed || g_start_pending;
}

bool axis_service_take_scheduled_start(uint32_t *started_tick_us, AxisResult *result)
{
    bool finished;
    const uint32_t interrupts = save_and_disable_interrupts();

    finished = g_scheduled.finished;
    if (finished) {
        *started_tick_us = g_scheduled.started_tick_us;
        *result = g_scheduled.result;
        g_scheduled.finished = false;
    }
    restore_interrupts(interrupts);
    return finished;
}

void axis_service_get_timing(AxisTimingStats *stats)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    *stats = g_timing;
    restore_interrupts(interrupts);
}

void axis_service_reset_timing(void)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    g_timing = (AxisTimingStats){0};
    restore_interrupts(interrupts);
}

void axis_service_get_status(AxisStatus *status)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    const int32_t position = g_axis.raw_steps + g_axis.offset_steps;
    const int32_t target = g_motion.mode == AXIS_MOTION_POSITION
        ? (int32_t)llround(g_motion.target_position) + g_axis.offset_steps
        : position;

    *status = (AxisStatus){
        .outputs_enabled = g_axis.outputs_enabled,
        .motor_enabled = g_axis.motor_enabled,
        .homed = g_axis.homed,
        .moving = motion_active(),
        .homing = g_axis.homing,
        .direction = g_axis.direction,
        .current_position_steps = position,
        .target_position_steps = target,
        .max_velocity_steps_s = g_axis.max_velocity_steps_s,
        .acceleration_steps_s2 = g_axis.acceleration_steps_s2,
        .home_velocity_steps_s = g_axis.home_velocity_steps_s,
        .home_offset_steps = g_axis.home_offset_steps,
        .current_velocity_steps_s = (uint32_t)fabsf(g_motion.velocity),
        .emitted_step_count = g_axis.emitted_step_count,
    };
    restore_interrupts(interrupts);
    status->limits = axis_hw_read_limits();
}
