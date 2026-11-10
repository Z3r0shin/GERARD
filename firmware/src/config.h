// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: timings and settings that are fixed at build time.
#pragma once

#include <stdint.h>

// Focus and breaks.
constexpr uint32_t BREAK_AFTER_MS = 30UL * 60 * 1000;      // running time before a break prompt
constexpr uint32_t PAUSE_COUNTS_AS_BREAK_MS = 5UL * 60 * 1000;  // a pause this long resets the break clock
constexpr uint32_t BREAK_TEXT_PERIOD_MS = 2000;            // break prompt texts change this often
// The break tune repeats this often until the prompt is answered; the prompt never gives up, never
// goes off and stays at full brightness (the owner, 2026-11-08: "never give up").
constexpr uint32_t BREAK_REMINDER_PERIOD_MS = 60UL * 1000;

// Countdown, in minutes.
constexpr int COUNTDOWN_MIN = 1;
constexpr int COUNTDOWN_MAX = 240;
constexpr int COUNTDOWN_DEFAULT = 30;
constexpr int COUNTDOWN_FAST_STEP = 5;           // step when the knob turns fast
constexpr uint32_t FAST_TURN_MS = 60;            // detents closer than this count as fast

// Idle prompts, counted from the last input, on screens where nothing is running.
constexpr uint32_t IDLE_1_MS = 3UL * 60 * 1000;   // "IDLE?"
constexpr uint32_t IDLE_2_MS = 10UL * 60 * 1000;  // "IDLE?!"
constexpr uint32_t IDLE_3_MS = 30UL * 60 * 1000;  // "IDLE?!?!"
constexpr uint32_t IDLE_OFF_MS = 35UL * 60 * 1000;  // screen off

// The screen dims this long after the last turn or press (the owner, 2026-11-06), on every screen;
// a turn, a press, a break prompt or the countdown's end brings it back to full brightness.
constexpr uint32_t DIM_AFTER_MS = 2UL * 60 * 1000;

// Run views (the owner, 2026-11-06): the count shows this long at a run's start, on a turn or a
// press, and when a run resumes; then the view is revealed from the left.
constexpr uint32_t COUNT_SHOW_MS = 5000;
constexpr uint32_t VIEW_REVEAL_MS = 300;  // 3 frames
constexpr uint32_t VIEW_FRAME_MS = 100;   // 10 frames a second

// Short screens.
constexpr uint32_t BOOT_SCREEN_MS = 2500;
constexpr uint32_t POOF_SCREEN_MS = 1000;
constexpr uint32_t SLEEP_SCREEN_MS = 1500;
constexpr uint32_t DONE_SCREEN_MS = 60UL * 1000;

// Push button.
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t LONG_PRESS_MS = 700;  // a long press means "back"

// Encoder. To check on the bench: transitions per detent, and which way is clockwise.
// FULL_STEP_DETENT true: one detent is a full cycle of four transitions (rest with CLK and DT high).
// false: one detent is half a cycle (rest with both high or both low).
constexpr bool ENC_FULL_STEP_DETENT = true;
// -1: the direction the owner found natural on the rev A unit (2026-11-05). The serial portal can
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
