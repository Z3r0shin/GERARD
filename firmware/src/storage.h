// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: lifetime stats and settings, kept in the board's flash.
#pragma once

#include <stddef.h>
#include <stdint.h>

struct Lifetime {
    uint64_t focusMs;          // focus time, counting up and counting down
    uint32_t runs;             // focus runs and countdowns started
    uint32_t longestRunMs;     // longest single run without a stop
    uint32_t countdownsDone;   // countdowns that reached zero
    uint32_t breaksPrompted;   // break prompts shown
    uint32_t breaksTaken;      // break prompts answered with the button ("U GOOD?")
    uint32_t boots;            // start-ups
    uint32_t watchdogResets;   // start-ups caused by the watchdog
};

// Every setting is 0 or false by default, so a record saved before a setting existed reads as
// that setting's default.
struct Settings {
    bool soundOn;
    bool knobReversed;         // turn the knob the other way round
    bool displaySequentialRows;  // the sequential COM row layout (U8g2's ALT0) instead of the standard one
    uint8_t bigFont;           // index into the large-line fonts
    uint8_t smallFont;         // index into the top-band fonts
    uint16_t viewsOff;         // run views switched off: bit n is view n (views.h); 0 = all on
};

namespace storage {

// Mounts the file system and loads the saved record. A missing or damaged record gives
// zeroed stats and every setting at its default (sound off). Returns false if the file system cannot be used.
bool begin(Lifetime &life, Settings &settings);

// Writes the record. Returns false on failure.
bool save(const Lifetime &life, const Settings &settings);

// Tunes edited from the serial portal, one file each, by tune name. loadTune returns false when
// there is none (the built-in default is then used). saveTune is read back like save(), but its
// result is only returned: it does not change lastSaveOk() or healthy().
bool loadTune(const char *name, char *out, size_t outLen);
bool saveTune(const char *name, const char *rtttl);
bool deleteTune(const char *name);

// The timings changed from the portal (timings.h), in a file of their own. loadTimings gives the
// saved values to timings::setAll; saveTimings writes them, read back like save(). A failed or
// damaged timings file shows on the portal; it does not change healthy().
enum class TimingsLoaded : uint8_t {
    None,     // none saved: the defaults are in use
    Ok,       // read at start-up with every value usable, or saved since
    Damaged,  // a file was there but could not be used: the defaults are in use
};
TimingsLoaded loadTimings();
bool saveTimings();
TimingsLoaded timingsLoaded();
bool timingsSaveOk();  // the last timings save worked (true before any)

enum class Loaded : uint8_t {
    Fresh,    // no record yet: a new board, or a new file system
    Ok,       // the record was read and its CRC matched
    Damaged,  // a record was there but could not be used; the stats restarted from zero
};

bool ok();             // the file system mounted
bool lastSaveOk();     // the last save worked and read back the same (true before any save)
uint32_t saveCount();  // saves since start-up
Loaded loaded();       // what start-up found

// False if the file system did not mount, the last save failed, or start-up found a damaged
// record. Shown as "!" on the screen.
bool healthy();

}  // namespace storage
