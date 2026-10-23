#include "keypad_bridge.h"
#include "config/board_config.h"
#include "platform_init.h"
#include "pico/time.h"

#if KEYPADBRIDGE_USB_CONSOLE
#include <stdio.h>

static void print_state(uint8_t state)
{
    printf("state=0x%02x pressed:", state);
    for (uint8_t index = 0; index < KEYPAD_BRIDGE_BUTTON_COUNT; ++index) {
        if ((state & (uint8_t)(1u << index)) != 0u) {
            printf(" K%u/GP%u", index, KEYPAD_BRIDGE_BUTTON_PINS[index]);
        }
    }
    printf("\n");
}

static void print_changed_keys(uint8_t previous_state, uint8_t current_state)
{
    const uint8_t changed = (uint8_t)(previous_state ^ current_state);

    for (uint8_t index = 0; index < KEYPAD_BRIDGE_BUTTON_COUNT; ++index) {
        const uint8_t bit = (uint8_t)(1u << index);
        if ((changed & bit) == 0u) {
            continue;
        }

        printf("key K%u GP%u %s\n",
               index,
               KEYPAD_BRIDGE_BUTTON_PINS[index],
               (current_state & bit) != 0u ? "pressed" : "released");
    }
}
#endif

int main(void)
{
    platform_init();
    keypad_bridge_init();

#if KEYPADBRIDGE_USB_CONSOLE
    uint8_t last_reported_state = keypad_bridge_get_button_state();
    printf("keypadbridge console ready\n");
    print_state(last_reported_state);
#endif

    while (true) {
        keypad_bridge_tick(time_us_64());

#if KEYPADBRIDGE_USB_CONSOLE
        const uint8_t state = keypad_bridge_get_button_state();
        if (state != last_reported_state) {
            print_changed_keys(last_reported_state, state);
            print_state(state);
            last_reported_state = state;
        }
#endif

        keypad_bridge_wait();
    }
}
