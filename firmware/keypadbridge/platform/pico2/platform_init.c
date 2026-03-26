#include "platform_init.h"

#include "config/board_config.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include <stddef.h>

void platform_init(void)
{
    for (size_t index = 0; index < KEYPAD_BRIDGE_BUTTON_COUNT; ++index) {
        gpio_init(KEYPAD_BRIDGE_BUTTON_PINS[index]);
        gpio_set_dir(KEYPAD_BRIDGE_BUTTON_PINS[index], GPIO_IN);
        gpio_pull_up(KEYPAD_BRIDGE_BUTTON_PINS[index]);
    }

    gpio_init(KEYPAD_BRIDGE_ATTENTION_PIN);
    gpio_set_dir(KEYPAD_BRIDGE_ATTENTION_PIN, GPIO_OUT);
    gpio_put(KEYPAD_BRIDGE_ATTENTION_PIN, KEYPAD_BRIDGE_ATTENTION_RELEASED_LEVEL);

    gpio_init(KEYPAD_BRIDGE_I2C_SDA_PIN);
    gpio_set_function(KEYPAD_BRIDGE_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(KEYPAD_BRIDGE_I2C_SDA_PIN);

    gpio_init(KEYPAD_BRIDGE_I2C_SCL_PIN);
    gpio_set_function(KEYPAD_BRIDGE_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(KEYPAD_BRIDGE_I2C_SCL_PIN);

    i2c_init(KEYPAD_BRIDGE_I2C_INSTANCE, KEYPAD_BRIDGE_I2C_BAUDRATE_HZ);
}
