// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the rotary encoder and its push button.
//
// The encoder is decoded in an interrupt with a transition table: each change of CLK or DT
// is looked up from the previous (CLK, DT) pair to the new one. A valid step counts +1 or -1;
// an impossible jump (contact bounce) counts 0. A detent is counted only when the encoder comes
// back to its rest position with at least two steps in one direction, so bounce cannot add
// or lose detents.

#include "input.h"

#include <Arduino.h>

#include "config.h"
#include "pins.h"

namespace {

// Index: (previous state << 2) | new state, a state being (CLK << 1) | DT.
constexpr int8_t STEP_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

volatile uint8_t encState = 0;
volatile int8_t encSteps = 0;
volatile int32_t encDetents = 0;
volatile int8_t encDirection = ENC_DIRECTION;  // flipped by the knob-direction setting
uint32_t holdMs = 700;  // a press this long is a hold; set from the timings (timings.h)

inline uint8_t readEncoder() {
    return (uint8_t)((digitalRead(PIN_ENC_CLK) ? 2 : 0) | (digitalRead(PIN_ENC_DT) ? 1 : 0));
}

inline bool isRest(uint8_t state) {
    return ENC_FULL_STEP_DETENT ? state == 0b11 : (state == 0b11 || state == 0b00);
}

void onEncoderChange() {
    uint8_t now = readEncoder();
    encSteps += STEP_TABLE[(encState << 2) | now];
    encState = now;
    if (isRest(now)) {
        if (encSteps >= 2) {
            encDetents += encDirection;
        } else if (encSteps <= -2) {
            encDetents -= encDirection;
        }
        encSteps = 0;
    }
}

bool swStable = false;      // debounced: true = pressed
bool swRaw = false;
uint32_t swRawSince = 0;
uint32_t swPressedAt = 0;
bool longFired = false;

inline bool readPressed() {
    bool level = digitalRead(PIN_ENC_SW);
    return ENC_SW_ACTIVE_LOW ? !level : level;
}

}  // namespace

namespace input {

void begin() {
    // The module has its own pull-ups. The chip's pull-ups are added as well, so that the
    // lines never float while the module is unplugged (a floating SW would read as presses).
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT, INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);
    delay(2);
    encState = readEncoder();
    swRaw = swStable = readPressed();
    swRawSince = millis();
    longFired = swStable;  // a button held at start-up gives no event until released
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), onEncoderChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), onEncoderChange, CHANGE);
}

int takeDetents() {
    noInterrupts();
    int32_t d = encDetents;
    encDetents = 0;
    interrupts();
    return (int)d;
}

ButtonEvent pollButton(uint32_t now) {
    bool raw = readPressed();
    if (raw != swRaw) {
        swRaw = raw;
        swRawSince = now;
    }
    if (swRaw != swStable && now - swRawSince >= BUTTON_DEBOUNCE_MS) {
        swStable = swRaw;
        if (swStable) {
            swPressedAt = now;
            longFired = false;
            return ButtonEvent::Down;
        }
        if (!longFired) {
            return ButtonEvent::Short;
        }
        longFired = false;
        return ButtonEvent::None;
    }
    if (swStable && !longFired && now - swPressedAt >= holdMs) {
        longFired = true;
        return ButtonEvent::Long;
    }
    return ButtonEvent::None;
}

void setHoldMs(uint32_t ms) { holdMs = ms; }

void setReversed(bool reversed) { encDirection = (int8_t)(reversed ? -ENC_DIRECTION : ENC_DIRECTION); }

bool clkLevel() { return digitalRead(PIN_ENC_CLK); }
bool dtLevel() { return digitalRead(PIN_ENC_DT); }
bool swLevel() { return digitalRead(PIN_ENC_SW); }

}  // namespace input
