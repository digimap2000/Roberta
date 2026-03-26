#include "keypad_bridge.h"

#include "config/board_config.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"
#include "keypad_protocol.h"
#include "pico/i2c_slave.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t raw_state_mask;
    uint8_t register_address;
    bool register_selected;
    uint64_t last_raw_change_us[KEYPAD_BRIDGE_BUTTON_COUNT];
} keypad_bridge_private_state_t;

static keypad_bridge_private_state_t g_private_state;
static volatile uint8_t g_button_state_mask;
static volatile uint8_t g_changed_state_mask;
static volatile uint8_t g_event_counter;
static volatile bool g_attention_asserted;

static inline void keypad_bridge_set_attention(bool asserted)
{
    g_attention_asserted = asserted;
    gpio_put(KEYPAD_BRIDGE_ATTENTION_PIN,
             asserted ? KEYPAD_BRIDGE_ATTENTION_ASSERTED_LEVEL
                      : KEYPAD_BRIDGE_ATTENTION_RELEASED_LEVEL);
}

static uint8_t keypad_bridge_sample_raw_state(void)
{
    uint8_t state_mask = 0u;

    for (size_t index = 0; index < KEYPAD_BRIDGE_BUTTON_COUNT; ++index) {
        if (!gpio_get(KEYPAD_BRIDGE_BUTTON_PINS[index])) {
            state_mask |= (uint8_t)(1u << index);
        }
    }

    return state_mask;
}

static void keypad_bridge_acknowledge_changes(void)
{
    g_changed_state_mask = 0u;
    keypad_bridge_set_attention(false);
}

static uint8_t keypad_bridge_read_register(uint8_t reg)
{
    switch (reg) {
    case KEYPAD_REG_PROTOCOL_VERSION:
        return KEYPAD_PROTOCOL_VERSION;
    case KEYPAD_REG_BUTTON_COUNT:
        return KEYPAD_BRIDGE_BUTTON_COUNT;
    case KEYPAD_REG_BUTTON_STATE:
        return g_button_state_mask;
    case KEYPAD_REG_CHANGED_STATE:
        return g_changed_state_mask;
    case KEYPAD_REG_STATUS: {
        uint8_t status = 0u;
        if (g_attention_asserted) {
            status |= KEYPAD_STATUS_ATTENTION_ASSERTED;
        }
        if (g_changed_state_mask != 0u) {
            status |= KEYPAD_STATUS_CHANGES_PENDING;
        }
        return status;
    }
    case KEYPAD_REG_EVENT_COUNTER:
        return g_event_counter;
    default:
        return 0u;
    }
}

static void keypad_bridge_write_register(uint8_t reg, uint8_t value)
{
    switch (reg) {
    case KEYPAD_REG_CONTROL:
        if ((value & KEYPAD_CONTROL_ACKNOWLEDGE_CHANGES) != 0u) {
            keypad_bridge_acknowledge_changes();
        }
        break;
    default:
        break;
    }
}

static void keypad_bridge_i2c_handler(i2c_inst_t *i2c, i2c_slave_event_t event)
{
    switch (event) {
    case I2C_SLAVE_RECEIVE:
        if (!g_private_state.register_selected) {
            g_private_state.register_address = i2c_read_byte_raw(i2c);
            g_private_state.register_selected = true;
        } else {
            keypad_bridge_write_register(g_private_state.register_address, i2c_read_byte_raw(i2c));
            ++g_private_state.register_address;
        }
        break;
    case I2C_SLAVE_REQUEST:
        i2c_write_byte_raw(i2c, keypad_bridge_read_register(g_private_state.register_address));
        ++g_private_state.register_address;
        break;
    case I2C_SLAVE_FINISH:
        g_private_state.register_selected = false;
        break;
    default:
        break;
    }
}

void keypad_bridge_init(void)
{
    const uint8_t initial_state = keypad_bridge_sample_raw_state();

    g_private_state.raw_state_mask = initial_state;
    g_private_state.register_address = 0u;
    g_private_state.register_selected = false;

    for (size_t index = 0; index < KEYPAD_BRIDGE_BUTTON_COUNT; ++index) {
        g_private_state.last_raw_change_us[index] = 0u;
    }

    g_button_state_mask = initial_state;
    g_changed_state_mask = 0u;
    g_event_counter = 0u;
    keypad_bridge_set_attention(false);

    i2c_slave_init(KEYPAD_BRIDGE_I2C_INSTANCE, KEYPAD_BRIDGE_I2C_ADDRESS, keypad_bridge_i2c_handler);
}

void keypad_bridge_tick(uint64_t now_us)
{
    const uint8_t sampled_state = keypad_bridge_sample_raw_state();
    uint8_t stable_state = g_button_state_mask;

    if (sampled_state != g_private_state.raw_state_mask) {
        const uint8_t delta = (uint8_t)(sampled_state ^ g_private_state.raw_state_mask);

        for (size_t index = 0; index < KEYPAD_BRIDGE_BUTTON_COUNT; ++index) {
            if ((delta & (uint8_t)(1u << index)) != 0u) {
                g_private_state.last_raw_change_us[index] = now_us;
            }
        }

        g_private_state.raw_state_mask = sampled_state;
    }

    for (size_t index = 0; index < KEYPAD_BRIDGE_BUTTON_COUNT; ++index) {
        const uint8_t bit = (uint8_t)(1u << index);
        const bool raw_pressed = (g_private_state.raw_state_mask & bit) != 0u;
        const bool stable_pressed = (stable_state & bit) != 0u;

        if (raw_pressed == stable_pressed) {
            continue;
        }

        if ((now_us - g_private_state.last_raw_change_us[index]) < KEYPAD_BRIDGE_DEBOUNCE_US) {
            continue;
        }

        if (raw_pressed) {
            stable_state |= bit;
        } else {
            stable_state &= (uint8_t)~bit;
        }
    }

    if (stable_state != g_button_state_mask) {
        const uint8_t delta = (uint8_t)(stable_state ^ g_button_state_mask);
        const uint32_t irq_state = save_and_disable_interrupts();

        g_button_state_mask = stable_state;
        g_changed_state_mask |= delta;
        ++g_event_counter;
        keypad_bridge_set_attention(true);

        restore_interrupts(irq_state);
    }
}

uint8_t keypad_bridge_get_button_state(void)
{
    const uint32_t irq_state = save_and_disable_interrupts();
    const uint8_t state = g_button_state_mask;
    restore_interrupts(irq_state);
    return state;
}
