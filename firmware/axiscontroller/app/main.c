#include <stdio.h>

//#include "config/platform_select.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/version.h"

#define GPIO_MOTOR_OE 6
#define GPIO_MOTOR_PUL 7
#define GPIO_MOTOR_DIR 8
#define GPIO_MOTOR_ENA 9

#define GPIO_FAN 4
#define GPIO_BRAKE 5

#define GPIO_LIMIT_HOME 15
#define GPIO_LIMIT_A 14
#define GPIO_LIMIT_B 10

// UART configuration
#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0   // DI on MAX485
#define UART_RX_PIN 1   // RO on MAX485

#define DE_RE_PIN 2    // DE and RE tied together




/*
void max485_send(uint8_t *data, size_t len) {
    // Enable transmit
    gpio_put(DE_RE_PIN, 1);

    // Send bytes
    for (size_t i = 0; i < len; i++) {
        uart_putc(UART_ID, data[i]);
    }

    // Wait for last byte to finish transmitting
//    uart_tx_wait_blocking(UART_ID);
    sleep_ms(100);

    // Switch back to receive
    gpio_put(DE_RE_PIN, 0);
}
*/
int main() {

    platform_init();
//    board_init();

    while (true) {
        gpio_put(GPIO_MOTOR_PUL, 1);
        sleep_ms(5);
        gpio_put(GPIO_MOTOR_PUL, 0);
        sleep_ms(5);
    }
    return 0;

// Initialize DE/RE pin
/*
    gpio_init(DE_RE_PIN);
    gpio_set_dir(DE_RE_PIN, true);
    gpio_put(DE_RE_PIN, 0); // start in receive mode

    gpio_init(GPIO_MOTOR_OE);
    gpio_set_dir(GPIO_MOTOR_OE, GPIO_OUT);
    gpio_put(GPIO_MOTOR_OE, false);

    gpio_init(GPIO_MOTOR_PUL);
    gpio_set_dir(GPIO_MOTOR_PUL, GPIO_OUT);
    gpio_put(GPIO_MOTOR_PUL, false);

    gpio_init(GPIO_MOTOR_DIR);
    gpio_set_dir(GPIO_MOTOR_DIR, GPIO_OUT);
    gpio_put(GPIO_MOTOR_DIR, false);

    gpio_init(GPIO_MOTOR_ENA);
    gpio_set_dir(GPIO_MOTOR_ENA, GPIO_OUT);
    gpio_put(GPIO_MOTOR_ENA, false);

    gpio_init(GPIO_FAN);
    gpio_set_dir(GPIO_FAN, GPIO_OUT);
    gpio_put(GPIO_FAN, false);

    gpio_init(GPIO_LIMIT_HOME);
    gpio_set_dir(GPIO_LIMIT_HOME, GPIO_IN);
    gpio_pull_up(GPIO_LIMIT_HOME);

    adc_init();
    adc_gpio_init(27);       // Initialize ADC1 on GP27
    adc_select_input(1);     // Select ADC1

    int index = 0;
    while (true) {
        gpio_put(GPIO_MOTOR_PUL, 1);
        sleep_ms(5);
        gpio_put(GPIO_MOTOR_PUL, 0);
//        sleep_ms(5);

        uint16_t result = adc_read();       // 12-bit result 0-4095
        float voltage = result * 3.3f / 4095.0f;
        //printf("Voltage: %f\n", voltage);
        sleep_ms(result);        

//        if (gpio_get(GPIO_LIMIT_HOME) == 0) {
  //          gpio_put(GPIO_MOTOR_DIR, true);
    //    } else {
      //      gpio_put(GPIO_MOTOR_DIR, false);
        //}

        if (index++ > 300) {
            index = 0;
            max485_send((uint8_t *)"Hello from RP2040!\n", 20);
        }   
    }
*/
    return 0;
}