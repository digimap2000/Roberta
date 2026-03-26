#include "keypad_bridge.h"
#include "platform_init.h"
#include "pico/time.h"

int main(void)
{
    platform_init();
    keypad_bridge_init();

    while (true) {
        keypad_bridge_tick(time_us_64());
        keypad_bridge_wait();
    }
}
