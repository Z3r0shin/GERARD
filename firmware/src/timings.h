// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the timings that can be changed from the serial portal.
//
// Each timing has a range, a step and a default (the value the firmware had before it could be
// changed). The values are saved in a file of their own, beside the lifetime record.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace timings {

enum Id : uint8_t {
    BreakEvery,      // running time before a break prompt, minutes
    ReminderEvery,   // the break tune repeats this often, seconds
    DimAfter,        // the screen dims this long after the last turn or press, minutes
    Idle1,           // "IDLE?", minutes
    Idle2,           // "IDLE?!", minutes
    Idle3,           // "IDLE?!?!", minutes
    OffAfter,        // the screen goes off, minutes
    CountShows,      // the count shows this long before the run view, seconds
    CountdownStart,  // the countdown setting's value after start-up and RESET, minutes
    HoldMs,          // a press this long is a hold ("back"), milliseconds
    DoneShows,       // "DONE!" shows this long, seconds
    COUNT
};

struct Info {
    char key;           // the portal key that changes it
    const char *label;
    const char *unit;   // "min", "s" or "ms"
    uint16_t min, max, step, def;
};

const Info &info(Id id);

// The values in use, in their own unit.
uint16_t get(Id id);
// The value in milliseconds.
uint32_t ms(Id id);

// Checks a new value: its range, its step, and the idle times' order (each later than the one before).
// Returns nullptr if it can be used, or what is wrong.
const char *check(Id id, long value);

// Sets a value already checked. Returns false if it was not checked first (out of range).
bool set(Id id, uint16_t value);
void setAll(const uint16_t *values);  // values in Id order, each checked; a bad set gives the defaults
void resetAll();
const uint16_t *all();

}  // namespace timings
