// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the passive buzzer, its tunes and its clicks.
//
// Each tune is an RTTTL string ("name:d=4,o=6,b=120:c,8e,g"), editable from the serial portal
// and kept in flash. The tick and the click are fixed.
#pragma once

#include <stddef.h>
#include <stdint.h>

struct Note {
    uint16_t hz;  // 0 = silence
    uint16_t ms;
};

enum class Tune : uint8_t {
    Boot,
    Wake,
    Pause,
    Resume,
    Break,
    Done,
    Poof,
    Sleep,
    Count_,
};

namespace sound {

constexpr size_t RTTTL_MAX = 200;      // characters in one tune
constexpr size_t TUNE_NOTES_MAX = 64;  // notes and rests in one tune
constexpr uint32_t TUNE_MS_MAX = 10000;  // longest tune

void begin();
void update(uint32_t now);

// Sounds for the device's own use. They are silent while sound is off.
void play(Tune tune);
void tick();   // one detent of the knob
void click();  // the button going down

// Sounds asked for from the serial portal. They play even while sound is off.
void playTest(Tune tune);
void toneTest(uint16_t hz, uint16_t ms);
bool playRtttlTest(const char *rtttl);  // false if it does not parse

void stop();
bool busy();

void setEnabled(bool on);
bool enabled();

const char *tuneName(Tune tune);
bool tuneFromName(const char *name, Tune &out);

// The tune's RTTTL text: the one in use, and the built-in default.
const char *tuneText(Tune tune);
const char *defaultTuneText(Tune tune);
bool tuneIsDefault(Tune tune);

// Checks an RTTTL text. On failure, writes a short reason into err (may be null) and returns false.
bool checkRtttl(const char *rtttl, char *err, size_t errLen);

// Uses this text for the tune. Returns false (and changes nothing) if it does not parse.
bool setTuneText(Tune tune, const char *rtttl);
void resetTuneText(Tune tune);

}  // namespace sound
