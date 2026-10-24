#include "axis_registers.h"

#include "axis_hw.h"
#include "axis_service.h"

#include "pico/time.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    REG_PROTOCOL_VERSION = 0x0000,
    REG_DEVICE_TYPE = 0x0001,
    REG_AXIS_ID = 0x0002,
    REG_CAPABILITY_WORD = 0x0003,
    REG_CONTROL_WORD = 0x0010,
    REG_STATUS_WORD = 0x0011,
    REG_FAULT_WORD = 0x0012,
    REG_MODE_WORD = 0x0013,
    REG_INPUT_WORD = 0x0014,
    REG_OUTPUT_WORD = 0x0015,
    REG_STAGED_COMMAND_COUNTER = 0x0016,
    REG_ACTIVE_COMMAND_COUNTER = 0x0017,
    REG_EXECUTE_AT_TICK_US = 0x0018,
    REG_SEGMENT_DURATION_US = 0x0019,
    REG_STAGED_TARGET_POSITION_STEPS = 0x0020,
    REG_ACTIVE_TARGET_POSITION_STEPS = 0x0021,
    REG_CURRENT_POSITION_STEPS = 0x0022,
    REG_STAGED_TARGET_VELOCITY_STEPS_S = 0x0023,
    REG_ACTIVE_TARGET_VELOCITY_STEPS_S = 0x0024,
    REG_CURRENT_VELOCITY_STEPS_S = 0x0025,
    REG_STAGED_TARGET_ACCEL_STEPS_S2 = 0x0026,
    REG_ACTIVE_TARGET_ACCEL_STEPS_S2 = 0x0027,
    REG_STAGED_HOME_VELOCITY_STEPS_S = 0x0028,
    REG_ACTIVE_HOME_VELOCITY_STEPS_S = 0x0029,
    REG_HOME_OFFSET_STEPS = 0x002A,
    REG_STAGED_RELATIVE_MOVE_STEPS = 0x002B,
    REG_FAN_LEVEL = 0x0030,
    REG_BRAKE_HOLD_ENABLE = 0x0031,
    REG_BRAKE_RELEASE_DELAY_MS = 0x0032,
    REG_BRAKE_APPLY_DELAY_MS = 0x0033,
    REG_STEPS_PER_MOTOR_REV = 0x0040,
    REG_MOTOR_REV_PER_JOINT_REV_NUMERATOR = 0x0041,
    REG_MOTOR_REV_PER_JOINT_REV_DENOMINATOR = 0x0042,
    REG_JOINT_DEGREES_PER_REV_Q12_20 = 0x0043,
    REG_JOINT_BACKLASH_DEG_Q12_20 = 0x0044,
    REG_SOFT_LIMIT_MIN_STEPS = 0x0045,
    REG_SOFT_LIMIT_MAX_STEPS = 0x0046,
    REG_JOINT_POSITION_DEG_Q12_20 = 0x0047,
    REG_STAGED_JOINT_TARGET_DEG_Q12_20 = 0x0048,
    REG_ACTIVE_JOINT_TARGET_DEG_Q12_20 = 0x0049,
    REG_STEP_COUNT = 0x0050,
    REG_UPTIME_MS = 0x0051,
    REG_LOCAL_TICK_US = 0x0052,
    REG_LAST_ERROR_CODE = 0x0053,
    REG_LAST_SYNC_RX_TICK_US = 0x0054,
    REG_SYNC_WATCHDOG_TIMEOUT_MS = 0x0055,
    REG_SYNC_STATUS_WORD = 0x0056,
    REG_LAST_START_TICK_US = 0x0057,
};

enum {
    CAPABILITY_ABSOLUTE_MOVE = 1u << 0,
    CAPABILITY_RELATIVE_MOVE = 1u << 1,
    CAPABILITY_HOMING = 1u << 2,
    CAPABILITY_SOFT_LIMITS = 1u << 3,
    CAPABILITY_FAN = 1u << 4,
    CAPABILITY_BRAKE = 1u << 5,
    CAPABILITY_JOINT_SPACE = 1u << 6,
};

enum {
    CONTROL_OUTPUT_ENABLE = 1u << 0,
    CONTROL_MOTOR_ENABLE = 1u << 1,
    CONTROL_MOVE_ABSOLUTE = 1u << 2,
    CONTROL_MOVE_RELATIVE = 1u << 3,
    CONTROL_HOME = 1u << 4,
    CONTROL_STOP = 1u << 5,
    CONTROL_ESTOP = 1u << 6,
    CONTROL_CLEAR_FAULT = 1u << 7,
    CONTROL_SET_ZERO = 1u << 8,
    CONTROL_MOVE_JOINT = 1u << 9,
    CONTROL_SAVE_CONFIG = 1u << 10,
    CONTROL_RELOAD_CONFIG = 1u << 11,
    CONTROL_LEVEL_MASK = CONTROL_OUTPUT_ENABLE | CONTROL_MOTOR_ENABLE,
    CONTROL_ACTION_MASK = 0x0ffcu,
};

enum {
    STATUS_OUTPUTS_ENABLED = 1u << 0,
    STATUS_MOTOR_ENABLED = 1u << 1,
    STATUS_MOVING = 1u << 2,
    STATUS_HOMING = 1u << 3,
    STATUS_HOMED = 1u << 4,
    STATUS_AT_TARGET = 1u << 5,
    STATUS_FAULT_ACTIVE = 1u << 6,
    STATUS_HOME_ACTIVE = 1u << 7,
    STATUS_LIMIT_A_ACTIVE = 1u << 8,
    STATUS_LIMIT_B_ACTIVE = 1u << 9,
    STATUS_SOFT_LIMIT_ACTIVE = 1u << 10,
    STATUS_BRAKE_APPLIED = 1u << 11,
    STATUS_BRAKE_RELEASED = 1u << 12,
    STATUS_FAN_ACTIVE = 1u << 13,
    STATUS_START_SCHEDULED = 1u << 14,
};

enum {
    FAULT_INVALID_COMMAND = 1u << 0,
    FAULT_MOTION_WHILE_DISABLED = 1u << 1,
    FAULT_MOVE_BEFORE_HOMED = 1u << 2,
    FAULT_SOFT_LIMIT = 1u << 3,
    FAULT_UNEXPECTED_LIMIT = 1u << 4,
    FAULT_HOMING_TIMEOUT = 1u << 5,
    FAULT_BRAKE_INTERLOCK = 1u << 6,
    FAULT_MOTION_ABORTED = 1u << 7,
};

enum {
    MODE_IDLE = 0,
    MODE_STEP_POSITION = 1,
    MODE_HOMING = 2,
    MODE_JOINT_POSITION = 3,
};

enum {
    PROTOCOL_VERSION = 1,
    DEVICE_TYPE_AXIS_CONTROLLER = 1,
    DEFAULT_AXIS_ID = 0,
    DEFAULT_MAX_VELOCITY_STEPS_S = 400,
    DEFAULT_ACCELERATION_STEPS_S2 = 800,
    DEFAULT_HOME_VELOCITY_STEPS_S = 150,
    DEFAULT_SYNC_WATCHDOG_TIMEOUT_MS = 100,
    DEFAULT_STEPS_PER_MOTOR_REV = 200,
    DEFAULT_MOTOR_REV_PER_JOINT_REV_NUMERATOR = 1,
    DEFAULT_MOTOR_REV_PER_JOINT_REV_DENOMINATOR = 1,
    DEFAULT_JOINT_DEGREES_PER_REV_Q12_20 = 377487360,
    ERROR_INVALID_WRITE = 1,
    ERROR_READ_ONLY = 2,
    ERROR_UNSUPPORTED = 3,
    ERROR_MOVE_DISABLED = 101,
    ERROR_MOVE_NOT_HOMED = 102,
    ERROR_SERVICE_BUSY = 103,
    ERROR_SOFT_LIMIT = 104,
    ERROR_LIMIT_ACTIVE = 105,
    ERROR_SCHEDULE_LATE = 106,
    ERROR_SCHEDULE_TOO_FAR = 107,
    // Scheduled starts further ahead than this are refused, which also
    // catches ticks computed from a stale or wrapped clock estimate.
    SCHEDULE_MAX_LEAD_US = 5000000,
};

typedef struct {
    uint32_t control_word_levels;
    uint32_t mode_word;
    uint32_t fault_word;
    uint32_t last_error_code;
    uint32_t axis_id;
    uint32_t staged_command_counter;
    uint32_t active_command_counter;
    uint32_t execute_at_tick_us;
    uint32_t segment_duration_us;
    int32_t staged_target_position_steps;
    uint32_t staged_target_velocity_steps_s;
    uint32_t active_target_velocity_steps_s;
    uint32_t staged_target_accel_steps_s2;
    uint32_t active_target_accel_steps_s2;
    uint32_t staged_home_velocity_steps_s;
    uint32_t active_home_velocity_steps_s;
    int32_t home_offset_steps;
    int32_t staged_relative_move_steps;
    uint32_t fan_level;
    uint32_t brake_hold_enable;
    uint32_t brake_release_delay_ms;
    uint32_t brake_apply_delay_ms;
    uint32_t steps_per_motor_rev;
    uint32_t motor_rev_per_joint_rev_numerator;
    uint32_t motor_rev_per_joint_rev_denominator;
    int32_t joint_degrees_per_rev_q12_20;
    int32_t joint_backlash_deg_q12_20;
    int32_t soft_limit_min_steps;
    int32_t soft_limit_max_steps;
    int32_t staged_joint_target_deg_q12_20;
    int32_t active_joint_target_deg_q12_20;
    uint32_t last_sync_rx_tick_us;
    uint32_t sync_watchdog_timeout_ms;
    uint32_t sync_status_word;
    uint64_t boot_time_us;
    uint64_t local_tick_us;
    // Motion bits of the start armed in the motion service, for bookkeeping
    // once it fires.
    uint32_t scheduled_motion_bits;
    uint32_t last_start_tick_us;
} AxisRegisterState;

static AxisRegisterState g_registers;

static const AxisRegisterInfo k_register_info[] = {
    {REG_PROTOCOL_VERSION, "PROTOCOL_VERSION", AXIS_REGISTER_ACCESS_RO},
    {REG_DEVICE_TYPE, "DEVICE_TYPE", AXIS_REGISTER_ACCESS_RO},
    {REG_AXIS_ID, "AXIS_ID", AXIS_REGISTER_ACCESS_RW},
    {REG_CAPABILITY_WORD, "CAPABILITY_WORD", AXIS_REGISTER_ACCESS_RO},
    {REG_CONTROL_WORD, "CONTROL_WORD", AXIS_REGISTER_ACCESS_RW},
    {REG_STATUS_WORD, "STATUS_WORD", AXIS_REGISTER_ACCESS_RO},
    {REG_FAULT_WORD, "FAULT_WORD", AXIS_REGISTER_ACCESS_RO},
    {REG_MODE_WORD, "MODE_WORD", AXIS_REGISTER_ACCESS_RW},
    {REG_INPUT_WORD, "INPUT_WORD", AXIS_REGISTER_ACCESS_RO},
    {REG_OUTPUT_WORD, "OUTPUT_WORD", AXIS_REGISTER_ACCESS_RO},
    {REG_STAGED_COMMAND_COUNTER, "STAGED_COMMAND_COUNTER", AXIS_REGISTER_ACCESS_RW},
    {REG_ACTIVE_COMMAND_COUNTER, "ACTIVE_COMMAND_COUNTER", AXIS_REGISTER_ACCESS_RO},
    {REG_EXECUTE_AT_TICK_US, "EXECUTE_AT_TICK_US", AXIS_REGISTER_ACCESS_RW},
    {REG_SEGMENT_DURATION_US, "SEGMENT_DURATION_US", AXIS_REGISTER_ACCESS_RW},
    {REG_STAGED_TARGET_POSITION_STEPS, "STAGED_TARGET_POSITION_STEPS", AXIS_REGISTER_ACCESS_RW},
    {REG_ACTIVE_TARGET_POSITION_STEPS, "ACTIVE_TARGET_POSITION_STEPS", AXIS_REGISTER_ACCESS_RO},
    {REG_CURRENT_POSITION_STEPS, "CURRENT_POSITION_STEPS", AXIS_REGISTER_ACCESS_RO},
    {REG_STAGED_TARGET_VELOCITY_STEPS_S, "STAGED_TARGET_VELOCITY_STEPS_S", AXIS_REGISTER_ACCESS_RW},
    {REG_ACTIVE_TARGET_VELOCITY_STEPS_S, "ACTIVE_TARGET_VELOCITY_STEPS_S", AXIS_REGISTER_ACCESS_RO},
    {REG_CURRENT_VELOCITY_STEPS_S, "CURRENT_VELOCITY_STEPS_S", AXIS_REGISTER_ACCESS_RO},
    {REG_STAGED_TARGET_ACCEL_STEPS_S2, "STAGED_TARGET_ACCEL_STEPS_S2", AXIS_REGISTER_ACCESS_RW},
    {REG_ACTIVE_TARGET_ACCEL_STEPS_S2, "ACTIVE_TARGET_ACCEL_STEPS_S2", AXIS_REGISTER_ACCESS_RO},
    {REG_STAGED_HOME_VELOCITY_STEPS_S, "STAGED_HOME_VELOCITY_STEPS_S", AXIS_REGISTER_ACCESS_RW},
    {REG_ACTIVE_HOME_VELOCITY_STEPS_S, "ACTIVE_HOME_VELOCITY_STEPS_S", AXIS_REGISTER_ACCESS_RO},
    {REG_HOME_OFFSET_STEPS, "HOME_OFFSET_STEPS", AXIS_REGISTER_ACCESS_RW},
    {REG_STAGED_RELATIVE_MOVE_STEPS, "STAGED_RELATIVE_MOVE_STEPS", AXIS_REGISTER_ACCESS_RW},
    {REG_FAN_LEVEL, "FAN_LEVEL", AXIS_REGISTER_ACCESS_RW},
    {REG_BRAKE_HOLD_ENABLE, "BRAKE_HOLD_ENABLE", AXIS_REGISTER_ACCESS_RW},
    {REG_BRAKE_RELEASE_DELAY_MS, "BRAKE_RELEASE_DELAY_MS", AXIS_REGISTER_ACCESS_RW},
    {REG_BRAKE_APPLY_DELAY_MS, "BRAKE_APPLY_DELAY_MS", AXIS_REGISTER_ACCESS_RW},
    {REG_STEPS_PER_MOTOR_REV, "STEPS_PER_MOTOR_REV", AXIS_REGISTER_ACCESS_RW},
    {REG_MOTOR_REV_PER_JOINT_REV_NUMERATOR, "MOTOR_REV_PER_JOINT_REV_NUMERATOR", AXIS_REGISTER_ACCESS_RW},
    {REG_MOTOR_REV_PER_JOINT_REV_DENOMINATOR, "MOTOR_REV_PER_JOINT_REV_DENOMINATOR", AXIS_REGISTER_ACCESS_RW},
    {REG_JOINT_DEGREES_PER_REV_Q12_20, "JOINT_DEGREES_PER_REV_Q12_20", AXIS_REGISTER_ACCESS_RW},
    {REG_JOINT_BACKLASH_DEG_Q12_20, "JOINT_BACKLASH_DEG_Q12_20", AXIS_REGISTER_ACCESS_RW},
    {REG_SOFT_LIMIT_MIN_STEPS, "SOFT_LIMIT_MIN_STEPS", AXIS_REGISTER_ACCESS_RW},
    {REG_SOFT_LIMIT_MAX_STEPS, "SOFT_LIMIT_MAX_STEPS", AXIS_REGISTER_ACCESS_RW},
    {REG_JOINT_POSITION_DEG_Q12_20, "JOINT_POSITION_DEG_Q12_20", AXIS_REGISTER_ACCESS_RO},
    {REG_STAGED_JOINT_TARGET_DEG_Q12_20, "STAGED_JOINT_TARGET_DEG_Q12_20", AXIS_REGISTER_ACCESS_RW},
    {REG_ACTIVE_JOINT_TARGET_DEG_Q12_20, "ACTIVE_JOINT_TARGET_DEG_Q12_20", AXIS_REGISTER_ACCESS_RO},
    {REG_STEP_COUNT, "STEP_COUNT", AXIS_REGISTER_ACCESS_RO},
    {REG_UPTIME_MS, "UPTIME_MS", AXIS_REGISTER_ACCESS_RO},
    {REG_LOCAL_TICK_US, "LOCAL_TICK_US", AXIS_REGISTER_ACCESS_RO},
    {REG_LAST_ERROR_CODE, "LAST_ERROR_CODE", AXIS_REGISTER_ACCESS_RO},
    {REG_LAST_SYNC_RX_TICK_US, "LAST_SYNC_RX_TICK_US", AXIS_REGISTER_ACCESS_RO},
    {REG_SYNC_WATCHDOG_TIMEOUT_MS, "SYNC_WATCHDOG_TIMEOUT_MS", AXIS_REGISTER_ACCESS_RW},
    {REG_SYNC_STATUS_WORD, "SYNC_STATUS_WORD", AXIS_REGISTER_ACCESS_RO},
    {REG_LAST_START_TICK_US, "LAST_START_TICK_US", AXIS_REGISTER_ACCESS_RO},
};

static bool names_equal_ignore_case(const char *lhs, const char *rhs)
{
    while (*lhs != '\0' && *rhs != '\0') {
        if (toupper((unsigned char)*lhs) != toupper((unsigned char)*rhs)) {
            return false;
        }
        lhs++;
        rhs++;
    }

    return *lhs == '\0' && *rhs == '\0';
}

static void set_fault(uint32_t fault_bit, uint32_t error_code)
{
    g_registers.fault_word |= fault_bit;
    g_registers.last_error_code = error_code;
}

static void clear_faults(void)
{
    g_registers.fault_word = 0;
    g_registers.last_error_code = 0;
}

static bool soft_limits_configured(void)
{
    return g_registers.soft_limit_min_steps < g_registers.soft_limit_max_steps;
}

static bool position_is_outside_soft_limits(int32_t position_steps)
{
    if (!soft_limits_configured()) {
        return false;
    }

    return position_steps < g_registers.soft_limit_min_steps ||
           position_steps > g_registers.soft_limit_max_steps;
}

static bool soft_limit_blocks_direction(int32_t position_steps, AxisDirection direction)
{
    if (!soft_limits_configured()) {
        return false;
    }

    if (direction == AXIS_DIRECTION_NEGATIVE) {
        return position_steps <= g_registers.soft_limit_min_steps;
    }

    return position_steps >= g_registers.soft_limit_max_steps;
}

static bool target_is_outside_soft_limits(int32_t target_position_steps)
{
    if (!soft_limits_configured()) {
        return false;
    }

    return target_position_steps < g_registers.soft_limit_min_steps ||
           target_position_steps > g_registers.soft_limit_max_steps;
}

static bool limit_blocks_direction(AxisLimitState limits, AxisDirection direction)
{
    if (direction == AXIS_DIRECTION_NEGATIVE) {
        return limits.home || limits.limit_a;
    }

    return limits.limit_b;
}

static bool target_requires_blocked_limit(const AxisStatus *status, int32_t target_position_steps)
{
    if (target_position_steps == status->current_position_steps) {
        return false;
    }

    return target_position_steps < status->current_position_steps
        ? limit_blocks_direction(status->limits, AXIS_DIRECTION_NEGATIVE)
        : limit_blocks_direction(status->limits, AXIS_DIRECTION_POSITIVE);
}

static uint32_t build_capability_word(void)
{
    return CAPABILITY_ABSOLUTE_MOVE |
           CAPABILITY_RELATIVE_MOVE |
           CAPABILITY_HOMING |
           CAPABILITY_SOFT_LIMITS |
           CAPABILITY_FAN;
}

static uint32_t build_input_word(AxisLimitState limits)
{
    uint32_t word = 0;

    if (limits.home) {
        word |= 1u << 0;
    }
    if (limits.limit_a) {
        word |= 1u << 1;
    }
    if (limits.limit_b) {
        word |= 1u << 2;
    }

    return word;
}

static uint32_t build_output_word(AxisOutputState outputs)
{
    uint32_t word = 0;

    if (outputs.outputs_enabled) {
        word |= 1u << 0;
    }
    if (outputs.motor_enabled) {
        word |= 1u << 1;
    }
    if (outputs.direction == AXIS_DIRECTION_POSITIVE) {
        word |= 1u << 2;
    }
    if (outputs.step_active) {
        word |= 1u << 3;
    }
    if (outputs.brake_active) {
        word |= 1u << 4;
    }
    if (outputs.fan_active) {
        word |= 1u << 5;
    }

    return word;
}

static uint32_t build_status_word(const AxisStatus *status, AxisOutputState outputs)
{
    uint32_t word = 0;

    if (status->outputs_enabled) {
        word |= STATUS_OUTPUTS_ENABLED;
    }
    if (status->motor_enabled) {
        word |= STATUS_MOTOR_ENABLED;
    }
    if (status->moving) {
        word |= STATUS_MOVING;
    }
    if (status->homing) {
        word |= STATUS_HOMING;
    }
    if (status->homed) {
        word |= STATUS_HOMED;
    }
    if (!status->moving && !status->homing &&
        status->current_position_steps == status->target_position_steps) {
        word |= STATUS_AT_TARGET;
    }
    if (g_registers.fault_word != 0) {
        word |= STATUS_FAULT_ACTIVE;
    }
    if (status->limits.home) {
        word |= STATUS_HOME_ACTIVE;
    }
    if (status->limits.limit_a) {
        word |= STATUS_LIMIT_A_ACTIVE;
    }
    if (status->limits.limit_b) {
        word |= STATUS_LIMIT_B_ACTIVE;
    }
    if (position_is_outside_soft_limits(status->current_position_steps)) {
        word |= STATUS_SOFT_LIMIT_ACTIVE;
    }
    if (outputs.brake_active) {
        word |= STATUS_BRAKE_APPLIED;
    }
    if (axis_service_schedule_armed()) {
        word |= STATUS_START_SCHEDULED;
    }
    if (outputs.fan_active) {
        word |= STATUS_FAN_ACTIVE;
    }

    return word;
}

static uint32_t current_mode_word(const AxisStatus *status)
{
    if (status->homing) {
        return MODE_HOMING;
    }
    if (status->moving) {
        return MODE_STEP_POSITION;
    }

    return g_registers.mode_word;
}

static int32_t steps_to_joint_degrees_q12_20(int32_t steps)
{
    int64_t numerator;
    int64_t denominator;

    if (g_registers.steps_per_motor_rev == 0 ||
        g_registers.motor_rev_per_joint_rev_numerator == 0 ||
        g_registers.joint_degrees_per_rev_q12_20 == 0) {
        return 0;
    }

    numerator = (int64_t)steps *
                (int64_t)g_registers.joint_degrees_per_rev_q12_20 *
                (int64_t)g_registers.motor_rev_per_joint_rev_denominator;
    denominator = (int64_t)g_registers.steps_per_motor_rev *
                  (int64_t)g_registers.motor_rev_per_joint_rev_numerator;

    if (denominator == 0) {
        return 0;
    }

    return (int32_t)(numerator / denominator);
}

static AxisRegisterResult translate_service_result(AxisResult result)
{
    switch (result) {
    case AXIS_RESULT_OK:
        return AXIS_REGISTER_RESULT_OK;
    case AXIS_RESULT_BUSY:
        g_registers.last_error_code = ERROR_SERVICE_BUSY;
        return AXIS_REGISTER_RESULT_NOT_SUPPORTED;
    case AXIS_RESULT_INVALID_ARGUMENT:
        set_fault(FAULT_MOTION_WHILE_DISABLED, ERROR_MOVE_DISABLED);
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    case AXIS_RESULT_NOT_HOMED:
        set_fault(FAULT_MOVE_BEFORE_HOMED, ERROR_MOVE_NOT_HOMED);
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    }

    set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
    return AXIS_REGISTER_RESULT_INVALID_VALUE;
}

static AxisRegisterResult submit_command(const AxisCommand *command)
{
    return translate_service_result(axis_service_submit(command));
}

static AxisRegisterResult apply_motion_parameters(void)
{
    AxisRegisterResult result;

    result = submit_command(&(AxisCommand){
        .type = AXIS_COMMAND_SET_MAX_VELOCITY,
        .rate = g_registers.staged_target_velocity_steps_s,
    });
    if (result != AXIS_REGISTER_RESULT_OK) {
        return result;
    }

    result = submit_command(&(AxisCommand){
        .type = AXIS_COMMAND_SET_ACCELERATION,
        .rate = g_registers.staged_target_accel_steps_s2,
    });
    if (result != AXIS_REGISTER_RESULT_OK) {
        return result;
    }

    result = submit_command(&(AxisCommand){
        .type = AXIS_COMMAND_SET_HOME_VELOCITY,
        .rate = g_registers.staged_home_velocity_steps_s,
    });
    if (result != AXIS_REGISTER_RESULT_OK) {
        return result;
    }

    return submit_command(&(AxisCommand){
        .type = AXIS_COMMAND_SET_HOME_OFFSET,
        .steps = g_registers.home_offset_steps,
    });
}

static uint32_t local_tick_now(void)
{
    return (uint32_t)time_us_64();
}

// A move with a non-zero EXECUTE_AT_TICK_US is armed and started by
// axis_registers_tick when the local tick reaches it.
static AxisRegisterResult validate_schedule(uint32_t action_bits)
{
    const uint32_t motion_actions = action_bits & (CONTROL_MOVE_ABSOLUTE | CONTROL_MOVE_RELATIVE | CONTROL_HOME);
    int32_t lead_us;

    if (motion_actions == 0 || g_registers.execute_at_tick_us == 0) {
        return AXIS_REGISTER_RESULT_OK;
    }

    lead_us = (int32_t)(g_registers.execute_at_tick_us - local_tick_now());
    if (lead_us <= 0) {
        set_fault(FAULT_INVALID_COMMAND, ERROR_SCHEDULE_LATE);
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    }
    if (lead_us > SCHEDULE_MAX_LEAD_US) {
        set_fault(FAULT_INVALID_COMMAND, ERROR_SCHEDULE_TOO_FAR);
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    }

    return AXIS_REGISTER_RESULT_OK;
}

static void cancel_scheduled_start(void)
{
    axis_service_cancel_scheduled();
    g_registers.scheduled_motion_bits = 0;
}

static AxisRegisterResult validate_motion_request(
    const AxisStatus *status,
    uint32_t requested_levels,
    uint32_t action_bits)
{
    const bool outputs_enabled = (requested_levels & CONTROL_OUTPUT_ENABLE) != 0;
    const bool motor_enabled = (requested_levels & CONTROL_MOTOR_ENABLE) != 0;
    const uint32_t motion_actions =
        action_bits & (CONTROL_MOVE_ABSOLUTE | CONTROL_MOVE_RELATIVE | CONTROL_HOME);
    int32_t target_position_steps = status->current_position_steps;

    if (motion_actions == 0) {
        return AXIS_REGISTER_RESULT_OK;
    }

    if ((motion_actions & (motion_actions - 1u)) != 0) {
        set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    }

    if (!outputs_enabled || !motor_enabled) {
        set_fault(FAULT_MOTION_WHILE_DISABLED, ERROR_MOVE_DISABLED);
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    }

    if (action_bits & CONTROL_HOME) {
        return AXIS_REGISTER_RESULT_OK;
    }

    if ((action_bits & CONTROL_MOVE_ABSOLUTE) != 0) {
        if (!status->homed) {
            set_fault(FAULT_MOVE_BEFORE_HOMED, ERROR_MOVE_NOT_HOMED);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        target_position_steps = g_registers.staged_target_position_steps;
    } else if ((action_bits & CONTROL_MOVE_RELATIVE) != 0) {
        target_position_steps = status->current_position_steps + g_registers.staged_relative_move_steps;
    }

    if (target_is_outside_soft_limits(target_position_steps)) {
        set_fault(FAULT_SOFT_LIMIT, ERROR_SOFT_LIMIT);
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    }

    if (target_requires_blocked_limit(status, target_position_steps)) {
        set_fault(FAULT_UNEXPECTED_LIMIT, ERROR_LIMIT_ACTIVE);
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    }

    return AXIS_REGISTER_RESULT_OK;
}

static AxisRegisterResult validate_control_word_request(
    const AxisStatus *status,
    uint32_t requested_levels,
    uint32_t action_bits)
{
    AxisRegisterResult result;

    if ((action_bits & CONTROL_MOVE_JOINT) != 0 ||
        (action_bits & CONTROL_SAVE_CONFIG) != 0 ||
        (action_bits & CONTROL_RELOAD_CONFIG) != 0) {
        set_fault(FAULT_INVALID_COMMAND, ERROR_UNSUPPORTED);
        return AXIS_REGISTER_RESULT_NOT_SUPPORTED;
    }

    result = validate_motion_request(status, requested_levels, action_bits);
    if (result != AXIS_REGISTER_RESULT_OK) {
        return result;
    }

    return validate_schedule(action_bits);
}

static void snapshot_active_step_command(const AxisStatus *status)
{
    g_registers.active_command_counter = g_registers.staged_command_counter;
    g_registers.active_target_velocity_steps_s = g_registers.staged_target_velocity_steps_s;
    g_registers.active_target_accel_steps_s2 = g_registers.staged_target_accel_steps_s2;
    g_registers.active_home_velocity_steps_s = g_registers.staged_home_velocity_steps_s;
    g_registers.active_joint_target_deg_q12_20 = g_registers.staged_joint_target_deg_q12_20;
    g_registers.mode_word = current_mode_word(status);
}

static AxisCommand motion_command(uint32_t motion_bits)
{
    if (motion_bits & CONTROL_HOME) {
        return (AxisCommand){.type = AXIS_COMMAND_HOME};
    }
    if (motion_bits & CONTROL_MOVE_ABSOLUTE) {
        return (AxisCommand){.type = AXIS_COMMAND_MOVE_TO, .steps = g_registers.staged_target_position_steps};
    }
    return (AxisCommand){.type = AXIS_COMMAND_MOVE_BY, .steps = g_registers.staged_relative_move_steps};
}

static void note_motion_started(uint32_t motion_bits, uint32_t started_tick_us)
{
    AxisStatus status;

    g_registers.last_start_tick_us = started_tick_us;
    axis_service_get_status(&status);
    snapshot_active_step_command(&status);
    g_registers.mode_word = (motion_bits & CONTROL_HOME) ? MODE_HOMING : MODE_STEP_POSITION;
}

static AxisRegisterResult start_motion(uint32_t motion_bits)
{
    const AxisCommand command = motion_command(motion_bits);
    const AxisRegisterResult result = submit_command(&command);

    if (result == AXIS_REGISTER_RESULT_OK) {
        note_motion_started(motion_bits, local_tick_now());
    }
    return result;
}

static AxisRegisterResult write_control_word(uint32_t value)
{
    AxisStatus status;
    AxisRegisterResult result = AXIS_REGISTER_RESULT_OK;
    uint32_t requested_levels = value & CONTROL_LEVEL_MASK;
    uint32_t action_bits = value & CONTROL_ACTION_MASK;
    uint32_t motion_bits;

    axis_service_get_status(&status);

    result = validate_control_word_request(&status, requested_levels, action_bits);
    if (result != AXIS_REGISTER_RESULT_OK) {
        return result;
    }

    if ((requested_levels & CONTROL_OUTPUT_ENABLE) !=
        (g_registers.control_word_levels & CONTROL_OUTPUT_ENABLE)) {
        result = submit_command(&(AxisCommand){
            .type = AXIS_COMMAND_SET_OUTPUT_ENABLE,
            .flag = (requested_levels & CONTROL_OUTPUT_ENABLE) != 0,
        });
        if (result != AXIS_REGISTER_RESULT_OK) {
            return result;
        }
    }

    if ((requested_levels & CONTROL_MOTOR_ENABLE) !=
        (g_registers.control_word_levels & CONTROL_MOTOR_ENABLE)) {
        result = submit_command(&(AxisCommand){
            .type = AXIS_COMMAND_SET_MOTOR_ENABLE,
            .flag = (requested_levels & CONTROL_MOTOR_ENABLE) != 0,
        });
        if (result != AXIS_REGISTER_RESULT_OK) {
            return result;
        }
    }

    g_registers.control_word_levels = requested_levels;
    if ((requested_levels & CONTROL_LEVEL_MASK) != CONTROL_LEVEL_MASK) {
        cancel_scheduled_start();
    }

    if (action_bits == 0) {
        return AXIS_REGISTER_RESULT_OK;
    }

    if (action_bits & CONTROL_CLEAR_FAULT) {
        clear_faults();
    }

    if ((action_bits & (CONTROL_MOVE_ABSOLUTE | CONTROL_MOVE_RELATIVE | CONTROL_HOME)) != 0) {
        result = apply_motion_parameters();
        if (result != AXIS_REGISTER_RESULT_OK) {
            return result;
        }
    }

    if (action_bits & (CONTROL_STOP | CONTROL_ESTOP)) {
        cancel_scheduled_start();
    }

    if (action_bits & CONTROL_STOP) {
        result = submit_command(&(AxisCommand){.type = AXIS_COMMAND_STOP});
        if (result != AXIS_REGISTER_RESULT_OK) {
            return result;
        }
        g_registers.mode_word = MODE_IDLE;
    }

    if (action_bits & CONTROL_ESTOP) {
        result = submit_command(&(AxisCommand){.type = AXIS_COMMAND_STOP});
        if (result != AXIS_REGISTER_RESULT_OK) {
            return result;
        }
        set_fault(FAULT_MOTION_ABORTED, ERROR_UNSUPPORTED);
        g_registers.mode_word = MODE_IDLE;
    }

    if (action_bits & CONTROL_SET_ZERO) {
        result = submit_command(&(AxisCommand){.type = AXIS_COMMAND_SET_ZERO});
        if (result != AXIS_REGISTER_RESULT_OK) {
            return result;
        }
        g_registers.mode_word = MODE_IDLE;
    }

    motion_bits = action_bits & (CONTROL_MOVE_ABSOLUTE | CONTROL_MOVE_RELATIVE | CONTROL_HOME);
    if (motion_bits == 0) {
        return AXIS_REGISTER_RESULT_OK;
    }

    // A newer motion command replaces any start still waiting.
    cancel_scheduled_start();

    if (g_registers.execute_at_tick_us != 0) {
        // The motion interrupt starts it on the exact tick.
        const AxisCommand command = motion_command(motion_bits);
        axis_service_schedule(&command, g_registers.execute_at_tick_us);
        g_registers.scheduled_motion_bits = motion_bits;
        return AXIS_REGISTER_RESULT_OK;
    }

    return start_motion(motion_bits);
}

void axis_registers_init(uint64_t now_us)
{
    g_registers = (AxisRegisterState){
        .axis_id = DEFAULT_AXIS_ID,
        .control_word_levels = CONTROL_OUTPUT_ENABLE,
        .mode_word = MODE_IDLE,
        .staged_target_velocity_steps_s = DEFAULT_MAX_VELOCITY_STEPS_S,
        .staged_target_accel_steps_s2 = DEFAULT_ACCELERATION_STEPS_S2,
        .staged_home_velocity_steps_s = DEFAULT_HOME_VELOCITY_STEPS_S,
        .fan_level = 0,
        .brake_hold_enable = 1,
        .steps_per_motor_rev = DEFAULT_STEPS_PER_MOTOR_REV,
        .motor_rev_per_joint_rev_numerator = DEFAULT_MOTOR_REV_PER_JOINT_REV_NUMERATOR,
        .motor_rev_per_joint_rev_denominator = DEFAULT_MOTOR_REV_PER_JOINT_REV_DENOMINATOR,
        .joint_degrees_per_rev_q12_20 = DEFAULT_JOINT_DEGREES_PER_REV_Q12_20,
        .sync_watchdog_timeout_ms = DEFAULT_SYNC_WATCHDOG_TIMEOUT_MS,
        .boot_time_us = now_us,
        .local_tick_us = now_us,
    };

    axis_hw_set_fan(false);
    axis_hw_set_brake(false);
}

void axis_registers_tick(uint64_t now_us)
{
    AxisStatus status;

    g_registers.local_tick_us = now_us;

    uint32_t started_tick_us;
    AxisResult scheduled_result;
    if (axis_service_take_scheduled_start(&started_tick_us, &scheduled_result)) {
        if (translate_service_result(scheduled_result) == AXIS_REGISTER_RESULT_OK) {
            note_motion_started(g_registers.scheduled_motion_bits, started_tick_us);
        }
        g_registers.scheduled_motion_bits = 0;
    }

    axis_service_get_status(&status);
    if (status.homing) {
        return;
    }

    if (status.moving && limit_blocks_direction(status.limits, status.direction)) {
        (void)submit_command(&(AxisCommand){.type = AXIS_COMMAND_STOP});
        set_fault(FAULT_UNEXPECTED_LIMIT, ERROR_LIMIT_ACTIVE);
        g_registers.mode_word = MODE_IDLE;
        return;
    }

    if (status.moving && soft_limit_blocks_direction(status.current_position_steps, status.direction)) {
        (void)submit_command(&(AxisCommand){.type = AXIS_COMMAND_STOP});
        set_fault(FAULT_SOFT_LIMIT, ERROR_SOFT_LIMIT);
        g_registers.mode_word = MODE_IDLE;
    }
}

AxisRegisterResult axis_registers_read(uint16_t address, uint32_t *value)
{
    AxisStatus status;
    AxisOutputState outputs;

    if (value == NULL) {
        return AXIS_REGISTER_RESULT_INVALID_VALUE;
    }

    axis_service_get_status(&status);
    outputs = axis_hw_get_output_state();

    switch (address) {
    case REG_PROTOCOL_VERSION:
        *value = PROTOCOL_VERSION;
        return AXIS_REGISTER_RESULT_OK;
    case REG_DEVICE_TYPE:
        *value = DEVICE_TYPE_AXIS_CONTROLLER;
        return AXIS_REGISTER_RESULT_OK;
    case REG_AXIS_ID:
        *value = g_registers.axis_id;
        return AXIS_REGISTER_RESULT_OK;
    case REG_CAPABILITY_WORD:
        *value = build_capability_word();
        return AXIS_REGISTER_RESULT_OK;
    case REG_CONTROL_WORD:
        *value = g_registers.control_word_levels;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STATUS_WORD:
        *value = build_status_word(&status, outputs);
        return AXIS_REGISTER_RESULT_OK;
    case REG_FAULT_WORD:
        *value = g_registers.fault_word;
        return AXIS_REGISTER_RESULT_OK;
    case REG_MODE_WORD:
        *value = current_mode_word(&status);
        return AXIS_REGISTER_RESULT_OK;
    case REG_INPUT_WORD:
        *value = build_input_word(status.limits);
        return AXIS_REGISTER_RESULT_OK;
    case REG_OUTPUT_WORD:
        *value = build_output_word(outputs);
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_COMMAND_COUNTER:
        *value = g_registers.staged_command_counter;
        return AXIS_REGISTER_RESULT_OK;
    case REG_ACTIVE_COMMAND_COUNTER:
        *value = g_registers.active_command_counter;
        return AXIS_REGISTER_RESULT_OK;
    case REG_EXECUTE_AT_TICK_US:
        *value = g_registers.execute_at_tick_us;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SEGMENT_DURATION_US:
        *value = g_registers.segment_duration_us;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_TARGET_POSITION_STEPS:
        *value = (uint32_t)g_registers.staged_target_position_steps;
        return AXIS_REGISTER_RESULT_OK;
    case REG_ACTIVE_TARGET_POSITION_STEPS:
        *value = (uint32_t)status.target_position_steps;
        return AXIS_REGISTER_RESULT_OK;
    case REG_CURRENT_POSITION_STEPS:
        *value = (uint32_t)status.current_position_steps;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_TARGET_VELOCITY_STEPS_S:
        *value = g_registers.staged_target_velocity_steps_s;
        return AXIS_REGISTER_RESULT_OK;
    case REG_ACTIVE_TARGET_VELOCITY_STEPS_S:
        *value = g_registers.active_target_velocity_steps_s;
        return AXIS_REGISTER_RESULT_OK;
    case REG_CURRENT_VELOCITY_STEPS_S:
        *value = status.current_velocity_steps_s;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_TARGET_ACCEL_STEPS_S2:
        *value = g_registers.staged_target_accel_steps_s2;
        return AXIS_REGISTER_RESULT_OK;
    case REG_ACTIVE_TARGET_ACCEL_STEPS_S2:
        *value = g_registers.active_target_accel_steps_s2;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_HOME_VELOCITY_STEPS_S:
        *value = g_registers.staged_home_velocity_steps_s;
        return AXIS_REGISTER_RESULT_OK;
    case REG_ACTIVE_HOME_VELOCITY_STEPS_S:
        *value = g_registers.active_home_velocity_steps_s;
        return AXIS_REGISTER_RESULT_OK;
    case REG_HOME_OFFSET_STEPS:
        *value = (uint32_t)g_registers.home_offset_steps;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_RELATIVE_MOVE_STEPS:
        *value = (uint32_t)g_registers.staged_relative_move_steps;
        return AXIS_REGISTER_RESULT_OK;
    case REG_FAN_LEVEL:
        *value = g_registers.fan_level;
        return AXIS_REGISTER_RESULT_OK;
    case REG_BRAKE_HOLD_ENABLE:
        *value = g_registers.brake_hold_enable;
        return AXIS_REGISTER_RESULT_OK;
    case REG_BRAKE_RELEASE_DELAY_MS:
        *value = g_registers.brake_release_delay_ms;
        return AXIS_REGISTER_RESULT_OK;
    case REG_BRAKE_APPLY_DELAY_MS:
        *value = g_registers.brake_apply_delay_ms;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STEPS_PER_MOTOR_REV:
        *value = g_registers.steps_per_motor_rev;
        return AXIS_REGISTER_RESULT_OK;
    case REG_MOTOR_REV_PER_JOINT_REV_NUMERATOR:
        *value = g_registers.motor_rev_per_joint_rev_numerator;
        return AXIS_REGISTER_RESULT_OK;
    case REG_MOTOR_REV_PER_JOINT_REV_DENOMINATOR:
        *value = g_registers.motor_rev_per_joint_rev_denominator;
        return AXIS_REGISTER_RESULT_OK;
    case REG_JOINT_DEGREES_PER_REV_Q12_20:
        *value = (uint32_t)g_registers.joint_degrees_per_rev_q12_20;
        return AXIS_REGISTER_RESULT_OK;
    case REG_JOINT_BACKLASH_DEG_Q12_20:
        *value = (uint32_t)g_registers.joint_backlash_deg_q12_20;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SOFT_LIMIT_MIN_STEPS:
        *value = (uint32_t)g_registers.soft_limit_min_steps;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SOFT_LIMIT_MAX_STEPS:
        *value = (uint32_t)g_registers.soft_limit_max_steps;
        return AXIS_REGISTER_RESULT_OK;
    case REG_JOINT_POSITION_DEG_Q12_20:
        *value = (uint32_t)steps_to_joint_degrees_q12_20(status.current_position_steps);
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_JOINT_TARGET_DEG_Q12_20:
        *value = (uint32_t)g_registers.staged_joint_target_deg_q12_20;
        return AXIS_REGISTER_RESULT_OK;
    case REG_ACTIVE_JOINT_TARGET_DEG_Q12_20:
        *value = (uint32_t)g_registers.active_joint_target_deg_q12_20;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STEP_COUNT:
        *value = status.emitted_step_count;
        return AXIS_REGISTER_RESULT_OK;
    case REG_UPTIME_MS:
        *value = (uint32_t)((g_registers.local_tick_us - g_registers.boot_time_us) / 1000u);
        return AXIS_REGISTER_RESULT_OK;
    case REG_LAST_START_TICK_US:
        *value = g_registers.last_start_tick_us;
        return AXIS_REGISTER_RESULT_OK;
    case REG_LOCAL_TICK_US:
        // Read the timer directly: hosts estimate clock offsets from it.
        *value = local_tick_now();
        return AXIS_REGISTER_RESULT_OK;
    case REG_LAST_ERROR_CODE:
        *value = g_registers.last_error_code;
        return AXIS_REGISTER_RESULT_OK;
    case REG_LAST_SYNC_RX_TICK_US:
        *value = g_registers.last_sync_rx_tick_us;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SYNC_WATCHDOG_TIMEOUT_MS:
        *value = g_registers.sync_watchdog_timeout_ms;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SYNC_STATUS_WORD:
        *value = g_registers.sync_status_word;
        return AXIS_REGISTER_RESULT_OK;
    }

    return AXIS_REGISTER_RESULT_NOT_FOUND;
}

AxisRegisterResult axis_registers_write(uint16_t address, uint32_t value)
{
    switch (address) {
    case REG_AXIS_ID:
        g_registers.axis_id = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_CONTROL_WORD:
        return write_control_word(value);
    case REG_MODE_WORD:
        if (value > MODE_JOINT_POSITION) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.mode_word = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_COMMAND_COUNTER:
        g_registers.staged_command_counter = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_EXECUTE_AT_TICK_US:
        g_registers.execute_at_tick_us = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SEGMENT_DURATION_US:
        g_registers.segment_duration_us = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_TARGET_POSITION_STEPS:
        g_registers.staged_target_position_steps = (int32_t)value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_TARGET_VELOCITY_STEPS_S:
        if (value == 0) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.staged_target_velocity_steps_s = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_TARGET_ACCEL_STEPS_S2:
        if (value == 0) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.staged_target_accel_steps_s2 = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_HOME_VELOCITY_STEPS_S:
        if (value == 0) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.staged_home_velocity_steps_s = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_HOME_OFFSET_STEPS:
        g_registers.home_offset_steps = (int32_t)value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_RELATIVE_MOVE_STEPS:
        g_registers.staged_relative_move_steps = (int32_t)value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_FAN_LEVEL:
        if (value > 1000u) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.fan_level = value;
        axis_hw_set_fan(value != 0);
        return AXIS_REGISTER_RESULT_OK;
    case REG_BRAKE_HOLD_ENABLE:
        if (value > 1u) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.brake_hold_enable = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_BRAKE_RELEASE_DELAY_MS:
        g_registers.brake_release_delay_ms = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_BRAKE_APPLY_DELAY_MS:
        g_registers.brake_apply_delay_ms = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STEPS_PER_MOTOR_REV:
        if (value == 0) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.steps_per_motor_rev = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_MOTOR_REV_PER_JOINT_REV_NUMERATOR:
        if (value == 0) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.motor_rev_per_joint_rev_numerator = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_MOTOR_REV_PER_JOINT_REV_DENOMINATOR:
        if (value == 0) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.motor_rev_per_joint_rev_denominator = value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_JOINT_DEGREES_PER_REV_Q12_20:
        g_registers.joint_degrees_per_rev_q12_20 = (int32_t)value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_JOINT_BACKLASH_DEG_Q12_20:
        g_registers.joint_backlash_deg_q12_20 = (int32_t)value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SOFT_LIMIT_MIN_STEPS:
        g_registers.soft_limit_min_steps = (int32_t)value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SOFT_LIMIT_MAX_STEPS:
        g_registers.soft_limit_max_steps = (int32_t)value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_STAGED_JOINT_TARGET_DEG_Q12_20:
        g_registers.staged_joint_target_deg_q12_20 = (int32_t)value;
        return AXIS_REGISTER_RESULT_OK;
    case REG_SYNC_WATCHDOG_TIMEOUT_MS:
        if (value == 0) {
            set_fault(FAULT_INVALID_COMMAND, ERROR_INVALID_WRITE);
            return AXIS_REGISTER_RESULT_INVALID_VALUE;
        }
        g_registers.sync_watchdog_timeout_ms = value;
        return AXIS_REGISTER_RESULT_OK;
    }

    if (axis_registers_read(address, &value) == AXIS_REGISTER_RESULT_OK) {
        g_registers.last_error_code = ERROR_READ_ONLY;
        return AXIS_REGISTER_RESULT_READ_ONLY;
    }

    g_registers.last_error_code = ERROR_INVALID_WRITE;
    return AXIS_REGISTER_RESULT_NOT_FOUND;
}

AxisRegisterResult axis_registers_bit_set(uint16_t address, uint32_t mask)
{
    uint32_t current_value;
    AxisRegisterResult result;

    if (address == REG_CONTROL_WORD) {
        return write_control_word(g_registers.control_word_levels | mask);
    }

    result = axis_registers_read(address, &current_value);
    if (result != AXIS_REGISTER_RESULT_OK) {
        return result;
    }

    return axis_registers_write(address, current_value | mask);
}

AxisRegisterResult axis_registers_bit_clear(uint16_t address, uint32_t mask)
{
    uint32_t current_value;
    AxisRegisterResult result;

    if (address == REG_CONTROL_WORD) {
        return write_control_word(g_registers.control_word_levels & ~mask);
    }

    result = axis_registers_read(address, &current_value);
    if (result != AXIS_REGISTER_RESULT_OK) {
        return result;
    }

    return axis_registers_write(address, current_value & ~mask);
}

bool axis_registers_lookup_address(const char *token, uint16_t *address)
{
    size_t index;
    char *end = NULL;
    unsigned long parsed;

    if (token == NULL || address == NULL) {
        return false;
    }

    parsed = strtoul(token, &end, 0);
    if (token[0] != '\0' && end != NULL && *end == '\0' && parsed <= 0xffffu) {
        *address = (uint16_t)parsed;
        return true;
    }

    for (index = 0; index < sizeof(k_register_info) / sizeof(k_register_info[0]); index++) {
        if (names_equal_ignore_case(token, k_register_info[index].name)) {
            *address = k_register_info[index].address;
            return true;
        }
    }

    return false;
}

size_t axis_registers_get_count(void)
{
    return sizeof(k_register_info) / sizeof(k_register_info[0]);
}

bool axis_registers_get_info(size_t index, AxisRegisterInfo *info)
{
    if (index >= axis_registers_get_count() || info == NULL) {
        return false;
    }

    *info = k_register_info[index];
    return true;
}

const char *axis_registers_result_to_string(AxisRegisterResult result)
{
    switch (result) {
    case AXIS_REGISTER_RESULT_OK:
        return "ok";
    case AXIS_REGISTER_RESULT_NOT_FOUND:
        return "not-found";
    case AXIS_REGISTER_RESULT_READ_ONLY:
        return "read-only";
    case AXIS_REGISTER_RESULT_INVALID_VALUE:
        return "invalid-value";
    case AXIS_REGISTER_RESULT_NOT_SUPPORTED:
        return "not-supported";
    }

    return "error";
}
