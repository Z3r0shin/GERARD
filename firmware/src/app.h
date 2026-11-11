// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: what the device does. Screens, timers, breaks and stats.
#pragma once

#include <stdint.h>

#include "sound.h"
#include "storage.h"

namespace app {

void begin(uint32_t now, bool watchdogReset);
void update(uint32_t now);

// For the serial portal.
const char *screenName();
uint8_t runViewIndex();        // the view of the run on or paused, else of the next run (views.h)
const char *runViewState();    // "showing now", "count showing", "run paused" or "for the next run"
const char *screenLevel();     // "full", "dimmed" (no turn or press for 2 min) or "off"
uint32_t sessionBreakCount();  // breaks answered this session
uint64_t sessionFocusMs();
uint64_t runMs();
uint32_t sinceBreakMs();
const Lifetime &lifetime();
bool soundOn();
void setSound(bool on);
// Settings, applied at once and saved.
const Settings &currentSettings();
void setKnobReversed(bool reversed);
void setDisplaySequentialRows(bool sequential);
void setBigFont(uint8_t index);
void setSmallFont(uint8_t index);
// Run views switched on or off (views.h). False when the change would leave none on.
bool setViewOn(uint8_t index, bool on);
void setAllViewsOn();
// Timings (timings.h): set one already checked with timings::check, or all back to their defaults;
// each is used at once and saved. False if the value was refused or the save failed.
bool setTiming(uint8_t id, uint16_t value);
bool resetTimings();

void clearLifetime();
void saveNow();

// Tunes edited from the portal.
uint32_t unreadableTuneMask();                 // bit n: tune n's saved text failed at start-up
bool keepTune(Tune tune, const char *rtttl);   // use it and save it; false if it fails to parse or save
bool restoreTune(Tune tune);                   // back to the built-in tune; false if the file stays

}  // namespace app
