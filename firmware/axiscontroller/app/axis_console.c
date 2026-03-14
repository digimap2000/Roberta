#include "axis_console.h"

#include "axis_service.h"

#include "pico/stdlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AXIS_CONSOLE_LINE_BUFFER_SIZE = 96,
    AXIS_CONSOLE_STATUS_PERIOD_US = 500000,
};

static char g_line_buffer[AXIS_CONSOLE_LINE_BUFFER_SIZE];
static size_t g_line_length;
static uint64_t g_last_status_us;

static const char *result_to_string(AxisResult result)
{
    switch (result) {
    case AXIS_RESULT_OK:
        return "ok";
    case AXIS_RESULT_BUSY:
        return "busy";
    case AXIS_RESULT_INVALID_ARGUMENT:
        return "invalid-argument";
    case AXIS_RESULT_NOT_HOMED:
        return "not-homed";
    }

    return "error";
}

static bool parse_on_off(const char *token, bool *value)
{
    if (strcmp(token, "on") == 0) {
        *value = true;
        return true;
    }

    if (strcmp(token, "off") == 0) {
        *value = false;
        return true;
    }

    return false;
}

static bool parse_i32(const char *token, int32_t *value)
{
    char *end = NULL;
    long parsed = strtol(token, &end, 10);

    if (token[0] == '\0' || end == NULL || *end != '\0') {
        return false;
    }

    *value = (int32_t)parsed;
    return true;
}

static bool parse_u32(const char *token, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(token, &end, 10);

    if (token[0] == '\0' || end == NULL || *end != '\0') {
        return false;
    }

    *value = (uint32_t)parsed;
    return true;
}

static void print_prompt(void)
{
    printf("axis> ");
}

static void print_help(void)
{
    printf("commands: help status enable on|off outputs on|off zero home goto <steps> move <steps> vel <sps> accel <sps2> stop\r\n");
}

static void print_status(void)
{
    AxisStatus status;

    axis_service_get_status(&status);
    printf("status pos=%ld target=%ld vel=%lu dir=%s outputs=%s motor=%s homed=%s moving=%s homing=%s home=%d a=%d b=%d steps=%lu\r\n",
           (long)status.current_position_steps,
           (long)status.target_position_steps,
           (unsigned long)status.current_velocity_steps_s,
           status.direction == AXIS_DIRECTION_POSITIVE ? "+" : "-",
           status.outputs_enabled ? "on" : "off",
           status.motor_enabled ? "on" : "off",
           status.homed ? "yes" : "no",
           status.moving ? "yes" : "no",
           status.homing ? "yes" : "no",
           status.limits.home ? 1 : 0,
           status.limits.limit_a ? 1 : 0,
           status.limits.limit_b ? 1 : 0,
           (unsigned long)status.emitted_step_count);
}

static void print_result(AxisResult result)
{
    printf("%s\r\n", result_to_string(result));
}

static void handle_line(char *line)
{
    char *save = NULL;
    char *command = strtok_r(line, " \t", &save);

    if (command == NULL) {
        print_prompt();
        return;
    }

    if (strcmp(command, "help") == 0) {
        print_help();
        print_prompt();
        return;
    }

    if (strcmp(command, "status") == 0) {
        print_status();
        print_prompt();
        return;
    }

    if (strcmp(command, "stop") == 0) {
        print_result(axis_service_submit(&(AxisCommand){.type = AXIS_COMMAND_STOP}));
        print_prompt();
        return;
    }

    if (strcmp(command, "zero") == 0) {
        print_result(axis_service_submit(&(AxisCommand){.type = AXIS_COMMAND_SET_ZERO}));
        print_prompt();
        return;
    }

    if (strcmp(command, "home") == 0) {
        print_result(axis_service_submit(&(AxisCommand){.type = AXIS_COMMAND_HOME}));
        print_prompt();
        return;
    }

    if (strcmp(command, "enable") == 0 || strcmp(command, "outputs") == 0) {
        bool flag = false;
        char *arg = strtok_r(NULL, " \t", &save);

        if (arg == NULL || !parse_on_off(arg, &flag)) {
            printf("usage: %s on|off\r\n", command);
            print_prompt();
            return;
        }

        print_result(axis_service_submit(&(AxisCommand){
            .type = strcmp(command, "enable") == 0 ? AXIS_COMMAND_SET_MOTOR_ENABLE
                                                   : AXIS_COMMAND_SET_OUTPUT_ENABLE,
            .flag = flag,
        }));
        print_prompt();
        return;
    }

    if (strcmp(command, "goto") == 0 || strcmp(command, "move") == 0) {
        int32_t steps = 0;
        char *arg = strtok_r(NULL, " \t", &save);

        if (arg == NULL || !parse_i32(arg, &steps)) {
            printf("usage: %s <steps>\r\n", command);
            print_prompt();
            return;
        }

        print_result(axis_service_submit(&(AxisCommand){
            .type = strcmp(command, "goto") == 0 ? AXIS_COMMAND_MOVE_TO : AXIS_COMMAND_MOVE_BY,
            .steps = steps,
        }));
        print_prompt();
        return;
    }

    if (strcmp(command, "vel") == 0 || strcmp(command, "accel") == 0) {
        uint32_t value = 0;
        char *arg = strtok_r(NULL, " \t", &save);

        if (arg == NULL || !parse_u32(arg, &value)) {
            printf("usage: %s <value>\r\n", command);
            print_prompt();
            return;
        }

        print_result(axis_service_submit(&(AxisCommand){
            .type = strcmp(command, "vel") == 0 ? AXIS_COMMAND_SET_MAX_VELOCITY
                                                : AXIS_COMMAND_SET_ACCELERATION,
            .rate = value,
        }));
        print_prompt();
        return;
    }

    printf("unknown command: %s\r\n", command);
    print_prompt();
}

void axis_console_init(void)
{
    g_line_length = 0;
    g_last_status_us = time_us_64();

    printf("axiscontroller ready\r\n");
    print_help();
    print_status();
    print_prompt();
}

void axis_console_poll(void)
{
    while (true) {
        int ch = getchar_timeout_us(0);

        if (ch == PICO_ERROR_TIMEOUT) {
            return;
        }

        if (ch == '\r' || ch == '\n') {
            g_line_buffer[g_line_length] = '\0';
            printf("\r\n");
            handle_line(g_line_buffer);
            g_line_length = 0;
            continue;
        }

        if ((ch == 0x08 || ch == 0x7f) && g_line_length > 0) {
            g_line_length--;
            continue;
        }

        if (g_line_length + 1 < sizeof(g_line_buffer)) {
            g_line_buffer[g_line_length++] = (char)ch;
        }
    }
}

void axis_console_report_status_if_due(uint64_t now_us)
{
    AxisStatus status;

    axis_service_get_status(&status);
    if (!status.moving && !status.homing) {
        return;
    }

    if (now_us - g_last_status_us < AXIS_CONSOLE_STATUS_PERIOD_US) {
        return;
    }

    g_last_status_us = now_us;
    print_status();
    print_prompt();
}
