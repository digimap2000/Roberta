#pragma once

#include "axis_settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    AXIS_NET_TCP_PORT = 7300,
    AXIS_NET_HOSTNAME_MAX = 40,
    AXIS_NET_DEFAULT_WATCHDOG_MS = 1000,
};

typedef enum {
    AXIS_NET_STATE_UNAVAILABLE,
    AXIS_NET_STATE_NO_CREDENTIALS,
    AXIS_NET_STATE_CONNECTING,
    AXIS_NET_STATE_CONNECTED,
    AXIS_NET_STATE_RETRY_WAIT,
} AxisNetState;

typedef struct {
    AxisNetState state;
    int link_status;
    char hostname[AXIS_NET_HOSTNAME_MAX + 1];
    char ssid[AXIS_SETTINGS_SSID_MAX + 1];
    char ip[16];
    bool client_connected;
    char client_ip[16];
    uint32_t watchdog_ms;
    uint32_t watchdog_trips;
} AxisNetStatus;

void axis_net_init(void);
void axis_net_poll(uint64_t now_us);

void axis_net_get_status(AxisNetStatus *status);
bool axis_net_read_rssi(int32_t *rssi);
const char *axis_net_state_to_string(AxisNetState state);

// Settings edits are staged in RAM until applied.
AxisNetSettings *axis_net_staged_settings(void);
bool axis_net_apply_settings(bool persist);
bool axis_net_forget_settings(void);
void axis_net_set_watchdog_ms(uint32_t timeout_ms);

// Console output sink for the active TCP client.
void axis_net_write(const char *text, size_t length);
