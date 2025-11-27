#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uart_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a UART interface.
 * @param instance UART instance identifier (platform-defined)
 * @param config Pointer to configuration structure
 * @return true if initialized successfully, false otherwise
 */
bool uart_init(UartInstance instance, const UartConfig *config);

/**
 * @brief Transmit data (blocking or async based on config).
 * @param instance UART instance
 * @param data Pointer to bytes to send
 * @param length Number of bytes to send
 * @return true if accepted/sent successfully
 */
bool uart_write(UartInstance instance, const uint8_t *data, size_t length);

/**
 * @brief Read data from UART (non-blocking).
 * @param instance UART instance
 * @param data Pointer to buffer to store received bytes
 * @param length Max number of bytes to read
 * @return Number of bytes actually read (0 if no data)
 */
size_t uart_read(UartInstance instance, uint8_t *data, size_t length);

/**
 * @brief Check if UART is ready to transmit.
 */
bool uart_tx_ready(UartInstance instance);

/**
 * @brief Deinitialize UART interface.
 */
void uart_deinit(UartInstance instance);

#ifdef __cplusplus
}
#endif
