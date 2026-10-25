#include "axis_config.h"

#include "hardware/flash.h"
#include "pico/flash.h"

#include <string.h>

// The sector below the network settings, which take the last one.
enum {
    AXIS_CONFIG_MAGIC = 0x52424d31u, // "RBM1"
    AXIS_CONFIG_VERSION = 1u,
    AXIS_CONFIG_FLASH_OFFSET = PICO_FLASH_SIZE_BYTES - 2u * FLASH_SECTOR_SIZE,
    AXIS_CONFIG_FLASH_TIMEOUT_MS = 500,
};

typedef struct {
    uint32_t magic;
    uint32_t version;
    AxisConfig config;
    uint32_t checksum;
} AxisConfigRecord;

_Static_assert(sizeof(AxisConfigRecord) <= FLASH_PAGE_SIZE, "config record too large");

static uint8_t g_program_buffer[FLASH_PAGE_SIZE];

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

static void erase_and_program_sector(void *param)
{
    (void)param;
    flash_range_erase(AXIS_CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(AXIS_CONFIG_FLASH_OFFSET, g_program_buffer, sizeof(g_program_buffer));
}

bool axis_config_load(AxisConfig *config)
{
    const AxisConfigRecord *record = (const AxisConfigRecord *)(XIP_BASE + AXIS_CONFIG_FLASH_OFFSET);

    if (record->magic != AXIS_CONFIG_MAGIC || record->version != AXIS_CONFIG_VERSION ||
        record->checksum != fnv1a(&record->config, sizeof(record->config))) {
        return false;
    }

    memcpy(config, &record->config, sizeof(*config));
    return true;
}

bool axis_config_save(const AxisConfig *config)
{
    AxisConfigRecord record;

    memset(&record, 0, sizeof(record));
    record.magic = AXIS_CONFIG_MAGIC;
    record.version = AXIS_CONFIG_VERSION;
    record.config = *config;
    record.checksum = fnv1a(&record.config, sizeof(record.config));

    memset(g_program_buffer, 0xff, sizeof(g_program_buffer));
    memcpy(g_program_buffer, &record, sizeof(record));

    return flash_safe_execute(erase_and_program_sector, NULL, AXIS_CONFIG_FLASH_TIMEOUT_MS) == PICO_OK;
}
