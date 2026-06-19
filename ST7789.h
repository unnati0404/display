#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include "hardware/spi.h"

// ---------------------------------------------------------------------------
// Pin / hardware configuration. Change these to match how you wired the panel.
// These are the pins used in the wiring guide (SPI0).
// ---------------------------------------------------------------------------
// Pins that your hardware actually responds on (backlight lights up here).
#define ST7789_SPI_PORT spi0
#define ST7789_PIN_SCK   18   // SCL  -> GP18
#define ST7789_PIN_MOSI  19   // SDA  -> GP19
#define ST7789_PIN_CS    17   // CSX  -> GP17
#define ST7789_PIN_DC    20   // DCX  -> GP20
#define ST7789_PIN_RST   21   // REST -> GP21
#define ST7789_PIN_BL    22   // LEDA/backlight -> GP22 (driven on at init)

// Panel size. Most square ST7789 boards are 240x240; tall ones are 240x320.
#define ST7789_WIDTH    240
#define ST7789_HEIGHT   240

#define ST7789_SPI_HZ   (8 * 1000 * 1000)  // 8 MHz baseline; raise later once working

// ---------------------------------------------------------------------------
// 16-bit RGB565 colors
// ---------------------------------------------------------------------------
#define RGB565(r,g,b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410
#define COLOR_DKGRAY  0x4208
#define COLOR_ORANGE  0xFD20
#define COLOR_NAVY    0x000F

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void st7789_init(void);
void st7789_set_rotation(uint8_t rotation);              // 0..3
void st7789_fill(uint16_t color);                        // whole screen
void st7789_fill_rect(int x, int y, int w, int h, uint16_t color);
void st7789_draw_rect(int x, int y, int w, int h, uint16_t color);
void st7789_draw_pixel(int x, int y, uint16_t color);
void st7789_draw_hline(int x, int y, int w, uint16_t color);
void st7789_draw_vline(int x, int y, int h, uint16_t color);
void st7789_draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void st7789_draw_circle(int x0, int y0, int r, uint16_t color);
void st7789_fill_circle(int x0, int y0, int r, uint16_t color);

// Text. 'size' scales the 5x7 font (1 = 5x7 px, 2 = 10x14 px, ...).
void st7789_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, uint8_t size);
void st7789_draw_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, uint8_t size);

#endif // ST7789_H