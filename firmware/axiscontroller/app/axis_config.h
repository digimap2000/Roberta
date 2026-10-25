#pragma once

#include "axis_motion.h"

#include <stdbool.h>
#include <stdint.h>

// Per-axis commissioning that survives a reboot: the governor's mechanical
// limits, soft limits and home offset. Kept in its own flash sector, apart
// from the network settings.
typedef struct {
    AxisMotionLimits limits;
    int32_t soft_limit_min_steps;
    int32_t soft_limit_max_steps;
    int32_t home_offset_steps;
} AxisConfig;

// Loads the saved config. Returns false, leaving `config` untouched, when
// flash holds none (the axis is uncommissioned).
bool axis_config_load(AxisConfig *config);
// Writes flash with interrupts off for tens of milliseconds: only call with
// the axis at rest.
bool axis_config_save(const AxisConfig *config);
