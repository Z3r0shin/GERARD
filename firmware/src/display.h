// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the 0.96 inch SSD1306 OLED, 128 x 64, on I2C.
//
// The screen has a top band of 16 rows and a lower part of 48 rows (on a two-colour module, the
// band and the part differ in colour). Every screen has a short line in the top band and one
// large line in the lower part,
// with an optional small line under it and optional position dots.
#pragma once

#include <stdint.h>

struct Frame {
    char top[24];    // top band
    char big[16];    // large text, lower part
    char small[24];  // small text under the large text, may be empty
    uint8_t dots;    // number of position dots under the large text; 0 = none
    uint8_t dot;     // which dot is filled
    bool off;        // screen blanked
    bool dim;        // screen dimmed
    uint8_t viewCut;    // run view: columns 0 to viewCut - 1 of rows 16 to 63 show it; 0 = no view
    uint32_t viewTick;  // run view frame number, so a new frame of the view redraws the screen
};

namespace display {

void begin();

// Call often. Looks for the display while it is missing, and redraws when the frame changed.
void update(uint32_t now, const Frame &frame);

bool present();
uint8_t address();  // 7-bit I2C address in use, 0 if none found

// Scans the I2C bus. Writes up to max addresses into found; returns how many answered.
int scan(uint8_t *found, int max);

// Settings, applied at once (the screen is redrawn).
// sequential true: the sequential COM row layout (U8g2's "ALT0"); false: the standard one, which
// the rev A display needs.
void setSequentialRows(bool sequential);

// Test patterns, drawn instead of the device's screens while one is on (0 = off):
// 1 every pixel lit; 2 even rows only; 3 odd rows only; 4 the top band (rows 0-15) filled,
// the rest dark; 5 a ruler: one row lit every 4 rows, with every 8th row longer.
constexpr uint8_t TEST_PATTERNS = 5;
void setTestPattern(uint8_t pattern);
uint8_t testPattern();
const char *testPatternName(uint8_t pattern);
void setBigFont(uint8_t index);    // out of range: the first font
void setSmallFont(uint8_t index);  // out of range: the first font
uint8_t bigFontCount();
uint8_t smallFontCount();
const char *bigFontName(uint8_t index);
const char *smallFontName(uint8_t index);
uint8_t bigFont();
uint8_t smallFont();

}  // namespace display
