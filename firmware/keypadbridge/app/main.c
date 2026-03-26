#include "keypad_bridge.h"
#include "config/board_config.h"
#include "platform_init.h"
#include "pico/stdlib.h"

#include <stdio.h>

int main(void)
{
    uint8_t last_state;

    stdio_init_all();
    sleep_ms(1500);

    platform_init();
    keypad_bridge_init();
    last_state = keypad_bridge_get_button_state();

    printf("keypadbridge ready\r\n");
    printf("buttons mask = 0x%02x\r\n", last_state);

    while (true) {
        keypad_bridge_tick(time_us_64());

        const uint8_t state = keypad_bridge_get_button_state();
        if (state != last_state) {
            printf("buttons mask = 0x%02x\r\n", state);
            last_state = state;
        }

        sleep_ms(KEYPAD_BRIDGE_POLL_INTERVAL_MS);
    }
}
