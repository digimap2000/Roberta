#include "platform_init.h"

#include "config/board_config.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include <stddef.h>

void platform_init(void)
{
#if KEYPADBRIDGE_USB_CONSOLE
    set_sys_clock_khz(KEYPAD_BRIDGE_USB_CONSOLE_SYS_CLOCK_KHZ, true);
    stdio_init_all();
    sleep_ms(KEYPAD_BRIDGE_USB_SETTLE_MS);
#else
    set_sys_clock_khz(KEYPAD_BRIDGE_SYS_CLOCK_KHZ, true);
#endif

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
