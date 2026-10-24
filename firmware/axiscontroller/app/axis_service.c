#include "axis_service.h"

#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/time.h"

// Motion runs in a fixed-rate hardware-timer interrupt at the highest
// priority, so WiFi, USB and console work in the main loop cannot delay
// steps. Main-loop entry points touch shared state with interrupts briefly
// disabled.
enum {
    AXIS_TICK_US = 50,
    // CL57T needs at least 2.5 us; leave margin for the buffer and opto.
    AXIS_STEP_PULSE_US = 5,
    // One step per tick at most.
    AXIS_MAX_STEP_RATE = 1000000 / AXIS_TICK_US,
    AXIS_DEFAULT_MAX_VELOCITY_STEPS_S = 400,
    AXIS_DEFAULT_ACCELERATION_STEPS_S2 = 800,
    AXIS_DEFAULT_HOME_VELOCITY_STEPS_S = 150,
};

typedef struct {
    bool outputs_enabled;
    bool motor_enabled;
    bool homed;
    bool moving;
    bool homing;
    AxisDirection direction;
    int32_t current_position_steps;
    int32_t target_position_steps;
    uint32_t max_velocity_steps_s;
    uint32_t acceleration_steps_s2;
    uint32_t home_velocity_steps_s;
    int32_t home_offset_steps;
    float current_velocity_steps_s;
    float step_accumulator;
    uint64_t last_tick_us;
    uint32_t emitted_step_count;
    // Set when DIR changed this tick; the next step waits a tick so the
    // driver sees DIR settle before the PUL edge.
    bool direction_settling;
} AxisState;

typedef struct {
    bool armed;
    bool finished;
    AxisCommand command;
    uint32_t at_tick_us;
    uint32_t started_tick_us;
    AxisResult result;
} AxisScheduledStart;

static AxisState g_axis;
static AxisTimingStats g_timing;
static AxisScheduledStart g_scheduled;
static int g_alarm = -1;
static uint64_t g_next_tick_us;

static float absolute_value(float value)
{
    return value < 0.0f ? -value : value;
}

static int signum(float value)
{
    if (value > 0.0f) {
        return 1;
    }
    if (value < 0.0f) {
        return -1;
    }
    return 0;
}

static void stop_motion(void)
{
    g_axis.moving = false;
    g_axis.homing = false;
    g_axis.current_velocity_steps_s = 0.0f;
    g_axis.step_accumulator = 0.0f;
}

static void update_direction_from_velocity(void)
{
    AxisDirection direction = g_axis.direction;

    if (g_axis.current_velocity_steps_s > 0.0f) {
        direction = AXIS_DIRECTION_POSITIVE;
    } else if (g_axis.current_velocity_steps_s < 0.0f) {
        direction = AXIS_DIRECTION_NEGATIVE;
    }

    if (direction != g_axis.direction) {
        g_axis.direction = direction;
        g_axis.direction_settling = true;
        axis_hw_set_direction(direction);
    }
}

static void update_outputs(void)
{
    axis_hw_set_output_enable(g_axis.outputs_enabled);
    axis_hw_set_motor_enable(g_axis.motor_enabled);
}

static void emit_step(void)
{
    axis_hw_step_pulse_start();
    busy_wait_us_32(AXIS_STEP_PULSE_US);
    axis_hw_step_pulse_end();
    g_axis.current_position_steps += (int32_t)g_axis.direction;
    g_axis.emitted_step_count++;
}

static bool step_would_pass_target(void)
{
    const int velocity_sign = signum(g_axis.current_velocity_steps_s);
    const int32_t next_position = g_axis.current_position_steps + velocity_sign;

    if (velocity_sign > 0) {
        return next_position > g_axis.target_position_steps;
    }

    if (velocity_sign < 0) {
        return next_position < g_axis.target_position_steps;
    }

    return false;
}

static void tick_homing(float dt_seconds, AxisLimitState limits)
{
    const float target_velocity = -(float)g_axis.home_velocity_steps_s;
    const float delta = (float)g_axis.acceleration_steps_s2 * dt_seconds;

    if (limits.home) {
        g_axis.current_position_steps = g_axis.home_offset_steps;
        g_axis.target_position_steps = g_axis.home_offset_steps;
        g_axis.homed = true;
        stop_motion();
        return;
    }

    if (g_axis.current_velocity_steps_s > target_velocity) {
        g_axis.current_velocity_steps_s -= delta;
        if (g_axis.current_velocity_steps_s < target_velocity) {
            g_axis.current_velocity_steps_s = target_velocity;
        }
    }

    update_direction_from_velocity();
    g_axis.step_accumulator += absolute_value(g_axis.current_velocity_steps_s) * dt_seconds;
}

static void tick_move(float dt_seconds)
{
    const int32_t remaining_steps = g_axis.target_position_steps - g_axis.current_position_steps;
    const int direction_to_target = signum((float)remaining_steps);
    const float accel = (float)g_axis.acceleration_steps_s2;
    float acceleration_command = 0.0f;

    if (remaining_steps == 0 && absolute_value(g_axis.current_velocity_steps_s) < 1.0f) {
        stop_motion();
        return;
    }

    if (direction_to_target == 0) {
        acceleration_command = -signum(g_axis.current_velocity_steps_s) * accel;
    } else if (signum(g_axis.current_velocity_steps_s) != 0 &&
               signum(g_axis.current_velocity_steps_s) != direction_to_target) {
        acceleration_command = -signum(g_axis.current_velocity_steps_s) * accel;
    } else {
        const float stopping_distance =
            (g_axis.current_velocity_steps_s * g_axis.current_velocity_steps_s) / (2.0f * accel);

        if (stopping_distance >= absolute_value((float)remaining_steps)) {
            acceleration_command = -direction_to_target * accel;
        } else {
            acceleration_command = direction_to_target * accel;
        }
    }

    g_axis.current_velocity_steps_s += acceleration_command * dt_seconds;

    if (absolute_value(g_axis.current_velocity_steps_s) > (float)g_axis.max_velocity_steps_s) {
        g_axis.current_velocity_steps_s =
            signum(g_axis.current_velocity_steps_s) * (float)g_axis.max_velocity_steps_s;
    }

    if (signum(g_axis.current_velocity_steps_s) != 0 &&
        signum(g_axis.current_velocity_steps_s) != direction_to_target &&
        direction_to_target != 0) {
        g_axis.current_velocity_steps_s = 0.0f;
    }

    update_direction_from_velocity();
    g_axis.step_accumulator += absolute_value(g_axis.current_velocity_steps_s) * dt_seconds;
}

// Retargets the axis to where it can come to rest, so the normal move
// profile ramps it down instead of the velocity dropping to zero at once.
static void begin_controlled_stop(void)
{
    const float speed = absolute_value(g_axis.current_velocity_steps_s);
    const float accel = (float)g_axis.acceleration_steps_s2;

    if ((!g_axis.moving && !g_axis.homing) || speed < 1.0f || accel <= 0.0f) {
        stop_motion();
        g_axis.target_position_steps = g_axis.current_position_steps;
        return;
    }

    const int32_t stopping_steps = (int32_t)((speed * speed) / (2.0f * accel) + 0.999f);
    g_axis.target_position_steps =
        g_axis.current_position_steps + signum(g_axis.current_velocity_steps_s) * stopping_steps;
    g_axis.homing = false;
    g_axis.moving = true;
}

// Applies a command to the motion state. Callers hold interrupts off or run
// in the tick interrupt.
static AxisResult apply_command(const AxisCommand *command)
{
    switch (command->type) {
    case AXIS_COMMAND_GET_STATUS:
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_OUTPUT_ENABLE:
        g_axis.outputs_enabled = command->flag;
        update_outputs();
        if (!g_axis.outputs_enabled) {
            stop_motion();
        }
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_MOTOR_ENABLE:
        g_axis.motor_enabled = command->flag;
        update_outputs();
        if (!g_axis.motor_enabled) {
            stop_motion();
        }
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_MAX_VELOCITY:
        if (command->rate == 0) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.max_velocity_steps_s = command->rate < AXIS_MAX_STEP_RATE ? command->rate : AXIS_MAX_STEP_RATE;
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
        g_axis.home_velocity_steps_s = command->rate < AXIS_MAX_STEP_RATE ? command->rate : AXIS_MAX_STEP_RATE;
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_HOME_OFFSET:
        g_axis.home_offset_steps = command->steps;
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_MOVE_TO:
        if (!g_axis.homed) {
            return AXIS_RESULT_NOT_HOMED;
        }
        if (!g_axis.outputs_enabled || !g_axis.motor_enabled) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.target_position_steps = command->steps;
        g_axis.moving = g_axis.target_position_steps != g_axis.current_position_steps;
        g_axis.homing = false;
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_MOVE_BY:
        if (!g_axis.outputs_enabled || !g_axis.motor_enabled) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.target_position_steps = g_axis.current_position_steps + command->steps;
        g_axis.moving = g_axis.target_position_steps != g_axis.current_position_steps;
        g_axis.homing = false;
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_HOME:
        if (!g_axis.outputs_enabled || !g_axis.motor_enabled) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.homing = true;
        g_axis.moving = false;
        g_axis.current_velocity_steps_s = 0.0f;
        g_axis.step_accumulator = 0.0f;
        g_axis.homed = false;
        g_axis.direction = AXIS_DIRECTION_NEGATIVE;
        g_axis.direction_settling = true;
        axis_hw_set_direction(g_axis.direction);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_ZERO:
        g_axis.current_position_steps = 0;
        g_axis.target_position_steps = 0;
        g_axis.homed = true;
        stop_motion();
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_STOP:
        begin_controlled_stop();
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_HALT:
        stop_motion();
        g_axis.target_position_steps = g_axis.current_position_steps;
        return AXIS_RESULT_OK;
    }

    return AXIS_RESULT_INVALID_ARGUMENT;
}

static void start_scheduled_if_due(uint64_t now_us)
{
    if (!g_scheduled.armed || (int32_t)((uint32_t)now_us - g_scheduled.at_tick_us) < 0) {
        return;
    }

    g_scheduled.armed = false;
    g_scheduled.result = apply_command(&g_scheduled.command);
    g_scheduled.started_tick_us = (uint32_t)now_us;
    g_scheduled.finished = true;
}

static void tick(uint64_t now_us)
{
    const AxisLimitState limits = axis_hw_read_limits();
    const uint64_t elapsed_us = now_us - g_axis.last_tick_us;
    const float dt_seconds = (float)elapsed_us * 1e-6f;

    g_axis.last_tick_us = now_us;

    if ((g_axis.moving || g_axis.homing) && elapsed_us > g_timing.max_tick_gap_us) {
        g_timing.max_tick_gap_us = (uint32_t)elapsed_us;
    }

    start_scheduled_if_due(now_us);

    if (!g_axis.outputs_enabled || !g_axis.motor_enabled) {
        return;
    }

    // Any earlier DIR change has had at least one tick to settle.
    g_axis.direction_settling = false;

    if (g_axis.homing) {
        tick_homing(dt_seconds, limits);
    } else if (g_axis.moving) {
        tick_move(dt_seconds);
    }

    if (g_axis.step_accumulator >= 1.0f && !g_axis.direction_settling) {
        if (!g_axis.homing && step_would_pass_target()) {
            g_axis.current_position_steps = g_axis.target_position_steps;
            stop_motion();
        } else {
            const uint32_t backlog = (uint32_t)(g_axis.step_accumulator * 1000.0f);
            if (backlog > g_timing.max_backlog_milli_steps) {
                g_timing.max_backlog_milli_steps = backlog;
            }
            if (g_axis.step_accumulator >= 2.0f) {
                g_timing.late_steps++;
            }
            g_timing.steps++;
            emit_step();
            g_axis.step_accumulator -= 1.0f;
        }
    }

    if (!g_axis.homing && g_axis.current_position_steps == g_axis.target_position_steps) {
        stop_motion();
    }
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
    g_axis = (AxisState){
        .outputs_enabled = true,
        .motor_enabled = false,
        .direction = AXIS_DIRECTION_POSITIVE,
        .max_velocity_steps_s = AXIS_DEFAULT_MAX_VELOCITY_STEPS_S,
        .acceleration_steps_s2 = AXIS_DEFAULT_ACCELERATION_STEPS_S2,
        .home_velocity_steps_s = AXIS_DEFAULT_HOME_VELOCITY_STEPS_S,
        .last_tick_us = now_us,
    };

    axis_hw_set_direction(g_axis.direction);
    axis_hw_step_pulse_end();
    update_outputs();

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

void axis_service_schedule(const AxisCommand *command, uint32_t at_tick_us)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    g_scheduled = (AxisScheduledStart){
        .armed = true,
        .command = *command,
        .at_tick_us = at_tick_us,
    };
    restore_interrupts(interrupts);
}

void axis_service_cancel_scheduled(void)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    g_scheduled.armed = false;
    g_scheduled.finished = false;
    restore_interrupts(interrupts);
}

bool axis_service_schedule_armed(void)
{
    return g_scheduled.armed;
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
    *status = (AxisStatus){
        .outputs_enabled = g_axis.outputs_enabled,
        .motor_enabled = g_axis.motor_enabled,
        .homed = g_axis.homed,
        .moving = g_axis.moving,
        .homing = g_axis.homing,
        .direction = g_axis.direction,
        .current_position_steps = g_axis.current_position_steps,
        .target_position_steps = g_axis.target_position_steps,
        .max_velocity_steps_s = g_axis.max_velocity_steps_s,
        .acceleration_steps_s2 = g_axis.acceleration_steps_s2,
        .home_velocity_steps_s = g_axis.home_velocity_steps_s,
        .home_offset_steps = g_axis.home_offset_steps,
        .current_velocity_steps_s = (uint32_t)absolute_value(g_axis.current_velocity_steps_s),
        .emitted_step_count = g_axis.emitted_step_count,
    };
    restore_interrupts(interrupts);
    status->limits = axis_hw_read_limits();
}
