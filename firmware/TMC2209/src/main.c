#include <stdio.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "esp_timer.h"

#define PIN_STEP 8
#define PIN_DIR 7

#define PIN_UP 1
#define PIN_DOWN 2

#define TMC_PIN       GPIO_NUM_5    // the single-wire UART GPIO

extern void tmc_set_microstepping(uint8_t mres);

void app_main()
{
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << PIN_STEP),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&io_conf);
    }

    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << PIN_DIR),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&io_conf);
    }

    {
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << TMC_PIN),
            .mode = GPIO_MODE_OUTPUT_OD,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = 0,
            .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&cfg);
        gpio_set_level(TMC_PIN, 1);
        usleep(5000);
    }

    tmc_set_microstepping(8);

    int iPos = 0;
    int msDelay = 10;
    int dir = 0;
    while (1)
    {
        iPos++;
        if ((iPos % 100) == 0)
        {
            dir = !dir;
//            gpio_set_level(PIN_DIR, dir);
            tmc_set_microstepping(8);
        }

        gpio_set_level(PIN_STEP, 1);
        usleep(msDelay * 1000);
        gpio_set_level(PIN_STEP, 0);
        usleep(msDelay * 1000);
    }
}