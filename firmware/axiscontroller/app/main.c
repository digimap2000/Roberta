#include "axis_console.h"
#include "axis_diag.h"
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
    axis_diag_start();

    while (true) {
        uint64_t now_us = time_us_64();
        axis_diag_mark(AXIS_DIAG_PHASE_SERVICE);
        axis_service_tick(now_us);
        axis_diag_mark(AXIS_DIAG_PHASE_REGISTERS);
        axis_registers_tick(now_us);
        axis_diag_mark(AXIS_DIAG_PHASE_CONSOLE);
        axis_console_poll();
        axis_diag_mark(AXIS_DIAG_PHASE_NET);
        axis_net_poll(now_us);
        axis_diag_mark(AXIS_DIAG_PHASE_IDLE);
        axis_diag_feed();
        sleep_us(50);
    }
}
