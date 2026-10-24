#include "axis_console.h"

#include "axis_diag.h"
#include "axis_net.h"
#include "axis_registers.h"

#include "pico/stdlib.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AXIS_CONSOLE_LINE_BUFFER_SIZE = 256,
    AXIS_CONSOLE_OUTPUT_BUFFER_SIZE = 256,
    AXIS_CONSOLE_STATUS_PERIOD_US = 500000,
    REG_CONTROL_WORD = 0x0010,
    REG_STATUS_WORD = 0x0011,
    REG_FAULT_WORD = 0x0012,
    REG_INPUT_WORD = 0x0014,
    REG_STAGED_TARGET_POSITION_STEPS = 0x0020,
    REG_ACTIVE_TARGET_POSITION_STEPS = 0x0021,
    REG_CURRENT_POSITION_STEPS = 0x0022,
    REG_STAGED_TARGET_VELOCITY_STEPS_S = 0x0023,
    REG_CURRENT_VELOCITY_STEPS_S = 0x0025,
    REG_STAGED_TARGET_ACCEL_STEPS_S2 = 0x0026,
    REG_STAGED_RELATIVE_MOVE_STEPS = 0x002B,
    REG_STEP_COUNT = 0x0050,
    CONTROL_OUTPUT_ENABLE = 1u << 0,
    CONTROL_MOTOR_ENABLE = 1u << 1,
    CONTROL_MOVE_ABSOLUTE = 1u << 2,
    CONTROL_MOVE_RELATIVE = 1u << 3,
    CONTROL_HOME = 1u << 4,
    CONTROL_STOP = 1u << 5,
    CONTROL_SET_ZERO = 1u << 8,
    STATUS_OUTPUTS_ENABLED = 1u << 0,
    STATUS_MOTOR_ENABLED = 1u << 1,
    STATUS_MOVING = 1u << 2,
    STATUS_HOMING = 1u << 3,
    STATUS_HOMED = 1u << 4,
    STATUS_HOME_ACTIVE = 1u << 7,
    STATUS_LIMIT_A_ACTIVE = 1u << 8,
    STATUS_LIMIT_B_ACTIVE = 1u << 9,
};

static char g_line_buffer[AXIS_CONSOLE_LINE_BUFFER_SIZE];
static size_t g_line_length;
static bool g_line_overflowed;
static uint64_t g_last_status_us;
static AxisConsoleWriteFn g_write;
// Echo typed characters on USB so terminal programs show what is typed. Host
// tools turn it off with "echo off".
static bool g_echo = true;

static void usb_console_write(const char *text, size_t length)
{
    fwrite(text, 1, length, stdout);
    fflush(stdout);
}

// Responses go to whichever transport delivered the current line.
static void console_printf(const char *format, ...)
{
    char buffer[AXIS_CONSOLE_OUTPUT_BUFFER_SIZE];
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0) {
        return;
    }
    if ((size_t)length >= sizeof(buffer)) {
        length = (int)sizeof(buffer) - 1;
    }

    (g_write != NULL ? g_write : usb_console_write)(buffer, (size_t)length);
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
    long parsed = strtol(token, &end, 0);

    if (token[0] == '\0' || end == NULL || *end != '\0') {
        return false;
    }

    *value = (int32_t)parsed;
    return true;
}

static bool parse_u32(const char *token, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(token, &end, 0);

    if (token[0] == '\0' || end == NULL || *end != '\0') {
        return false;
    }

    *value = (uint32_t)parsed;
    return true;
}

static bool parse_register_value(const char *token, uint32_t *value)
{
    char *end = NULL;
    long long parsed = strtoll(token, &end, 0);

    if (token[0] == '\0' || end == NULL || *end != '\0') {
        return false;
    }

    *value = (uint32_t)parsed;
    return true;
}

static bool parse_register_address(const char *token, uint16_t *address)
{
    return axis_registers_lookup_address(token, address);
}

static bool find_register_info(uint16_t address, AxisRegisterInfo *info)
{
    size_t index;

    for (index = 0; index < axis_registers_get_count(); index++) {
        if (axis_registers_get_info(index, info) && info->address == address) {
            return true;
        }
    }

    return false;
}

static void print_prompt(void)
{
    console_printf("axis> ");
}

static void print_register_result(AxisRegisterResult result)
{
    console_printf("%s\r\n", axis_registers_result_to_string(result));
}

static void print_register_value(uint16_t address)
{
    AxisRegisterInfo info;
    uint32_t value = 0;
    AxisRegisterResult result = axis_registers_read(address, &value);

    if (result != AXIS_REGISTER_RESULT_OK) {
        print_register_result(result);
        return;
    }

    if (!find_register_info(address, &info)) {
        info.address = address;
        info.name = "UNKNOWN";
    }

    console_printf("%s (0x%04x) = 0x%08lx u=%lu i=%ld\r\n",
           info.name,
           info.address,
           (unsigned long)value,
           (unsigned long)value,
           (long)(int32_t)value);
}

static void print_help(void)
{
    console_printf("commands: help status reg read <addr|name> reg readm <addr|name>... reg write <addr|name> <value> reg bitset <addr|name> <mask> reg bitclear <addr|name> <mask> reg dump\r\n");
    console_printf("aliases: echo on|off enable on|off outputs on|off zero home goto <steps> move <steps> vel <sps> accel <sps2> stop\r\n");
    console_printf("diagnostics: diag (why the last reset happened) diag hang diag fault diag panic (test the watchdog)\r\n");
    console_printf("network: ping net [status] net scan net ssid <ssid> net pass <password> net name <name> net save net forget net watchdog <ms>\r\n");
}

static void print_net_status(void)
{
    AxisNetStatus status;
    int32_t rssi = 0;
    char rssi_text[12] = "-";

    if (axis_net_read_rssi(&rssi)) {
        snprintf(rssi_text, sizeof(rssi_text), "%ld", (long)rssi);
    }

    axis_net_get_status(&status);
    console_printf("net state=%s link=%d host=%s.local port=%d ssid=%s ip=%s client=%s rssi=%s watchdog_ms=%lu trips=%lu\r\n",
                   axis_net_state_to_string(status.state),
                   status.link_status,
                   status.hostname,
                   AXIS_NET_TCP_PORT,
                   status.ssid[0] != '\0' ? status.ssid : "-",
                   status.ip[0] != '\0' ? status.ip : "-",
                   status.client_ip[0] != '\0' ? status.client_ip : "-",
                   rssi_text,
                   (unsigned long)status.watchdog_ms,
                   (unsigned long)status.watchdog_trips);
}

// Copies the rest of the line, so SSIDs and passwords may contain spaces.
static bool copy_remainder(char *save, char *destination, size_t capacity)
{
    size_t length;

    if (save == NULL) {
        return false;
    }
    while (*save == ' ' || *save == '\t') {
        save++;
    }

    length = strlen(save);
    if (length == 0 || length >= capacity) {
        return false;
    }

    memcpy(destination, save, length + 1);
    return true;
}

static void handle_net_command(char *save)
{
    AxisNetSettings *staged = axis_net_staged_settings();
    char *subcommand = strtok_r(NULL, " \t", &save);

    if (subcommand == NULL || strcmp(subcommand, "status") == 0) {
        print_net_status();
    } else if (strcmp(subcommand, "ssid") == 0) {
        console_printf(copy_remainder(save, staged->ssid, sizeof(staged->ssid))
                           ? "staged ssid, run net save to apply\r\n"
                           : "usage: net ssid <ssid>\r\n");
    } else if (strcmp(subcommand, "pass") == 0) {
        console_printf(copy_remainder(save, staged->password, sizeof(staged->password))
                           ? "staged password, run net save to apply\r\n"
                           : "usage: net pass <password>\r\n");
    } else if (strcmp(subcommand, "name") == 0) {
        char *name = strtok_r(NULL, " \t", &save);
        if (name != NULL && axis_settings_name_is_valid(name)) {
            snprintf(staged->name, sizeof(staged->name), "%s", name);
            console_printf("staged name, run net save to apply\r\n");
        } else {
            console_printf("usage: net name <letters, digits, hyphens>\r\n");
        }
    } else if (strcmp(subcommand, "save") == 0) {
        console_printf(axis_net_apply_settings(true) ? "saved, reconnecting\r\n" : "save failed\r\n");
    } else if (strcmp(subcommand, "forget") == 0) {
        console_printf(axis_net_forget_settings() ? "forgotten\r\n" : "erase failed\r\n");
    } else if (strcmp(subcommand, "scan") == 0) {
        console_printf(axis_net_start_scan()
                           ? "scanning, results follow on the USB console\r\n"
                           : "scan unavailable, try again in a few seconds\r\n");
    } else if (strcmp(subcommand, "watchdog") == 0) {
        uint32_t timeout_ms = 0;
        char *arg = strtok_r(NULL, " \t", &save);
        if (arg != NULL && parse_u32(arg, &timeout_ms)) {
            axis_net_set_watchdog_ms(timeout_ms);
            console_printf("OK\r\n");
        } else {
            console_printf("usage: net watchdog <ms, 0 disables>\r\n");
        }
    } else {
        console_printf("unknown net command: %s\r\n", subcommand);
    }

    print_prompt();
}

static void print_status(void)
{
    uint32_t status_word = 0;
    uint32_t fault_word = 0;
    uint32_t position = 0;
    uint32_t target = 0;
    uint32_t velocity = 0;
    uint32_t inputs = 0;
    uint32_t step_count = 0;

    axis_registers_read(REG_STATUS_WORD, &status_word);
    axis_registers_read(REG_FAULT_WORD, &fault_word);
    axis_registers_read(REG_CURRENT_POSITION_STEPS, &position);
    axis_registers_read(REG_ACTIVE_TARGET_POSITION_STEPS, &target);
    axis_registers_read(REG_CURRENT_VELOCITY_STEPS_S, &velocity);
    axis_registers_read(REG_INPUT_WORD, &inputs);
    axis_registers_read(REG_STEP_COUNT, &step_count);

    console_printf("status pos=%ld target=%ld vel=%lu outputs=%s motor=%s homed=%s moving=%s homing=%s home=%d a=%d b=%d faults=0x%08lx steps=%lu\r\n",
           (long)(int32_t)position,
           (long)(int32_t)target,
           (unsigned long)velocity,
           (status_word & STATUS_OUTPUTS_ENABLED) != 0 ? "on" : "off",
           (status_word & STATUS_MOTOR_ENABLED) != 0 ? "on" : "off",
           (status_word & STATUS_HOMED) != 0 ? "yes" : "no",
           (status_word & STATUS_MOVING) != 0 ? "yes" : "no",
           (status_word & STATUS_HOMING) != 0 ? "yes" : "no",
           (inputs & (1u << 0)) != 0 ? 1 : 0,
           (inputs & (1u << 1)) != 0 ? 1 : 0,
           (inputs & (1u << 2)) != 0 ? 1 : 0,
           (unsigned long)fault_word,
           (unsigned long)step_count);
}

static void print_dump(void)
{
    AxisRegisterInfo info;
    size_t index;

    for (index = 0; index < axis_registers_get_count(); index++) {
        uint32_t value = 0;

        if (!axis_registers_get_info(index, &info)) {
            continue;
        }
        if (axis_registers_read(info.address, &value) != AXIS_REGISTER_RESULT_OK) {
            continue;
        }

        console_printf("0x%04x %-32s 0x%08lx u=%lu i=%ld %s\r\n",
               info.address,
               info.name,
               (unsigned long)value,
               (unsigned long)value,
               (long)(int32_t)value,
               info.access == AXIS_REGISTER_ACCESS_RW ? "RW" : "RO");
    }
}

static AxisRegisterResult write_alias_move(uint16_t staged_address, int32_t steps, uint32_t action_mask)
{
    AxisRegisterResult result;

    result = axis_registers_write(staged_address, (uint32_t)steps);
    if (result != AXIS_REGISTER_RESULT_OK) {
        return result;
    }

    return axis_registers_bit_set(REG_CONTROL_WORD, action_mask);
}

static void handle_reg_command(char *save)
{
    char *subcommand = strtok_r(NULL, " \t", &save);

    if (subcommand == NULL) {
        console_printf("usage: reg read|write|bitset|bitclear|dump\r\n");
        print_prompt();
        return;
    }

    if (strcmp(subcommand, "dump") == 0) {
        print_dump();
        print_prompt();
        return;
    }

    if (strcmp(subcommand, "read") == 0) {
        uint16_t address = 0;
        char *target = strtok_r(NULL, " \t", &save);

        if (target == NULL || !parse_register_address(target, &address)) {
            console_printf("usage: reg read <addr|name>\r\n");
            print_prompt();
            return;
        }

        print_register_value(address);
        print_prompt();
        return;
    }

    // Batched read: one response line per register, in request order.
    if (strcmp(subcommand, "readm") == 0) {
        char *target = strtok_r(NULL, " \t", &save);

        if (target == NULL) {
            console_printf("usage: reg readm <addr|name>...\r\n");
        }
        for (; target != NULL; target = strtok_r(NULL, " \t", &save)) {
            uint16_t address = 0;
            if (parse_register_address(target, &address)) {
                print_register_value(address);
            } else {
                console_printf("not-found %s\r\n", target);
            }
        }
        print_prompt();
        return;
    }

    if (strcmp(subcommand, "write") == 0 ||
        strcmp(subcommand, "bitset") == 0 ||
        strcmp(subcommand, "bitclear") == 0) {
        uint16_t address = 0;
        uint32_t value = 0;
        char *target = strtok_r(NULL, " \t", &save);
        char *arg = strtok_r(NULL, " \t", &save);
        AxisRegisterResult result;

        if (target == NULL || arg == NULL ||
            !parse_register_address(target, &address) ||
            !parse_register_value(arg, &value)) {
            console_printf("usage: reg %s <addr|name> <value>\r\n", subcommand);
            print_prompt();
            return;
        }

        if (strcmp(subcommand, "write") == 0) {
            result = axis_registers_write(address, value);
        } else if (strcmp(subcommand, "bitset") == 0) {
            result = axis_registers_bit_set(address, value);
        } else {
            result = axis_registers_bit_clear(address, value);
        }

        print_register_result(result);
        if (result == AXIS_REGISTER_RESULT_OK) {
            print_register_value(address);
        }
        print_prompt();
        return;
    }

    console_printf("unknown reg command: %s\r\n", subcommand);
    print_prompt();
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

    if (strcmp(command, "reg") == 0) {
        handle_reg_command(save);
        return;
    }

    if (strcmp(command, "net") == 0) {
        handle_net_command(save);
        return;
    }

    if (strcmp(command, "echo") == 0) {
        bool flag = false;
        char *arg = strtok_r(NULL, " \t", &save);

        if (arg == NULL || !parse_on_off(arg, &flag)) {
            console_printf("usage: echo on|off\r\n");
        } else {
            g_echo = flag;
            console_printf("OK\r\n");
        }
        print_prompt();
        return;
    }

    if (strcmp(command, "diag") == 0) {
        char line[192];
        char *arg = strtok_r(NULL, " \t", &save);

        // Deliberate failures for proving the watchdog and fault capture.
        if (arg != NULL && strcmp(arg, "hang") == 0) {
            console_printf("hanging; watchdog reset in 2 s\r\n");
            while (true) {
            }
        }
        if (arg != NULL && strcmp(arg, "panic") == 0) {
            console_printf("panicking\r\n");
            sleep_ms(50);
            panic("diag panic test");
        }
        if (arg != NULL && strcmp(arg, "fault") == 0) {
            console_printf("faulting\r\n");
            sleep_ms(50);
            ((void (*)(void))0xfffffff1u)();
        }

        axis_diag_describe(line, sizeof(line));
        console_printf("diag %s uptime_ms=%lu\r\n", line, (unsigned long)(time_us_64() / 1000u));
        print_prompt();
        return;
    }

    if (strcmp(command, "ping") == 0) {
        console_printf("pong\r\n");
        print_prompt();
        return;
    }

    if (strcmp(command, "stop") == 0) {
        print_register_result(axis_registers_bit_set(REG_CONTROL_WORD, CONTROL_STOP));
        print_prompt();
        return;
    }

    if (strcmp(command, "zero") == 0) {
        print_register_result(axis_registers_bit_set(REG_CONTROL_WORD, CONTROL_SET_ZERO));
        print_prompt();
        return;
    }

    if (strcmp(command, "home") == 0) {
        print_register_result(axis_registers_bit_set(REG_CONTROL_WORD, CONTROL_HOME));
        print_prompt();
        return;
    }

    if (strcmp(command, "enable") == 0 || strcmp(command, "outputs") == 0) {
        bool flag = false;
        char *arg = strtok_r(NULL, " \t", &save);
        uint32_t mask = strcmp(command, "enable") == 0 ? CONTROL_MOTOR_ENABLE : CONTROL_OUTPUT_ENABLE;
        AxisRegisterResult result;

        if (arg == NULL || !parse_on_off(arg, &flag)) {
            console_printf("usage: %s on|off\r\n", command);
            print_prompt();
            return;
        }

        result = flag ? axis_registers_bit_set(REG_CONTROL_WORD, mask)
                      : axis_registers_bit_clear(REG_CONTROL_WORD, mask);
        print_register_result(result);
        print_prompt();
        return;
    }

    if (strcmp(command, "goto") == 0 || strcmp(command, "move") == 0) {
        int32_t steps = 0;
        char *arg = strtok_r(NULL, " \t", &save);
        AxisRegisterResult result;

        if (arg == NULL || !parse_i32(arg, &steps)) {
            console_printf("usage: %s <steps>\r\n", command);
            print_prompt();
            return;
        }

        result = strcmp(command, "goto") == 0
            ? write_alias_move(REG_STAGED_TARGET_POSITION_STEPS, steps, CONTROL_MOVE_ABSOLUTE)
            : write_alias_move(REG_STAGED_RELATIVE_MOVE_STEPS, steps, CONTROL_MOVE_RELATIVE);
        print_register_result(result);
        print_prompt();
        return;
    }

    if (strcmp(command, "vel") == 0 || strcmp(command, "accel") == 0) {
        uint32_t value = 0;
        uint16_t address = strcmp(command, "vel") == 0
            ? REG_STAGED_TARGET_VELOCITY_STEPS_S
            : REG_STAGED_TARGET_ACCEL_STEPS_S2;
        char *arg = strtok_r(NULL, " \t", &save);
        AxisRegisterResult result;

        if (arg == NULL || !parse_u32(arg, &value)) {
            console_printf("usage: %s <value>\r\n", command);
            print_prompt();
            return;
        }

        result = axis_registers_write(address, value);
        print_register_result(result);
        print_prompt();
        return;
    }

    console_printf("unknown command: %s\r\n", command);
    print_prompt();
}

void axis_console_handle_line(char *line, AxisConsoleWriteFn write)
{
    g_write = write;
    handle_line(line);
    g_write = NULL;
}

void axis_console_init(void)
{
    g_line_length = 0;
    g_line_overflowed = false;
    g_last_status_us = time_us_64();

    console_printf("axiscontroller ready\r\n");
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
            console_printf("\r\n");
            if (g_line_overflowed) {
                console_printf("line-too-long\r\n");
                print_prompt();
            } else {
                g_line_buffer[g_line_length] = '\0';
                axis_console_handle_line(g_line_buffer, usb_console_write);
            }
            g_line_length = 0;
            g_line_overflowed = false;
            continue;
        }

        if (ch == 0x08 || ch == 0x7f) {
            if (g_line_length > 0) {
                g_line_length--;
                if (g_echo) {
                    usb_console_write("\b \b", 3);
                }
            }
            continue;
        }

        if (g_line_length + 1 < sizeof(g_line_buffer)) {
            g_line_buffer[g_line_length++] = (char)ch;
            if (g_echo && ch >= 0x20 && ch < 0x7f) {
                const char echoed = (char)ch;
                usb_console_write(&echoed, 1);
            }
        } else {
            g_line_overflowed = true;
        }
    }
}

void axis_console_report_status_if_due(uint64_t now_us)
{
    uint32_t status_word = 0;

    axis_registers_read(REG_STATUS_WORD, &status_word);
    if ((status_word & (STATUS_MOVING | STATUS_HOMING)) == 0) {
        return;
    }

    if (now_us - g_last_status_us < AXIS_CONSOLE_STATUS_PERIOD_US) {
        return;
    }

    g_last_status_us = now_us;
    print_status();
    print_prompt();
}
