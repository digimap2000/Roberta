#include "pico/stdlib.h"

#define GPIO_MOTOR_OE 6
#define GPIO_MOTOR_PUL 7
#define GPIO_MOTOR_DIR 8
#define GPIO_MOTOR_ENA 9

int main() {

    gpio_init(GPIO_MOTOR_OE);
    gpio_set_dir(GPIO_MOTOR_OE, GPIO_OUT);
    gpio_put(GPIO_MOTOR_OE, false);

    gpio_init(GPIO_MOTOR_PUL);
    gpio_set_dir(GPIO_MOTOR_PUL, GPIO_OUT);
    gpio_put(GPIO_MOTOR_PUL, false);

    gpio_init(GPIO_MOTOR_DIR);
    gpio_set_dir(GPIO_MOTOR_DIR, GPIO_OUT);
    gpio_put(GPIO_MOTOR_DIR, false);

    int iIndex = 0;
    int iDir = 0;
    while (1) {
        gpio_put(GPIO_MOTOR_PUL, 1);
        sleep_us(2500);
        gpio_put(GPIO_MOTOR_PUL, 0);
        sleep_us(2500);

        if (iIndex % 1000 == 0) {
            iDir = !iDir;
//            gpio_put(GPIO_MOTOR_DIR, iDir);
        }
        iIndex++;
    }
}
