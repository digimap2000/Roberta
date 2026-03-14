#pragma once

#include "axis_hw.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AXIS_COMMAND_GET_STATUS,
    AXIS_COMMAND_SET_OUTPUT_ENABLE,
    AXIS_COMMAND_SET_MOTOR_ENABLE,
    AXIS_COMMAND_SET_MAX_VELOCITY,
    AXIS_COMMAND_SET_ACCELERATION,
    AXIS_COMMAND_MOVE_TO,
    AXIS_COMMAND_MOVE_BY,
    AXIS_COMMAND_HOME,
    AXIS_COMMAND_SET_ZERO,
    AXIS_COMMAND_STOP,
} AxisCommandType;

typedef struct {
    AxisCommandType type;
    int32_t steps;
    uint32_t rate;
    bool flag;
} AxisCommand;

typedef enum {
    AXIS_RESULT_OK,
    AXIS_RESULT_BUSY,
    AXIS_RESULT_INVALID_ARGUMENT,
    AXIS_RESULT_NOT_HOMED,
} AxisResult;

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
    uint32_t current_velocity_steps_s;
    uint32_t emitted_step_count;
    AxisLimitState limits;
} AxisStatus;

void axis_service_init(uint64_t now_us);
AxisResult axis_service_submit(const AxisCommand *command);
void axis_service_tick(uint64_t now_us);
void axis_service_get_status(AxisStatus *status);
