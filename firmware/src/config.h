// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: timings and settings that are fixed at build time.
#pragma once

#include <stdint.h>

// Focus and breaks.
// The time before a break prompt, the break tune's repeat, the idle and dimming times, the count's
// time, the countdown's start, the hold time and "DONE!"'s time are set from the serial portal
// (timings.h). A pause or a stop never counts as a break: only ending a break prompt restarts the
// time to the next one.
constexpr uint32_t BREAK_TEXT_PERIOD_MS = 2000;            // break prompt texts change this often
// The break tune repeats until the prompt is answered; the prompt never gives up, never goes off and
// never dims.

// Countdown, in minutes.
// Its range, 1 to 240, is the "Countdown starts at" timing's (timings.cpp).
constexpr int COUNTDOWN_FAST_STEP = 5;           // step when the knob turns fast
constexpr uint32_t FAST_TURN_MS = 60;            // detents closer than this count as fast

// Run views: after the count's time (timings.h), the view is revealed from
// the left.
constexpr uint32_t VIEW_REVEAL_MS = 300;  // 3 frames
constexpr uint32_t VIEW_FRAME_MS = 100;   // 10 frames a second

// Short screens.
constexpr uint32_t BOOT_SCREEN_MS = 2500;
constexpr uint32_t POOF_SCREEN_MS = 1000;
constexpr uint32_t SLEEP_SCREEN_MS = 1500;

// Push button.
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;

// Encoder. To check on the bench: transitions per detent, and which way is clockwise.
// FULL_STEP_DETENT true: one detent is a full cycle of four transitions (rest with CLK and DT high).
// false: one detent is half a cycle (rest with both high or both low).
constexpr bool ENC_FULL_STEP_DETENT = true;
// -1: the natural direction with the KY-040 as wired on the schematic. The serial portal can
// reverse it (a saved setting).
constexpr int ENC_DIRECTION = -1;

// Push button level when pressed. To check on the bench; the KY-040's switch is expected to pull SW to ground.
constexpr bool ENC_SW_ACTIVE_LOW = true;

// Display.
constexpr uint32_t I2C_CLOCK_HZ = 400000;
constexpr uint32_t I2C_TIMEOUT_MS = 25;  // a stuck bus gives up after this, not after the default 1 s
constexpr uint32_t DISPLAY_PROBE_PERIOD_MS = 5000;  // how often a missing display is looked for

// Lifetime stats are saved after this much unsaved focus time, and at every stop, pause and sleep.
constexpr uint32_t SAVE_AFTER_MS = 5UL * 60 * 1000;

// Watchdog: the board restarts if the main loop stops for this long.
constexpr uint32_t WATCHDOG_MS = 4000;

// Serial portal.
constexpr uint32_t SERIAL_BAUD = 115200;
