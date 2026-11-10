// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware, hardware revision A: a desk focus timer on an RP2040-Zero.
//
// Start-up order: the buzzer pin is driven low first, so the buzzer cannot sound by accident;
// then the serial portal, the inputs, the display bus and the saved record. The watchdog starts
// last, so a first-time format of the flash storage cannot trip it.

#include <Arduino.h>
#include <hardware/watchdog.h>

#include "app.h"
#include "config.h"
#include "console.h"
#include "display.h"
#include "input.h"
#include "sound.h"
#include "views.h"

void setup() {
    sound::begin();
    bool watchdogReset = watchdog_enable_caused_reboot();
    console::begin(watchdogReset);
    input::begin();
    display::begin();
    views::begin();
    app::begin(millis(), watchdogReset);
    rp2040.wdt_begin(WATCHDOG_MS);
}

void loop() {
    uint32_t now = millis();
    console::update(now);
    app::update(now);
    rp2040.wdt_reset();
}
