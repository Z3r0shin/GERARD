// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the 0.96 inch SSD1306 OLED, 128 x 64, on I2C.
//
// Such modules answer at 0x3C or 0x3D, so both are
// tried. While no display answers, the firmware keeps running and looks again every few
// seconds, so the display can be plugged in later and a missing display is visible on the
// serial portal.
//
// Row layout: U8g2's usual SSD1306 set-up sends the "alternative" COM pin layout (command 0xDA,
// 0x12). Some two-colour modules are wired for the sequential layout (0xDA, 0x02, U8g2's "ALT0").
// Here the layout is a setting, sent after set-up, so it can be switched from the serial portal.
// The default is the standard one: on the rev A display the sequential layout scrambles the
// text.
//
// Test patterns (serial portal, hardware check) light known rows, to tell a fault of the panel
// from one of the firmware. They show at full brightness.
//
// Dimming: on an SSD1306 the contrast command alone barely dims the panel while the VCOMH level is
// high. Dimmed, the screen also gets a short pre-charge and the lowest VCOMH level, which brings it
// to about a tenth of full brightness (sources: github.com/thePunderWoman/SnipsControllers/pull/79,
// github.com/arendst/Tasmota/discussions/22999). Not dimmed: U8g2's own set-up values. U8g2 notes
// that VCOMH 0 does not suit every module (its issue #116); it is used only while dimmed. The
// display's set-up (oled.begin()) resets all three, so they are sent again after it.
//
// Layout of a screen: each line is placed from its font's own ascent and descent, so no font
// can reach over the top band's edge or past the bottom row.
//
// Fonts (U8g2), all free to redistribute:
//   Inconsolata LGC Bold, SIL Open Font License; Logisoso, by Mathieu Gabiot, GPL-2.0 with font
//   exception or SIL Open Font License; FreeUniversal Bold, by Stephen Wilson, SIL Open Font
//   License; Spleen, by Frederic Cambus, BSD-2-Clause; ProFont, MIT licence; X11 7x14 Bold,
//   public domain.

#include "display.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <string.h>

#include "config.h"
#include "pins.h"
#include "views.h"

namespace {

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

constexpr uint8_t CANDIDATES[] = {0x3C, 0x3D};
constexpr int YELLOW_ROWS = 16;  // rows 0 to 15
constexpr int BLUE_TOP = 18;     // first row the large line may use (a 2-row margin under the top band)
constexpr int LAST_ROW = 63;
constexpr int FOOT_ROWS = 10;    // kept for the small line or the dots at the bottom

struct BigFont {
    const char *name;
    const uint8_t *sizes[3];  // largest first; the first that fits is used
};

const BigFont BIG_FONTS[] = {
    {"Inconsolata Bold", {u8g2_font_inb24_mr, u8g2_font_inb19_mr, u8g2_font_inb16_mr}},
    {"Logisoso", {u8g2_font_logisoso28_tr, u8g2_font_logisoso22_tr, u8g2_font_logisoso18_tr}},
    {"FreeUniversal Bold", {u8g2_font_fub25_tr, u8g2_font_fub20_tr, u8g2_font_fub17_tr}},
    {"Spleen", {u8g2_font_spleen16x32_mr, u8g2_font_spleen12x24_mr, u8g2_font_spleen8x16_mr}},
};
constexpr uint8_t BIG_COUNT = sizeof(BIG_FONTS) / sizeof(BIG_FONTS[0]);

struct SmallFont {
    const char *name;
    const uint8_t *font;
};

const SmallFont SMALL_FONTS[] = {
    {"ProFont 15", u8g2_font_profont15_mr},
    {"Spleen 8x16", u8g2_font_spleen8x16_mr},
    {"X11 7x14 Bold", u8g2_font_7x14B_mr},
    {"FreeUniversal Bold 11", u8g2_font_fub11_tr},
};
constexpr uint8_t SMALL_COUNT = sizeof(SMALL_FONTS) / sizeof(SMALL_FONTS[0]);
const uint8_t *const SMALL_FALLBACK = u8g2_font_profont12_mr;  // when the band text is too wide
const uint8_t *const FOOT_FONT = u8g2_font_profont10_mr;

bool isPresent = false;
uint8_t addr = 0;
uint32_t lastProbe = 0;
bool haveProbed = false;
Frame shown;
bool shownValid = false;

bool sequentialRows = false;
bool dimKnown = false;  // the display's brightness registers match dimSent
bool dimSent = false;
uint8_t pattern = 0;
uint8_t bigIndex = 0;
uint8_t smallIndex = 0;

bool answers(uint8_t a) {
    Wire.beginTransmission(a);
    return Wire.endTransmission() == 0;
}

uint8_t findDisplay() {
    for (uint8_t a : CANDIDATES) {
        if (answers(a)) {
            return a;
        }
    }
    return 0;
}

void sendRowLayout() {
    oled.sendF("ca", 0xDA, sequentialRows ? 0x02 : 0x12);
}

// Contrast (0x81), pre-charge period (0xD9) and VCOMH deselect level (0xDB).
void sendBrightness(bool dim) {
    if (dim) {
        oled.sendF("cacaca", 0x81, 0x28, 0xD9, 0x11, 0xDB, 0x00);
    } else {
        oled.sendF("cacaca", 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40);
    }
    dimSent = dim;
    dimKnown = true;
}

// Left edge for text of width w, centred; never negative (a negative x would wrap in U8g2's unsigned
// coordinates and draw stray pixels at the left edge).
int centreX(int w) { return w < 128 ? (128 - w) / 2 : 0; }

// Draws text centred across the width, with its glyphs between rows top and bottom.
void drawLine(const char *text, int top, int bottom) {
    int ascent = oled.getAscent();
    int descent = oled.getDescent();  // zero or negative
    int height = ascent - descent;
    int room = bottom - top + 1;
    int baseline = top + (room > height ? (room - height) / 2 : 0) + ascent;
    oled.drawStr(centreX(oled.getStrWidth(text)), baseline, text);
}

void drawPattern() {
    oled.clearBuffer();
    for (int y = 0; y <= LAST_ROW; y++) {
        bool lit = pattern == 1 || (pattern == 2 && y % 2 == 0) || (pattern == 3 && y % 2 == 1) ||
                   (pattern == 4 && y < YELLOW_ROWS);
        if (lit) {
            oled.drawHLine(0, y, 128);
        } else if (pattern == 5 && y % 4 == 0) {
            oled.drawHLine(0, y, y % 8 == 0 ? 128 : 64);
        }
    }
    oled.sendBuffer();
}

void draw(const Frame &f) {
    oled.clearBuffer();
    if (!f.off) {
        // Top band (rows 0 to 15): the chosen small font, or a narrower one if the text is too wide.
        oled.setFont(SMALL_FONTS[smallIndex].font);
        if (oled.getStrWidth(f.top) > 126) {
            oled.setFont(SMALL_FALLBACK);
        }
        drawLine(f.top, 0, YELLOW_ROWS - 1);

        // Lower part: the biggest size of the chosen large font that fits both ways. The large line
        // has no lower-case letters, so only its ascent must fit, plus one row: a few glyphs, such
        // as Logisoso's '?', reach one row above their font's ascent.
        bool foot = f.small[0] || f.dots;
        int bottom = foot ? LAST_ROW - FOOT_ROWS : LAST_ROW;
        for (const uint8_t *font : BIG_FONTS[bigIndex].sizes) {
            oled.setFont(font);
            if (oled.getStrWidth(f.big) <= 126 && oled.getAscent() + 1 <= bottom - BLUE_TOP + 1) {
                break;
            }
        }

        int ascent = oled.getAscent();
        int room = bottom - BLUE_TOP + 1;
        int baseline = BLUE_TOP + 1 + (room > ascent + 1 ? (room - ascent - 1) / 2 : 0) + ascent;
        oled.drawStr(centreX(oled.getStrWidth(f.big)), baseline, f.big);

        if (f.small[0]) {
            oled.setFont(FOOT_FONT);
            drawLine(f.small, LAST_ROW - FOOT_ROWS + 2, LAST_ROW);
        }
        if (f.dots) {
            int x0 = 64 - (f.dots * 8 - 4) / 2;
            for (uint8_t i = 0; i < f.dots; i++) {
                int x = x0 + i * 8 + 2;
                if (i == f.dot) {
                    oled.drawDisc(x, LAST_ROW - 3, 2);
                } else {
                    oled.drawCircle(x, LAST_ROW - 3, 2);
                }
            }
        }
        // Run view: copied over the count in rows 16 to 63 (pages 2 to 7 of the buffer, which has the
        // view's own layout: 128 bytes a page, each byte 8 rows, top row in bit 0).
        if (f.viewCut) {
            uint8_t *buf = oled.getBufferPtr();
            const uint8_t *vb = views::buffer();
            int cut = f.viewCut > 128 ? 128 : f.viewCut;
            for (int page = 2; page < 8; page++) {
                memcpy(buf + page * 128, vb + (page - 2) * 128, cut);
            }
        }
    }
    oled.sendBuffer();
}

}  // namespace

namespace display {

void begin() {
    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.setClock(I2C_CLOCK_HZ);
    Wire.setTimeout(I2C_TIMEOUT_MS, true);  // true: reset the I2C block after a timeout
    Wire.begin();
}

void update(uint32_t now, const Frame &frame) {
    if (!isPresent) {
        if (haveProbed && now - lastProbe < DISPLAY_PROBE_PERIOD_MS) {
            return;
        }
        haveProbed = true;
        lastProbe = now;
        addr = findDisplay();
        if (!addr) {
            return;
        }
        oled.setI2CAddress(addr << 1);
        oled.setBusClock(I2C_CLOCK_HZ);
        oled.begin();
        sendRowLayout();
        dimKnown = false;
        isPresent = true;
        shownValid = false;
    } else if (now - lastProbe >= DISPLAY_PROBE_PERIOD_MS) {
        lastProbe = now;
        if (!answers(addr)) {
            isPresent = false;
            addr = 0;
            return;
        }
    }
    if (shownValid && memcmp(&shown, &frame, sizeof(Frame)) == 0) {
        return;
    }
    bool dim = frame.dim && !pattern;
    if (!dimKnown || dim != dimSent) {
        sendBrightness(dim);
    }
    if (pattern) {
        drawPattern();
        oled.setPowerSave(0);
    } else {
        draw(frame);
        oled.setPowerSave(frame.off ? 1 : 0);
    }
    shown = frame;
    shownValid = true;
}

bool present() { return isPresent; }
uint8_t address() { return addr; }

int scan(uint8_t *found, int max) {
    int n = 0;
    for (uint8_t a = 0x08; a < 0x78 && n < max; a++) {
        if (answers(a)) {
            found[n++] = a;
        }
    }
    return n;
}

void setSequentialRows(bool sequential) {
    sequentialRows = sequential;
    if (isPresent) {
        sendRowLayout();
    }
    shownValid = false;
}

void setBigFont(uint8_t index) {
    bigIndex = index < BIG_COUNT ? index : 0;
    shownValid = false;
}

void setSmallFont(uint8_t index) {
    smallIndex = index < SMALL_COUNT ? index : 0;
    shownValid = false;
}

void setTestPattern(uint8_t p) {
    pattern = p <= TEST_PATTERNS ? p : 0;
    shownValid = false;
}

uint8_t testPattern() { return pattern; }

const char *testPatternName(uint8_t p) {
    static const char *const NAMES[] = {"off", "every pixel lit", "even rows only (0, 2, 4 ...)",
                                        "odd rows only (1, 3, 5 ...)", "top band filled (rows 0 to 15)",
                                        "ruler: a row every 4, longer every 8"};
    return p <= TEST_PATTERNS ? NAMES[p] : "?";
}

uint8_t bigFontCount() { return BIG_COUNT; }
uint8_t smallFontCount() { return SMALL_COUNT; }
const char *bigFontName(uint8_t index) { return index < BIG_COUNT ? BIG_FONTS[index].name : "?"; }
const char *smallFontName(uint8_t index) { return index < SMALL_COUNT ? SMALL_FONTS[index].name : "?"; }
uint8_t bigFont() { return bigIndex; }
uint8_t smallFont() { return smallIndex; }

}  // namespace display
