// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the timings that can be changed from the serial portal.

#include "timings.h"

namespace timings {
namespace {

// The defaults are the firmware's values before they could be changed.
const Info INFO[COUNT] = {
    {'a', "Break every", "min", 10, 90, 5, 30},
    {'c', "Break tune repeats every", "s", 30, 600, 30, 60},
    {'d', "Screen dims after", "min", 1, 30, 1, 2},
    {'e', "\"IDLE?\" after", "min", 1, 120, 1, 3},
    {'f', "\"IDLE?!\" after", "min", 1, 120, 1, 10},
    {'g', "\"IDLE?!?!\" after", "min", 1, 120, 1, 30},
    {'h', "Screen off after", "min", 1, 120, 1, 35},
    {'i', "Count shows for", "s", 2, 30, 1, 5},
    {'j', "Countdown starts at", "min", 1, 240, 1, 30},
    {'k', "Hold for \"back\"", "ms", 400, 2000, 100, 700},
    {'l', "\"DONE!\" shows for", "s", 10, 300, 10, 60},
};

uint16_t values[COUNT];
bool started = false;

void defaults() {
    for (uint8_t i = 0; i < COUNT; i++) {
        values[i] = INFO[i].def;
    }
    started = true;
}

void ensure() {
    if (!started) {
        defaults();
    }
}

bool inRange(Id id, long v) {
    const Info &in = INFO[id];
    return v >= in.min && v <= in.max && (v - in.min) % in.step == 0;
}

// The idle times in order: "IDLE?" < "IDLE?!" < "IDLE?!?!" < screen off.
bool idleOrderOk(const uint16_t *v) { return v[Idle1] < v[Idle2] && v[Idle2] < v[Idle3] && v[Idle3] < v[OffAfter]; }

}  // namespace

const Info &info(Id id) { return INFO[id < COUNT ? id : 0]; }

uint16_t get(Id id) {
    ensure();
    return values[id < COUNT ? id : 0];
}

uint32_t ms(Id id) {
    uint32_t v = get(id);
    const char *u = info(id).unit;
    if (u[0] == 'm' && u[1] == 'i') return v * 60000UL;  // min
    if (u[0] == 's') return v * 1000UL;                   // s
    return v;                                             // ms
}

const char *check(Id id, long value) {
    ensure();
    if (id >= COUNT) {
        return "no such timing";
    }
    if (!inRange(id, value)) {
        return "out of its range or not on its step";
    }
    uint16_t v[COUNT];
    for (uint8_t i = 0; i < COUNT; i++) {
        v[i] = values[i];
    }
    v[id] = (uint16_t)value;
    if (!idleOrderOk(v)) {
        return "the idle times must each be later than the one before: IDLE?, IDLE?!, IDLE?!?!, then off";
    }
    return nullptr;
}

bool set(Id id, uint16_t value) {
    if (check(id, value)) {
        return false;
    }
    values[id] = value;
    return true;
}

void setAll(const uint16_t *v) {
    for (uint8_t i = 0; i < COUNT; i++) {
        if (!inRange((Id)i, v[i])) {
            defaults();
            return;
        }
    }
    if (!idleOrderOk(v)) {
        defaults();
        return;
    }
    for (uint8_t i = 0; i < COUNT; i++) {
        values[i] = v[i];
    }
    started = true;
}

void resetAll() { defaults(); }

const uint16_t *all() {
    ensure();
    return values;
}

}  // namespace timings
