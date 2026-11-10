// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the run views, the animations shown in place of the count during a run.
//
// Nine views, ported from the owner's mockup (gerard-run-views.html, 2026-11-06). Each draws only
// in rows 16 to 63, into its own buffer laid out as the display's pages 2 to 7; the display
// copies it over the count.
#pragma once

#include <stdint.h>

namespace views {

constexpr uint8_t COUNT = 9;  // A hourglass, B plant, E coffee pot, F train, I scarf, J robot,
                              // K fishing, N Newton's cradle, O heartbeat

struct Input {
    double p;          // progress, 0 to 1: focus run, time since the last break / 30 min;
                       // countdown run, elapsed / length
    double posMs;      // focus run: time since the last break; countdown run: elapsed time
    double lenMs;      // focus run: 30 min; countdown run: its length
    uint32_t breaks;   // breaks answered this session (the plant's flowers)
};

void begin();

// Draws view `index` for the time now (ms) into the view buffer. Call at 10 frames a second while
// the view shows; a view that keeps state (fishing, heartbeat) waits while it is not drawn.
void render(uint8_t index, const Input &in, uint32_t nowMs);

// The view buffer: 6 pages of 128 bytes, rows 16 to 63, each byte 8 rows, top row in bit 0.
const uint8_t *buffer();

char letter(uint8_t index);
const char *name(uint8_t index);
uint32_t lastRenderUs();  // time the last render took
uint32_t maxRenderUs();   // longest render since start-up

}  // namespace views
