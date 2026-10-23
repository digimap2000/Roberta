#pragma once

#include <stddef.h>
#include <stdint.h>

typedef void (*AxisConsoleWriteFn)(const char *text, size_t length);

void axis_console_init(void);
void axis_console_poll(void);
void axis_console_report_status_if_due(uint64_t now_us);

// Runs one command line and sends its response through write.
void axis_console_handle_line(char *line, AxisConsoleWriteFn write);
