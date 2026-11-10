// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the rotary encoder and its push button.
#pragma once

#include <stdint.h>

enum class ButtonEvent : uint8_t {
    None,
    Down,   // the button has just gone down (for the click sound)
    Short,  // released before LONG_PRESS_MS
    Long,   // held for LONG_PRESS_MS (fires once, while still held)
};

namespace input {

void begin();

// Detents turned since the last call: positive clockwise, negative anticlockwise.
int takeDetents();

// Call often from the main loop. Returns at most one event per call.
ButtonEvent pollButton(uint32_t now);

// true: turning the knob counts the other way round from ENC_DIRECTION.
void setReversed(bool reversed);

// Raw levels, for the serial portal.
bool clkLevel();
bool dtLevel();
bool swLevel();

}  // namespace input
