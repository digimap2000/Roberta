#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UART_INSTANCE_1 = 0,
    UART_INSTANCE_2,
    UART_INSTANCE_3,
    UART_INSTANCE_COUNT
} UartInstance;

typedef enum {
    UART_PARITY_NONE,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} UartParity;

typedef enum {
    UART_STOPBITS_1,
    UART_STOPBITS_2
} UartStopBits;

typedef enum {
    UART_MODE_BLOCKING,
    UART_MODE_NONBLOCKING
} UartMode;

typedef struct {
    uint32_t baudrate;
    UartParity parity;
    UartStopBits stop_bits;
    UartMode mode;
} UartConfig;
