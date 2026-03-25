// platform/stm32f4/platform_init.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize MCU-level components.
 *
 * This includes:
 *  - HAL initialization
 *  - Clock setup
 *  - Core peripherals (e.g. SysTick, debug UART)
 *  - Global interrupts
 */
void platform_init(void);

#ifdef __cplusplus
}
#endif