#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void keypad_bridge_init(void);
void keypad_bridge_tick(uint64_t now_us);
uint8_t keypad_bridge_get_button_state(void);

#ifdef __cplusplus
}
#endif
