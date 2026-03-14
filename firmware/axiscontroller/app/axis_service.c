#include "axis_service.h"

#include <math.h>

enum {
    AXIS_STEP_PULSE_WIDTH_US = 20,
    AXIS_DEFAULT_MAX_VELOCITY_STEPS_S = 400,
    AXIS_DEFAULT_ACCELERATION_STEPS_S2 = 800,
    AXIS_HOME_VELOCITY_STEPS_S = 150,
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
    double current_velocity_steps_s;
    double step_accumulator;
    uint64_t last_tick_us;
    uint64_t pulse_end_us;
    uint32_t emitted_step_count;
    bool pulse_active;
} AxisState;

static AxisState g_axis;

static double absolute_value(double value)
{
    return value < 0.0 ? -value : value;
}

static int signum(double value)
{
    if (value > 0.0) {
        return 1;
    }
    if (value < 0.0) {
        return -1;
    }
    return 0;
}

static void stop_motion(void)
{
    g_axis.moving = false;
    g_axis.homing = false;
    g_axis.current_velocity_steps_s = 0.0;
    g_axis.step_accumulator = 0.0;
}

static void update_direction_from_velocity(void)
{
    if (g_axis.current_velocity_steps_s > 0.0) {
        g_axis.direction = AXIS_DIRECTION_POSITIVE;
        axis_hw_set_direction(g_axis.direction);
    } else if (g_axis.current_velocity_steps_s < 0.0) {
        g_axis.direction = AXIS_DIRECTION_NEGATIVE;
        axis_hw_set_direction(g_axis.direction);
    }
}

static void update_outputs(void)
{
    axis_hw_set_output_enable(g_axis.outputs_enabled);
    axis_hw_set_motor_enable(g_axis.motor_enabled);
}

static void update_pulse_state(uint64_t now_us)
{
    if (g_axis.pulse_active && now_us >= g_axis.pulse_end_us) {
        axis_hw_step_pulse_end();
        g_axis.pulse_active = false;
    }
}

static void emit_step(uint64_t now_us)
{
    if (g_axis.pulse_active) {
        return;
    }

    update_direction_from_velocity();
    axis_hw_step_pulse_start();
    g_axis.pulse_active = true;
    g_axis.pulse_end_us = now_us + AXIS_STEP_PULSE_WIDTH_US;
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

static void tick_homing(double dt_seconds, AxisLimitState limits)
{
    const double target_velocity = -(double)AXIS_HOME_VELOCITY_STEPS_S;
    const double accel = (double)g_axis.acceleration_steps_s2;
    const double delta = accel * dt_seconds;

    if (limits.home) {
        g_axis.current_position_steps = 0;
        g_axis.target_position_steps = 0;
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

static void tick_move(double dt_seconds)
{
    const int32_t remaining_steps = g_axis.target_position_steps - g_axis.current_position_steps;
    const int direction_to_target = signum((double)remaining_steps);
    const double accel = (double)g_axis.acceleration_steps_s2;
    double acceleration_command = 0.0;

    if (remaining_steps == 0 && absolute_value(g_axis.current_velocity_steps_s) < 1.0) {
        stop_motion();
        return;
    }

    if (direction_to_target == 0) {
        acceleration_command = -signum(g_axis.current_velocity_steps_s) * accel;
    } else if (signum(g_axis.current_velocity_steps_s) != 0 &&
               signum(g_axis.current_velocity_steps_s) != direction_to_target) {
        acceleration_command = -signum(g_axis.current_velocity_steps_s) * accel;
    } else {
        const double stopping_distance =
            (g_axis.current_velocity_steps_s * g_axis.current_velocity_steps_s) / (2.0 * accel);

        if (stopping_distance >= absolute_value((double)remaining_steps)) {
            acceleration_command = -direction_to_target * accel;
        } else {
            acceleration_command = direction_to_target * accel;
        }
    }

    g_axis.current_velocity_steps_s += acceleration_command * dt_seconds;

    if (absolute_value(g_axis.current_velocity_steps_s) > (double)g_axis.max_velocity_steps_s) {
        g_axis.current_velocity_steps_s =
            signum(g_axis.current_velocity_steps_s) * (double)g_axis.max_velocity_steps_s;
    }

    if (signum(g_axis.current_velocity_steps_s) != 0 &&
        signum(g_axis.current_velocity_steps_s) != direction_to_target &&
        direction_to_target != 0) {
        g_axis.current_velocity_steps_s = 0.0;
    }

    update_direction_from_velocity();
    g_axis.step_accumulator += absolute_value(g_axis.current_velocity_steps_s) * dt_seconds;
}

void axis_service_init(uint64_t now_us)
{
    g_axis = (AxisState){
        .outputs_enabled = true,
        .motor_enabled = false,
        .homed = false,
        .moving = false,
        .homing = false,
        .direction = AXIS_DIRECTION_POSITIVE,
        .current_position_steps = 0,
        .target_position_steps = 0,
        .max_velocity_steps_s = AXIS_DEFAULT_MAX_VELOCITY_STEPS_S,
        .acceleration_steps_s2 = AXIS_DEFAULT_ACCELERATION_STEPS_S2,
        .current_velocity_steps_s = 0.0,
        .step_accumulator = 0.0,
        .last_tick_us = now_us,
        .pulse_end_us = 0,
        .emitted_step_count = 0,
        .pulse_active = false,
    };

    axis_hw_set_direction(g_axis.direction);
    axis_hw_step_pulse_end();
    update_outputs();
}

AxisResult axis_service_submit(const AxisCommand *command)
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
        g_axis.max_velocity_steps_s = command->rate;
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_ACCELERATION:
        if (command->rate == 0) {
            return AXIS_RESULT_INVALID_ARGUMENT;
        }
        g_axis.acceleration_steps_s2 = command->rate;
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
        g_axis.current_velocity_steps_s = 0.0;
        g_axis.step_accumulator = 0.0;
        g_axis.homed = false;
        g_axis.direction = AXIS_DIRECTION_NEGATIVE;
        axis_hw_set_direction(g_axis.direction);
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_SET_ZERO:
        g_axis.current_position_steps = 0;
        g_axis.target_position_steps = 0;
        g_axis.homed = true;
        stop_motion();
        return AXIS_RESULT_OK;

    case AXIS_COMMAND_STOP:
        stop_motion();
        g_axis.target_position_steps = g_axis.current_position_steps;
        return AXIS_RESULT_OK;
    }

    return AXIS_RESULT_INVALID_ARGUMENT;
}

void axis_service_tick(uint64_t now_us)
{
    AxisLimitState limits = axis_hw_read_limits();
    const uint64_t elapsed_us = now_us - g_axis.last_tick_us;
    const double dt_seconds = (double)elapsed_us / 1000000.0;

    g_axis.last_tick_us = now_us;
    update_outputs();
    update_pulse_state(now_us);

    if (!g_axis.outputs_enabled || !g_axis.motor_enabled) {
        return;
    }

    if (g_axis.homing) {
        tick_homing(dt_seconds, limits);
    } else if (g_axis.moving) {
        tick_move(dt_seconds);
    }

    if (g_axis.step_accumulator >= 1.0) {
        if (!g_axis.homing && step_would_pass_target()) {
            g_axis.current_position_steps = g_axis.target_position_steps;
            stop_motion();
        } else {
            emit_step(now_us);
            g_axis.step_accumulator -= 1.0;
        }
    }

    if (!g_axis.homing &&
        g_axis.current_position_steps == g_axis.target_position_steps &&
        absolute_value(g_axis.current_velocity_steps_s) < 1.0) {
        stop_motion();
    }

    if (!g_axis.homing && g_axis.current_position_steps == g_axis.target_position_steps) {
        stop_motion();
    }
}

void axis_service_get_status(AxisStatus *status)
{
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
        .current_velocity_steps_s = (uint32_t)absolute_value(g_axis.current_velocity_steps_s),
        .emitted_step_count = g_axis.emitted_step_count,
        .limits = axis_hw_read_limits(),
    };
}
