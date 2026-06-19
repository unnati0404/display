#include "ST7789.h"
#include "font5x7.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdlib.h>
#include <string.h>

// ---- ST7789 command set (subset) ------------------------------------------
#define ST_SWRESET 0x01
#define ST_SLPOUT  0x11
#define ST_NORON   0x13
#define ST_INVON   0x21
#define ST_DISPON  0x29
#define ST_CASET   0x2A
#define ST_RASET   0x2B
#define ST_RAMWR   0x2C
#define ST_MADCTL  0x36
#define ST_COLMOD  0x3A

// Column/row offsets. Many 240x240 panels need no offset; some need (0,0),
// others (0,80) depending on which corner the controller starts from.
static int16_t x_offset = 0;
static int16_t y_offset = 0;
static uint16_t cur_w = ST7789_WIDTH;
static uint16_t cur_h = ST7789_HEIGHT;

static inline void cs_low(void)  { if (ST7789_PIN_CS >= 0) gpio_put(ST7789_PIN_CS, 0); }
static inline void cs_high(void) { if (ST7789_PIN_CS >= 0) gpio_put(ST7789_PIN_CS, 1); }
static inline void dc_cmd(void)  { gpio_put(ST7789_PIN_DC, 0); }
static inline void dc_data(void) { gpio_put(ST7789_PIN_DC, 1); }

static void write_cmd(uint8_t cmd) {
    cs_low();
    dc_cmd();
    spi_write_blocking(ST7789_SPI_PORT, &cmd, 1);
    cs_high();
}

static void write_data(const uint8_t *data, size_t len) {
    cs_low();
    dc_data();
    spi_write_blocking(ST7789_SPI_PORT, data, len);
    cs_high();
}

static void write_data8(uint8_t d) { write_data(&d, 1); }

static void set_window(int x0, int y0, int x1, int y1) {
    x0 += x_offset; x1 += x_offset;
    y0 += y_offset; y1 += y_offset;
    uint8_t buf[4];

    write_cmd(ST_CASET);
    buf[0] = x0 >> 8; buf[1] = x0 & 0xFF; buf[2] = x1 >> 8; buf[3] = x1 & 0xFF;
    write_data(buf, 4);

    write_cmd(ST_RASET);
    buf[0] = y0 >> 8; buf[1] = y0 & 0xFF; buf[2] = y1 >> 8; buf[3] = y1 & 0xFF;
    write_data(buf, 4);

    write_cmd(ST_RAMWR);
}

void st7789_init(void) {
    // SPI peripheral. ST7789 uses SPI mode 3 (clock idles high, sample on
    // rising edge). The Pico defaults to mode 0, which leaves many ST7789
    // panels blank, so we set the format explicitly here.
    spi_init(ST7789_SPI_PORT, ST7789_SPI_HZ);
    spi_set_format(ST7789_SPI_PORT, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(ST7789_PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(ST7789_PIN_MOSI, GPIO_FUNC_SPI);

    // Control pins as plain GPIO outputs
    if (ST7789_PIN_CS >= 0) { gpio_init(ST7789_PIN_CS); gpio_set_dir(ST7789_PIN_CS, GPIO_OUT); gpio_put(ST7789_PIN_CS, 1); }
    gpio_init(ST7789_PIN_DC);  gpio_set_dir(ST7789_PIN_DC, GPIO_OUT);
    gpio_init(ST7789_PIN_RST); gpio_set_dir(ST7789_PIN_RST, GPIO_OUT);
    if (ST7789_PIN_BL >= 0) { gpio_init(ST7789_PIN_BL); gpio_set_dir(ST7789_PIN_BL, GPIO_OUT); gpio_put(ST7789_PIN_BL, 1); }

    // Hardware reset
    gpio_put(ST7789_PIN_RST, 1); sleep_ms(50);
    gpio_put(ST7789_PIN_RST, 0); sleep_ms(50);
    gpio_put(ST7789_PIN_RST, 1); sleep_ms(150);

    write_cmd(ST_SWRESET);  sleep_ms(150);
    write_cmd(ST_SLPOUT);   sleep_ms(120);
    write_cmd(ST_COLMOD);   write_data8(0x55);   // 16-bit / pixel (RGB565)
    sleep_ms(10);
    st7789_set_rotation(0);
    write_cmd(ST_INVON);    sleep_ms(10);        // ST7789 IPS panels need inversion ON
    write_cmd(ST_NORON);    sleep_ms(10);
    write_cmd(ST_DISPON);   sleep_ms(120);

    st7789_fill(COLOR_BLACK);
}

void st7789_set_rotation(uint8_t rotation) {
    uint8_t madctl;
    switch (rotation & 3) {
        case 0: madctl = 0x00; cur_w = ST7789_WIDTH;  cur_h = ST7789_HEIGHT; x_offset = 0;  y_offset = 0;  break;
        case 1: madctl = 0x60; cur_w = ST7789_HEIGHT; cur_h = ST7789_WIDTH;  x_offset = 0;  y_offset = 0;  break;
        case 2: madctl = 0xC0; cur_w = ST7789_WIDTH;  cur_h = ST7789_HEIGHT; x_offset = 0;  y_offset = 0;  break;
        default:madctl = 0xA0; cur_w = ST7789_HEIGHT; cur_h = ST7789_WIDTH;  x_offset = 0;  y_offset = 0;  break;
    }
    write_cmd(ST_MADCTL);
    write_data8(madctl);
}

// Stream one color repeatedly into the active window.
static void push_color(uint16_t color, uint32_t count) {
    uint8_t hi = color >> 8, lo = color & 0xFF;
    uint8_t buf[64];
    for (int i = 0; i < 64; i += 2) { buf[i] = hi; buf[i + 1] = lo; }
    cs_low();
    dc_data();
    while (count) {
        uint32_t chunk = count > 32 ? 32 : count;     // 32 pixels = 64 bytes
        spi_write_blocking(ST7789_SPI_PORT, buf, chunk * 2);
        count -= chunk;
    }
    cs_high();
}

void st7789_fill(uint16_t color) {
    set_window(0, 0, cur_w - 1, cur_h - 1);
    push_color(color, (uint32_t)cur_w * cur_h);
}

void st7789_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > cur_w) w = cur_w - x;
    if (y + h > cur_h) h = cur_h - y;
    if (w <= 0 || h <= 0) return;
    set_window(x, y, x + w - 1, y + h - 1);
    push_color(color, (uint32_t)w * h);
}

void st7789_draw_pixel(int x, int y, uint16_t color) {
    if (x < 0 || y < 0 || x >= cur_w || y >= cur_h) return;
    set_window(x, y, x, y);
    push_color(color, 1);
}

void st7789_draw_hline(int x, int y, int w, uint16_t color) { st7789_fill_rect(x, y, w, 1, color); }
void st7789_draw_vline(int x, int y, int h, uint16_t color) { st7789_fill_rect(x, y, 1, h, color); }

void st7789_draw_rect(int x, int y, int w, int h, uint16_t color) {
    st7789_draw_hline(x, y, w, color);
    st7789_draw_hline(x, y + h - 1, w, color);
    st7789_draw_vline(x, y, h, color);
    st7789_draw_vline(x + w - 1, y, h, color);
}

void st7789_draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        st7789_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void st7789_draw_circle(int x0, int y0, int r, uint16_t color) {
    int x = -r, y = 0, err = 2 - 2 * r;
    do {
        st7789_draw_pixel(x0 - x, y0 + y, color);
        st7789_draw_pixel(x0 - y, y0 - x, color);
        st7789_draw_pixel(x0 + x, y0 - y, color);
        st7789_draw_pixel(x0 + y, y0 + x, color);
        r = err;
        if (r <= y) err += ++y * 2 + 1;
        if (r > x || err > y) err += ++x * 2 + 1;
    } while (x < 0);
}

void st7789_fill_circle(int x0, int y0, int r, uint16_t color) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r)
                st7789_draw_pixel(x0 + x, y0 + y, color);
}

void st7789_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, uint8_t size) {
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) c = '?';
    const uint8_t *glyph = font5x7[c - FONT_FIRST_CHAR];
    for (int col = 0; col < FONT_WIDTH; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint16_t color = (bits & (1 << row)) ? fg : bg;
            if (size == 1) st7789_draw_pixel(x + col, y + row, color);
            else           st7789_fill_rect(x + col * size, y + row * size, size, size, color);
        }
    }
    // 1-column gap after the glyph so text doesn't run together
    if (size == 1) st7789_draw_vline(x + FONT_WIDTH, y, FONT_HEIGHT, bg);
    else           st7789_fill_rect(x + FONT_WIDTH * size, y, size, FONT_HEIGHT * size, bg);
}

void st7789_draw_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, uint8_t size) {
    int cx = x;
    while (*s) {
        if (*s == '\n') { y += (FONT_HEIGHT + 1) * size; cx = x; s++; continue; }
        st7789_draw_char(cx, y, *s, fg, bg, size);
        cx += (FONT_WIDTH + 1) * size;
        s++;
    }
}