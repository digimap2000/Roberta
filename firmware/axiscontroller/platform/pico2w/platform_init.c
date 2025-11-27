#include "platform_init.h"
#include "pico/stdlib.h"

#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0   // DI on MAX485
#define UART_RX_PIN 1   // RO on MAX485


void platform_init(void) {

    // Initialize UART
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}
