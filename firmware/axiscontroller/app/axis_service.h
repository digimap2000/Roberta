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
    AXIS_COMMAND_SET_HOME_VELOCITY,
    AXIS_COMMAND_SET_HOME_OFFSET,
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
    uint32_t home_velocity_steps_s;
    int32_t home_offset_steps;
    uint32_t current_velocity_steps_s;
    uint32_t emitted_step_count;
    AxisLimitState limits;
} AxisStatus;

// Step timing while moving: how far the polled step loop falls behind.
// A step is "late" when at least one further step was already owed when it
// was emitted, i.e. it came out a full step period or more behind schedule.
typedef struct {
    uint32_t steps;
    uint32_t late_steps;
    uint32_t max_tick_gap_us;
    uint32_t max_backlog_milli_steps;
} AxisTimingStats;

void axis_service_get_timing(AxisTimingStats *stats);
void axis_service_reset_timing(void);

// Starts the 50 us motion interrupt; motion then runs without main-loop help.
void axis_service_init(uint64_t now_us);
AxisResult axis_service_submit(const AxisCommand *command);
void axis_service_get_status(AxisStatus *status);

// Arms a command to start, from the motion interrupt, when the 32-bit local
// microsecond tick reaches at_tick_us. A newer schedule replaces it.
void axis_service_schedule(const AxisCommand *command, uint32_t at_tick_us);
void axis_service_cancel_scheduled(void);
bool axis_service_schedule_armed(void);
// Reports, once, that a scheduled command was applied: when, and its result.
bool axis_service_take_scheduled_start(uint32_t *started_tick_us, AxisResult *result);
