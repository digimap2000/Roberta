#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define TMC_PIN       GPIO_NUM_5    // the single-wire UART GPIO
#define TMC_ADDR      0x00          // driver address (0–3)
#define REG_CHOPCONF  0x6C
#define BIT_DELAY_US  8             // ~115200 baud
#define UART_READ_TIMEOUT_US 50000  // 50 ms timeout

// ---------------- CRC8 calculator (polynomial x^8 + x^2 + x + 1 = 0x07) ----------------
static uint8_t _tmc_crc8(uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t current = data[i];
        for (int b = 0; b < 8; b++) {
            uint8_t mix = (crc ^ current) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C; // reversed polynomial 0x07
            current >>= 1;
        }
    }
    return crc;
}

uint8_t calcCRC(uint8_t datagram[], uint8_t len) {
	uint8_t crc = 0;
	for (uint8_t i = 0; i < len; i++) {
		uint8_t currentByte = datagram[i];
		for (uint8_t j = 0; j < 8; j++) {
			if ((crc >> 7) ^ (currentByte & 0x01)) {
				crc = (crc << 1) ^ 0x07;
			} else {
				crc = (crc << 1);
			}
			crc &= 0xff;
			currentByte = currentByte >> 1;
		}
	}
	return crc;
}

// ---------------- Bit-bang UART send/receive ----------------
static void uart_send_byte(uint8_t byte)
{
    // Start bit
    gpio_set_level(TMC_PIN, 0);
    esp_rom_delay_us(BIT_DELAY_US);

    for (int i = 0; i < 8; i++) {
        gpio_set_level(TMC_PIN, (byte >> i) & 1);
        esp_rom_delay_us(BIT_DELAY_US);
    }

    // Stop bit
    gpio_set_level(TMC_PIN, 1);
    esp_rom_delay_us(BIT_DELAY_US);
}

static bool uart_read_byte(uint8_t *out)
{
    uint8_t byte = 0;
    // Wait for start bit
    uint64_t start = esp_timer_get_time();
    while (gpio_get_level(TMC_PIN)) {
        if (esp_timer_get_time() - start > UART_READ_TIMEOUT_US) return false;
    }

    esp_rom_delay_us(BIT_DELAY_US + BIT_DELAY_US / 2); // center of first bit
    for (int i = 0; i < 8; i++) {
        byte |= gpio_get_level(TMC_PIN) << i;
        esp_rom_delay_us(BIT_DELAY_US);
    }

    // Stop bit (ignore value)
    esp_rom_delay_us(BIT_DELAY_US);
    *out = byte;

    printf("Received byte: 0x%02X\n", byte);
    
    return true;
}

// ---------------- TMC2209 read/write helpers ----------------
static bool tmc_read_register(uint8_t reg, uint32_t *val)
{
    uint8_t tx[4] = {0x05, TMC_ADDR, reg, 0 };
    tx[3] = calcCRC(tx, 3);

    // Send request
    gpio_set_direction(TMC_PIN, GPIO_MODE_OUTPUT_OD);
    esp_rom_delay_us(50);
   // gpio_set_level(TMC_PIN, 1);
 //   esp_rom_delay_us(100);
//    gpio_set_level(TMC_PIN, 0);
//    esp_rom_delay_us(100);
  //  gpio_set_level(TMC_PIN, 1);
    for (int i = 0; i < 4; i++) uart_send_byte(tx[i]);

    // Prepare for read
    gpio_set_level(TMC_PIN, 1);
//    gpio_set_direction(TMC_PIN, GPIO_MODE_INPUT);
    esp_rom_delay_us(100);

    // Expect 8 bytes: 0xFF, addr, reg, 4 data, crc
    uint8_t resp[8];
    for (int i = 0; i < 8; i++) {
        if (!uart_read_byte(&resp[i])) return false;
    }

    // CRC check
    uint8_t crc = calcCRC(&resp[1], 6);
    if (crc != resp[7]) return false;

    *val = ((uint32_t)resp[3] << 24) |
           ((uint32_t)resp[4] << 16) |
           ((uint32_t)resp[5] << 8)  |
           (uint32_t)resp[6];
    return true;
}

static void tmc_write_register(uint8_t reg, uint32_t val)
{
    uint8_t tx[8];
    tx[0] = TMC_ADDR | 0x80;  // write bit
    tx[1] = reg;
    tx[2] = (val >> 24) & 0xFF;
    tx[3] = (val >> 16) & 0xFF;
    tx[4] = (val >> 8)  & 0xFF;
    tx[5] = val & 0xFF;
    tx[6] = calcCRC(tx, 6);

    gpio_set_direction(TMC_PIN, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(TMC_PIN, 1);
    for (int i = 0; i < 7; i++) uart_send_byte(tx[i]);

    // return to idle input mode
//    gpio_set_direction(TMC_PIN, GPIO_MODE_INPUT);
}

// ---------------- Example: read/modify microstepping ----------------
void tmc_set_microstepping(uint8_t mres)
{
    uint32_t chopconf;
    if (!tmc_read_register(REG_CHOPCONF, &chopconf)) {
        printf("Read failed\n");
        return;
    }

    printf("CHOPCONF before = 0x%08X\n", (unsigned int)chopconf);

    // Modify MRES bits [27:24]
    chopconf &= ~(0xF << 24);
    chopconf |= ((mres & 0xF) << 24);

    tmc_write_register(REG_CHOPCONF, chopconf);

    esp_rom_delay_us(5000); // short delay
    uint32_t newval;
    if (tmc_read_register(REG_CHOPCONF, &newval))
        printf("CHOPCONF after  = 0x%08X\n", (unsigned int)newval);
    else
        printf("Re-read failed\n");
}

