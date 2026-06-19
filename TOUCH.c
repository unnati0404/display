#include "TOUCH.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>

#define ADDR_CST816  0x15
#define ADDR_FT6236  0x38
#define ADDR_GT911_A 0x5D
#define ADDR_GT911_B 0x14

static touch_chip_t chip = TOUCH_NONE;
static uint8_t chip_addr = 0;

// --- low-level I2C helpers -------------------------------------------------
static bool i2c_present(uint8_t addr) {
    uint8_t dummy;
    int r = i2c_read_blocking(TOUCH_I2C_PORT, addr, &dummy, 1, false);
    return r >= 0;
}

// 8-bit register read (CST816 / FT6236)
static bool rd8(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    if (i2c_write_blocking(TOUCH_I2C_PORT, addr, &reg, 1, true) < 0) return false;
    return i2c_read_blocking(TOUCH_I2C_PORT, addr, buf, len, false) == (int)len;
}

// 16-bit register access (GT911)
static bool rd16(uint8_t addr, uint16_t reg, uint8_t *buf, size_t len) {
    uint8_t r[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    if (i2c_write_blocking(TOUCH_I2C_PORT, addr, r, 2, true) < 0) return false;
    return i2c_read_blocking(TOUCH_I2C_PORT, addr, buf, len, false) == (int)len;
}
static void wr16(uint8_t addr, uint16_t reg, uint8_t val) {
    uint8_t b[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val };
    i2c_write_blocking(TOUCH_I2C_PORT, addr, b, 3, false);
}

// --- init / detect ---------------------------------------------------------
touch_chip_t touch_init(void) {
    i2c_init(TOUCH_I2C_PORT, TOUCH_I2C_HZ);
    gpio_set_function(TOUCH_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(TOUCH_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(TOUCH_PIN_SDA);
    gpio_pull_up(TOUCH_PIN_SCL);

    if (TOUCH_PIN_RST >= 0) {
        gpio_init(TOUCH_PIN_RST); gpio_set_dir(TOUCH_PIN_RST, GPIO_OUT);
        gpio_put(TOUCH_PIN_RST, 0); sleep_ms(10);
        gpio_put(TOUCH_PIN_RST, 1); sleep_ms(50);
    }
    sleep_ms(50);

    // Scan and report — open the Serial Monitor (115200) to see this.
    printf("I2C scan on SDA=GP%d SCL=GP%d:\n", TOUCH_PIN_SDA, TOUCH_PIN_SCL);
    int found = 0;
    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (i2c_present(a)) { printf("  device at 0x%02X\n", a); found++; }
    }
    if (!found) printf("  (none found - check wiring/pull-ups)\n");

    if (i2c_present(ADDR_CST816))       { chip = TOUCH_CST816; chip_addr = ADDR_CST816; }
    else if (i2c_present(ADDR_FT6236))  { chip = TOUCH_FT6236; chip_addr = ADDR_FT6236; }
    else if (i2c_present(ADDR_GT911_A)) { chip = TOUCH_GT911;  chip_addr = ADDR_GT911_A; }
    else if (i2c_present(ADDR_GT911_B)) { chip = TOUCH_GT911;  chip_addr = ADDR_GT911_B; }
    else                                { chip = TOUCH_NONE;   chip_addr = 0; }

    printf("Detected touch chip: %s (addr 0x%02X)\n", touch_chip_name(), chip_addr);
    return chip;
}

touch_chip_t touch_chip(void) { return chip; }

const char *touch_chip_name(void) {
    switch (chip) {
        case TOUCH_CST816: return "CST816";
        case TOUCH_FT6236: return "FT6236";
        case TOUCH_GT911:  return "GT911";
        default:           return "NONE";
    }
}

// --- map raw -> screen coordinates ----------------------------------------
static void map_coords(int rx, int ry, int *x, int *y) {
#if TOUCH_SWAP_XY
    int t = rx; rx = ry; ry = t;
#endif
#if TOUCH_FLIP_X
    rx = (TOUCH_MAX_X - 1) - rx;
#endif
#if TOUCH_FLIP_Y
    ry = (TOUCH_MAX_Y - 1) - ry;
#endif
    *x = rx; *y = ry;
}

// --- read coordinates ------------------------------------------------------
bool touch_read(int *x, int *y, int *raw_x, int *raw_y) {
    int rx = 0, ry = 0;
    bool down = false;

    if (chip == TOUCH_CST816 || chip == TOUCH_FT6236) {
        // Both share layout: reg 0x02 = #points, 0x03..0x06 = X/Y (high nibble).
        uint8_t b[5];
        if (rd8(chip_addr, 0x02, b, 5)) {
            uint8_t points = b[0] & 0x0F;
            if (points > 0 && points != 0x0F) {
                rx = ((b[1] & 0x0F) << 8) | b[2];
                ry = ((b[3] & 0x0F) << 8) | b[4];
                down = true;
            }
        }
    } else if (chip == TOUCH_GT911) {
        uint8_t status;
        if (rd16(chip_addr, 0x814E, &status, 1)) {
            uint8_t points = status & 0x0F;
            if ((status & 0x80) && points > 0) {
                uint8_t b[4];
                if (rd16(chip_addr, 0x8150, b, 4)) {  // point 1: xL,xH,yL,yH
                    rx = b[0] | (b[1] << 8);
                    ry = b[2] | (b[3] << 8);
                    down = true;
                }
            }
            wr16(chip_addr, 0x814E, 0);  // clear status flag
        }
    }

    if (raw_x) *raw_x = rx;
    if (raw_y) *raw_y = ry;
    if (down) map_coords(rx, ry, x, y);
    return down;
}