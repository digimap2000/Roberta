#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTOR_MICROSTEP_1 = 1,
    MOTOR_MICROSTEP_2 = 2,
    MOTOR_MICROSTEP_4 = 4,
    MOTOR_MICROSTEP_8 = 8,
    MOTOR_MICROSTEP_16 = 16,
    MOTOR_MICROSTEP_32 = 32
} MotorMicrostep;

bool motor_api_init(void);
bool motor_set_microstep(MotorMicrostep ms);
bool motor_get_microstep(MotorMicrostep *ms);
