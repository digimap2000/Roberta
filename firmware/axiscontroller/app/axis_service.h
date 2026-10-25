#pragma once

#include "axis_hw.h"
#include "axis_motion.h"

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
    // Stop levels. All motion passes through the motion governor
    // (axis_motion.h), which keeps it within the axis's limits; only KILL
    // bypasses it.
    // STOP: ramp down at the move's own deceleration, then hold.
    AXIS_COMMAND_STOP,
    // QUICK_STOP: ramp down at the axis's quick-stop deceleration, the
    // hardest it is commissioned to take, then hold. For limit switches,
    // link loss and development e-stops.
    AXIS_COMMAND_QUICK_STOP,
    // KILL: drive off at once. The arm is no longer held and the position
    // is no longer trusted (homed is cleared). For real emergencies, where
    // the hardware e-stop chain should also be cutting drive power.
    AXIS_COMMAND_KILL,
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
// late_segments counts motion updates the governor failed to deliver in
// time (the axis held still for that millisecond); it should stay zero.
typedef struct {
    uint32_t steps;
    uint32_t late_steps;
    uint32_t max_tick_gap_us;
    uint32_t max_backlog_milli_steps;
    uint32_t late_segments;
} AxisTimingStats;

void axis_service_get_timing(AxisTimingStats *stats);
void axis_service_reset_timing(void);

// Fastest step rate the step tick can produce.
#define AXIS_SERVICE_MAX_STEP_RATE 20000.0f

// Starts the motion interrupts; motion then runs without main-loop help.
// The axis starts with axis_service_default_limits until told otherwise.
void axis_service_init(uint64_t now_us);
AxisResult axis_service_submit(const AxisCommand *command);
void axis_service_get_status(AxisStatus *status);

// The axis's mechanical limits. They are sanitised (see
// axis_motion_sanitise_limits) and take effect when the axis is next at
// rest. Returns false if they had to be adjusted.
bool axis_service_set_limits(AxisMotionLimits *limits);
// Conservative limits for an axis nobody has commissioned yet.
AxisMotionLimits axis_service_default_limits(void);
// Smoothing window of the limits in force, in governor updates (ms): how
// far motion trails requests, and twice the extra distance of a stop.
int axis_service_window_ms(void);
// Soft limits in position-register coordinates; enabled when min < max.
// The governor brakes so the axis never passes them.
void axis_service_set_soft_limits(int32_t min_steps, int32_t max_steps);
// Reports, once, that motion ran into an active limit switch and was
// quick-stopped.
bool axis_service_take_limit_stop(void);

// Called every main-loop pass. If the main loop goes silent for 500 ms while
// the axis moves, the governor quick-stops it: the link watchdog and limit
// reporting run there.
void axis_service_heartbeat(void);
// Reports, once, that the main loop went silent and motion was quick-stopped.
bool axis_service_take_deadman_stop(void);
// Moving, homing, or a scheduled start armed: not a time to write flash,
// which stops the motion interrupts.
bool axis_service_busy(void);

// Arms a command to start, from the motion interrupt, when the 32-bit local
// microsecond tick reaches at_tick_us. A newer schedule replaces it.
void axis_service_schedule(const AxisCommand *command, uint32_t at_tick_us);
void axis_service_cancel_scheduled(void);
bool axis_service_schedule_armed(void);
// Reports, once, that a scheduled command was applied: when, and its result.
bool axis_service_take_scheduled_start(uint32_t *started_tick_us, AxisResult *result);
