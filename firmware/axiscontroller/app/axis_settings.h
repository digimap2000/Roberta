#pragma once

#include <stdbool.h>
#include <stddef.h>

enum {
    AXIS_SETTINGS_SSID_MAX = 32,
    AXIS_SETTINGS_PASSWORD_MAX = 64,
    AXIS_SETTINGS_NAME_MAX = 24,
};

typedef struct {
    char ssid[AXIS_SETTINGS_SSID_MAX + 1];
    char password[AXIS_SETTINGS_PASSWORD_MAX + 1];
    char name[AXIS_SETTINGS_NAME_MAX + 1];
} AxisNetSettings;

// Loads the persisted network settings, or zeroed settings when flash holds
// nothing valid.
void axis_settings_load(AxisNetSettings *settings);
bool axis_settings_save(const AxisNetSettings *settings);
bool axis_settings_erase(void);

bool axis_settings_has_credentials(const AxisNetSettings *settings);
bool axis_settings_name_is_valid(const char *name);
