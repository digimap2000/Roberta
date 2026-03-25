#include "axis_hw.h"

#include "pico/stdlib.h"

enum {
    GPIO_FAN = 4,
    GPIO_BRAKE = 5,
    GPIO_MOTOR_OE = 6,
    GPIO_MOTOR_PUL = 7,
    GPIO_MOTOR_DIR = 8,
    GPIO_MOTOR_ENA = 9,
    GPIO_LIMIT_B = 10,
    DE_RE_PIN = 2,
    GPIO_LIMIT_A = 14,
    GPIO_LIMIT_HOME = 15,
};

static const bool k_output_enable_active_level = false;
static const bool k_motor_enable_active_level = false;
static const bool k_step_active_level = false;
static const bool k_fan_active_level = true;
static const bool k_brake_active_level = true;

static AxisOutputState g_outputs = {
    .outputs_enabled = false,
    .motor_enabled = false,
    .direction = AXIS_DIRECTION_POSITIVE,
    .step_active = false,
    .fan_active = false,
    .brake_active = false,
};

static void init_output_pin(uint pin, bool initial_level)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, initial_level);
}

static void init_input_pin(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

static bool level_for_enable(bool enabled, bool active_level)
{
    return enabled ? active_level : !active_level;
}

void axis_hw_init(void)
{
    init_output_pin(DE_RE_PIN, 0);
    init_output_pin(GPIO_MOTOR_OE, !k_output_enable_active_level);
    init_output_pin(GPIO_MOTOR_PUL, !k_step_active_level);
    init_output_pin(GPIO_MOTOR_DIR, 0);
    init_output_pin(GPIO_MOTOR_ENA, !k_motor_enable_active_level);
    init_output_pin(GPIO_FAN, !k_fan_active_level);
    init_output_pin(GPIO_BRAKE, !k_brake_active_level);

    init_input_pin(GPIO_LIMIT_HOME);
    init_input_pin(GPIO_LIMIT_A);
    init_input_pin(GPIO_LIMIT_B);
}

void axis_hw_set_output_enable(bool enabled)
{
    g_outputs.outputs_enabled = enabled;
    gpio_put(GPIO_MOTOR_OE, level_for_enable(enabled, k_output_enable_active_level));
}

void axis_hw_set_motor_enable(bool enabled)
{
    g_outputs.motor_enabled = enabled;
    gpio_put(GPIO_MOTOR_ENA, level_for_enable(enabled, k_motor_enable_active_level));
}

void axis_hw_set_direction(AxisDirection direction)
{
    g_outputs.direction = direction;
    gpio_put(GPIO_MOTOR_DIR, direction == AXIS_DIRECTION_POSITIVE);
}

void axis_hw_step_pulse_start(void)
{
    g_outputs.step_active = true;
    gpio_put(GPIO_MOTOR_PUL, k_step_active_level);
}

void axis_hw_step_pulse_end(void)
{
    g_outputs.step_active = false;
    gpio_put(GPIO_MOTOR_PUL, !k_step_active_level);
}

void axis_hw_set_fan(bool enabled)
{
    g_outputs.fan_active = enabled;
    gpio_put(GPIO_FAN, level_for_enable(enabled, k_fan_active_level));
}

void axis_hw_set_brake(bool enabled)
{
    g_outputs.brake_active = enabled;
    gpio_put(GPIO_BRAKE, level_for_enable(enabled, k_brake_active_level));
}

AxisLimitState axis_hw_read_limits(void)
{
    AxisLimitState limits = {
        .home = gpio_get(GPIO_LIMIT_HOME) == 0,
        .limit_a = gpio_get(GPIO_LIMIT_A) == 0,
        .limit_b = gpio_get(GPIO_LIMIT_B) == 0,
    };
    return limits;
}

AxisOutputState axis_hw_get_output_state(void)
{
    return g_outputs;
}
