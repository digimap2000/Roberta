#include "axis_settings.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

// Settings live in the last flash sector, well clear of the firmware image.
enum {
    AXIS_SETTINGS_MAGIC = 0x52425431u, // "RBT1"
    AXIS_SETTINGS_VERSION = 1u,
    AXIS_SETTINGS_FLASH_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE,
    AXIS_SETTINGS_FLASH_TIMEOUT_MS = 500,
};

typedef struct {
    uint32_t magic;
    uint32_t version;
    AxisNetSettings settings;
    uint32_t checksum;
} AxisSettingsRecord;

_Static_assert(sizeof(AxisSettingsRecord) <= FLASH_PAGE_SIZE * 2u, "settings record too large");

enum {
    AXIS_SETTINGS_PROGRAM_SIZE =
        ((sizeof(AxisSettingsRecord) + FLASH_PAGE_SIZE - 1u) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE,
};

static uint8_t g_program_buffer[AXIS_SETTINGS_PROGRAM_SIZE];

static uint32_t fnv1a(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t hash = 2166136261u;

    for (size_t index = 0; index < length; index++) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }

    return hash;
}

static void erase_sector(void *param)
{
    (void)param;
    flash_range_erase(AXIS_SETTINGS_FLASH_OFFSET, FLASH_SECTOR_SIZE);
}

static void erase_and_program_sector(void *param)
{
    (void)param;
    flash_range_erase(AXIS_SETTINGS_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(AXIS_SETTINGS_FLASH_OFFSET, g_program_buffer, sizeof(g_program_buffer));
}

static void terminate_strings(AxisNetSettings *settings)
{
    settings->ssid[AXIS_SETTINGS_SSID_MAX] = '\0';
    settings->password[AXIS_SETTINGS_PASSWORD_MAX] = '\0';
    settings->name[AXIS_SETTINGS_NAME_MAX] = '\0';
}

void axis_settings_load(AxisNetSettings *settings)
{
    const AxisSettingsRecord *record =
        (const AxisSettingsRecord *)(XIP_BASE + AXIS_SETTINGS_FLASH_OFFSET);

    memset(settings, 0, sizeof(*settings));

    if (record->magic != AXIS_SETTINGS_MAGIC || record->version != AXIS_SETTINGS_VERSION) {
        return;
    }
    if (record->checksum != fnv1a(&record->settings, sizeof(record->settings))) {
        return;
    }

    memcpy(settings, &record->settings, sizeof(*settings));
    terminate_strings(settings);
}

bool axis_settings_save(const AxisNetSettings *settings)
{
    AxisSettingsRecord record;

    memset(&record, 0, sizeof(record));
    record.magic = AXIS_SETTINGS_MAGIC;
    record.version = AXIS_SETTINGS_VERSION;
    memcpy(&record.settings, settings, sizeof(record.settings));
    terminate_strings(&record.settings);
    record.checksum = fnv1a(&record.settings, sizeof(record.settings));

    memset(g_program_buffer, 0xff, sizeof(g_program_buffer));
    memcpy(g_program_buffer, &record, sizeof(record));

    return flash_safe_execute(erase_and_program_sector, NULL, AXIS_SETTINGS_FLASH_TIMEOUT_MS) == PICO_OK;
}

bool axis_settings_erase(void)
{
    return flash_safe_execute(erase_sector, NULL, AXIS_SETTINGS_FLASH_TIMEOUT_MS) == PICO_OK;
}

bool axis_settings_has_credentials(const AxisNetSettings *settings)
{
    return settings->ssid[0] != '\0';
}

bool axis_settings_name_is_valid(const char *name)
{
    size_t length = strlen(name);

    if (length == 0 || length > AXIS_SETTINGS_NAME_MAX) {
        return false;
    }

    // Names become part of the mDNS hostname, so keep to DNS label characters.
    for (size_t index = 0; index < length; index++) {
        const unsigned char ch = (unsigned char)name[index];
        if (!isalnum(ch) && ch != '-') {
            return false;
        }
    }

    return name[0] != '-' && name[length - 1] != '-';
}
