#pragma once

#include <stdint.h>

enum {
    KEYPAD_PROTOCOL_VERSION = 1u,
};

enum keypad_register_address {
    KEYPAD_REG_PROTOCOL_VERSION = 0x00,
    KEYPAD_REG_BUTTON_COUNT = 0x01,
    KEYPAD_REG_BUTTON_STATE = 0x02,
    KEYPAD_REG_CHANGED_STATE = 0x03,
    KEYPAD_REG_STATUS = 0x04,
    KEYPAD_REG_EVENT_COUNTER = 0x05,
    KEYPAD_REG_CONTROL = 0x10,
};

enum keypad_status_flag {
    KEYPAD_STATUS_ATTENTION_ASSERTED = 1u << 0,
    KEYPAD_STATUS_CHANGES_PENDING = 1u << 1,
};

enum keypad_control_flag {
    KEYPAD_CONTROL_ACKNOWLEDGE_CHANGES = 1u << 0,
};
