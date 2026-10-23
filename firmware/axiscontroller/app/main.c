#include "axis_console.h"
#include "axis_hw.h"
#include "axis_net.h"
#include "axis_registers.h"
#include "axis_service.h"
#include "config/platform_select.h"
#include "pico/stdlib.h"

int main(void)
{
    stdio_init_all();
    sleep_ms(1500);

    platform_init();
    axis_hw_init();
    axis_service_init(time_us_64());
    axis_registers_init(time_us_64());
    axis_console_init();
    axis_net_init();

    while (true) {
        uint64_t now_us = time_us_64();
        axis_service_tick(now_us);
        axis_registers_tick(now_us);
        axis_console_poll();
        axis_net_poll(now_us);
        sleep_us(50);
    }
}
