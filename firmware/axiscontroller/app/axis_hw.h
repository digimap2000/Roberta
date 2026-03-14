#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AXIS_DIRECTION_NEGATIVE = -1,
    AXIS_DIRECTION_POSITIVE = 1,
} AxisDirection;

typedef struct {
    bool home;
    bool limit_a;
    bool limit_b;
} AxisLimitState;

void axis_hw_init(void);
void axis_hw_set_output_enable(bool enabled);
void axis_hw_set_motor_enable(bool enabled);
void axis_hw_set_direction(AxisDirection direction);
void axis_hw_step_pulse_start(void);
void axis_hw_step_pulse_end(void);
void axis_hw_set_fan(bool enabled);
void axis_hw_set_brake(bool enabled);
AxisLimitState axis_hw_read_limits(void);
