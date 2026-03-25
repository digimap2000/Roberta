#pragma once

#include <stdint.h>

void axis_console_init(void);
void axis_console_poll(void);
void axis_console_report_status_if_due(uint64_t now_us);
