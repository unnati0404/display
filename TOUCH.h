#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

// ---------------------------------------------------------------------------
// Capacitive touch panel (I2C). Wired per your table:
//   I2C-SDA -> GP4   I2C-SCL -> GP5
//   TP-INT and TP-RST are NOT connected (we poll instead of using interrupts).
// ---------------------------------------------------------------------------
#define TOUCH_I2C_PORT  i2c0
#define TOUCH_PIN_SDA   4
#define TOUCH_PIN_SCL   5
#define TOUCH_PIN_INT   -1     // not wired
#define TOUCH_PIN_RST   -1     // not wired
#define TOUCH_I2C_HZ    400000 // 400 kHz

// The driver figures these out at runtime by scanning the bus.
typedef enum {
    TOUCH_NONE = 0,
    TOUCH_CST816,   // addr 0x15  (also covers CST816S/CST816T/CST820)
    TOUCH_FT6236,   // addr 0x38  (also FT6206/FT6336)
    TOUCH_GT911,    // addr 0x5D or 0x14
} touch_chip_t;

// ---------------------------------------------------------------------------
// Coordinate mapping. Raw touch coordinates may be flipped/swapped relative to
// how the ST7789 is drawn. Flash once, tap corners, watch the RAW values the
// serial monitor prints, then set these so taps line up with what's on screen.
// (Defaults match ST7789 rotation 0 on most of these panels.)
// ---------------------------------------------------------------------------
#define TOUCH_SWAP_XY   0
#define TOUCH_FLIP_X    0
#define TOUCH_FLIP_Y    0
#define TOUCH_MAX_X     240    // raw range of the panel
#define TOUCH_MAX_Y     240

// Scans the I2C bus, prints every address found to stdio, and selects a driver.
// Returns the detected chip (TOUCH_NONE if nothing answered).
touch_chip_t touch_init(void);

touch_chip_t touch_chip(void);          // what was detected
const char  *touch_chip_name(void);

// Returns true if a finger is currently down. When true, *x/*y are filled with
// SCREEN coordinates (already mapped via the SWAP/FLIP settings above).
// raw_x/raw_y (may be NULL) receive the un-mapped values for calibration.
bool touch_read(int *x, int *y, int *raw_x, int *raw_y);

#endif // TOUCH_H