#include "pico/stdlib.h"
#include "ST7789.h"
#include "TOUCH.h"
#include <stdio.h>

// ---- UI layout / theme ----------------------------------------------------
#define BG        COLOR_BLACK
#define ACCENT    COLOR_CYAN
#define TITLE_FG  COLOR_WHITE
#define ITEM_FG   COLOR_WHITE
#define SEL_FG    COLOR_BLACK
#define SEL_BG    COLOR_CYAN

static const char *menu_items[] = {
    "Status",
    "Lights",
    "Sensors",
    "Settings",
    "About",
};
#define MENU_COUNT (int)(sizeof(menu_items) / sizeof(menu_items[0]))

// Each menu row is a touch target.
#define ITEM_H     34
#define ITEM_X     14
#define LIST_TOP   54
#define ROW_X      8
#define ROW_W      224
#define ROW_H      (ITEM_H - 6)

static int selected = -1;   // -1 = nothing highlighted

static void draw_header(const char *title) {
    st7789_fill_rect(0, 0, 240, 44, COLOR_NAVY);
    st7789_draw_text(14, 14, title, TITLE_FG, COLOR_NAVY, 2);
    st7789_draw_hline(0, 44, 240, ACCENT);
}

static void draw_item(int index, bool highlight) {
    int y = LIST_TOP + index * ITEM_H;
    uint16_t fg = highlight ? SEL_FG : ITEM_FG;
    uint16_t bg = highlight ? SEL_BG : BG;
    st7789_fill_rect(ROW_X, y, ROW_W, ROW_H, bg);
    st7789_draw_rect(ROW_X, y, ROW_W, ROW_H, highlight ? ACCENT : COLOR_DKGRAY);
    st7789_draw_text(ITEM_X + 8, y + 7, menu_items[index], fg, bg, 2);
}

static void draw_menu(void) {
    st7789_fill(BG);
    draw_header("MAIN MENU");
    for (int i = 0; i < MENU_COUNT; i++) draw_item(i, false);
    st7789_draw_text(14, 222, "Tap an item", COLOR_GRAY, BG, 1);
}

// Which row (if any) does a screen coordinate fall in?
static int row_at(int x, int y) {
    if (x < ROW_X || x > ROW_X + ROW_W) return -1;
    for (int i = 0; i < MENU_COUNT; i++) {
        int ry = LIST_TOP + i * ITEM_H;
        if (y >= ry && y < ry + ROW_H) return i;
    }
    return -1;
}

static void show_detail(int index) {
    st7789_fill(BG);
    draw_header(menu_items[index]);
    st7789_draw_text(16, 64, "You opened:", COLOR_GRAY, BG, 1);
    st7789_draw_text(16, 80, menu_items[index], ACCENT, BG, 2);

    st7789_fill_circle(40, 150, 22, COLOR_GREEN);
    st7789_draw_rect(80, 128, 130, 44, COLOR_WHITE);
    st7789_draw_text(92, 144, "OK / RUNNING", COLOR_GREEN, BG, 1);

    // A "Back" button as a touch target
    st7789_fill_rect(70, 200, 100, 30, COLOR_NAVY);
    st7789_draw_rect(70, 200, 100, 30, ACCENT);
    st7789_draw_text(92, 209, "< BACK", TITLE_FG, COLOR_NAVY, 1);

    // Wait for a tap on the Back button (then debounce the release)
    while (true) {
        int x, y;
        if (touch_read(&x, &y, NULL, NULL)) {
            if (x >= 70 && x <= 170 && y >= 200 && y <= 230) {
                while (touch_read(&x, &y, NULL, NULL)) sleep_ms(10); // wait release
                break;
            }
        }
        sleep_ms(10);
    }
    draw_menu();
}

int main(void) {
    stdio_init_all();
    sleep_ms(500);            // give USB serial a moment so the scan is visible

    st7789_init();
    st7789_set_rotation(0);   // try 1/2/3 if the image is rotated/mirrored

    // ---- DISPLAY SELF-TEST -------------------------------------------------
    // Set to 1 to flash solid colors at boot (proves the display pipeline).
    // Set back to 0 once the screen works.
#define DISPLAY_TEST 1
#if DISPLAY_TEST
    st7789_fill(COLOR_RED);    sleep_ms(700);
    st7789_fill(COLOR_GREEN);  sleep_ms(700);
    st7789_fill(COLOR_BLUE);   sleep_ms(700);
    st7789_fill(COLOR_WHITE);  sleep_ms(700);
    st7789_fill(COLOR_BLACK);  sleep_ms(300);
#endif
    // ------------------------------------------------------------------------

    touch_chip_t tc = touch_init();

    draw_menu();

    // Show on-screen which chip we found (helpful first-boot feedback)
    char msg[40];
    snprintf(msg, sizeof(msg), "Touch: %s", touch_chip_name());
    st7789_draw_text(140, 222, msg, (tc == TOUCH_NONE) ? COLOR_RED : COLOR_GREEN, BG, 1);

    bool was_down = false;
    while (true) {
        int x, y, rx, ry;
        bool down = touch_read(&x, &y, &rx, &ry);

        // Act on the rising edge of a tap (press, not hold)
        if (down && !was_down) {
            printf("TAP screen=(%d,%d) raw=(%d,%d)\n", x, y, rx, ry); // for calibration
            int r = row_at(x, y);
            if (r >= 0) {
                selected = r;
                draw_item(r, true);     // flash highlight
                sleep_ms(120);
                show_detail(r);
            }
        }
        was_down = down;
        sleep_ms(10);
    }
}