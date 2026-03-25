#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    AXIS_REGISTER_ACCESS_RO,
    AXIS_REGISTER_ACCESS_RW,
} AxisRegisterAccess;

typedef enum {
    AXIS_REGISTER_RESULT_OK,
    AXIS_REGISTER_RESULT_NOT_FOUND,
    AXIS_REGISTER_RESULT_READ_ONLY,
    AXIS_REGISTER_RESULT_INVALID_VALUE,
    AXIS_REGISTER_RESULT_NOT_SUPPORTED,
} AxisRegisterResult;

typedef struct {
    uint16_t address;
    const char *name;
    AxisRegisterAccess access;
} AxisRegisterInfo;

void axis_registers_init(uint64_t now_us);
void axis_registers_tick(uint64_t now_us);

AxisRegisterResult axis_registers_read(uint16_t address, uint32_t *value);
AxisRegisterResult axis_registers_write(uint16_t address, uint32_t value);
AxisRegisterResult axis_registers_bit_set(uint16_t address, uint32_t mask);
AxisRegisterResult axis_registers_bit_clear(uint16_t address, uint32_t mask);

bool axis_registers_lookup_address(const char *token, uint16_t *address);
size_t axis_registers_get_count(void);
bool axis_registers_get_info(size_t index, AxisRegisterInfo *info);

const char *axis_registers_result_to_string(AxisRegisterResult result);
