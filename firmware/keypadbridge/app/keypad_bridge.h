#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void keypad_bridge_init(void);
void keypad_bridge_tick(uint64_t now_us);
void keypad_bridge_wait(void);
uint8_t keypad_bridge_get_button_state(void);

#ifdef __cplusplus
}
#endif
