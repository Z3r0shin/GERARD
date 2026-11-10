// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the passive buzzer, its tunes and its clicks.
//
// The buzzer is a passive magnetic one, switched by an NPN transistor from BUZZER (GP8).
// A tone is a 50 % square wave made by the RP2040's PIO (arduino-pico's tone()).
// Between sounds the pin is held low, so the transistor and the buzzer carry no current.
//
// Tunes are standard RTTTL texts (the Nokia ringtone format), editable from the serial portal:
// octaves 4 to 7 with C4 = 262 Hz; a whole note lasts 240000 / bpm ms; a dot, after the note or
// after the octave, adds half; two equal notes in a row are parted by 10 ms of silence.
// The defaults sit between about 1.5 kHz and 2.7 kHz: the same kind of buzzer on the owner's
// other build is loudest at 2200 Hz.
// Nothing here blocks: update() moves from note to note.

#include "sound.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "pins.h"

namespace {

constexpr uint16_t CLICK_HZ = 2200;
constexpr uint16_t REPEAT_GAP_MS = 10;

// Octave 4, from C to B, in hertz (equal temperament, A4 = 440 Hz, rounded).
constexpr uint16_t OCTAVE4[12] = {262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494};

struct TuneInfo {
    const char *name;
    const char *rtttl;  // built-in default
};

constexpr TuneInfo TUNES[] = {
    {"boot", "boot:d=16,o=6,b=250:g,c7,8e7"},
    {"wake", "wake:d=16,o=7,b=250:c,8e"},
    {"pause", "pause:d=16,o=7,b=250:e,32p,8c"},
    {"resume", "resume:d=16,o=7,b=250:c,32p,8e"},
    {"break", "break:d=8,o=6,b=250:a,32p,a,32p,4d7"},
    {"done", "done:d=8,o=6,b=250:g,c7,e7,32p,c7,4.e7"},
    {"poof", "poof:d=16,o=6,b=250:e7,c7,8g"},
    {"sleep", "sleep:d=8,o=6,b=250:e7,c7,b,4g"},
};
static_assert(sizeof(TUNES) / sizeof(TUNES[0]) == (size_t)Tune::Count_, "one entry per tune");

char texts[(size_t)Tune::Count_][sound::RTTTL_MAX + 1];

constexpr Note TICK[] = {{CLICK_HZ, 4}, {0, 0}};
constexpr Note CLICK[] = {{CLICK_HZ, 14}, {0, 0}};

bool soundOn = false;
const Note *current = nullptr;  // the melody playing, or nullptr
size_t noteIndex = 0;
uint32_t noteEnd = 0;
bool currentIsTune = false;     // a tune is not cut short by a tick or a click
Note playBuf[sound::TUNE_NOTES_MAX + 1];
Note testNote[2] = {{0, 0}, {0, 0}};

void fail(char *err, size_t errLen, const char *s, const char *at, const char *what) {
    if (err && errLen) {
        snprintf(err, errLen, "at character %u: %s", (unsigned)(at - s + 1), what);
    }
}

const char *skipSpaces(const char *p) {
    while (*p == ' ') {
        p++;
    }
    return p;
}

// Reads a whole number; returns false if there is none.
bool readNumber(const char *&p, long &v) {
    if (!isdigit((unsigned char)*p)) {
        return false;
    }
    v = 0;
    while (isdigit((unsigned char)*p) && v < 100000) {
        v = v * 10 + (*p - '0');
        p++;
    }
    return true;
}

bool validDuration(long d) { return d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32; }

// Parses an RTTTL text into notes ending with {0, 0}. out may be null to check only.
bool parse(const char *s, Note *out, char *err, size_t errLen) {
    if (strlen(s) > sound::RTTTL_MAX) {
        if (err && errLen) {
            snprintf(err, errLen, "longer than %u characters", (unsigned)sound::RTTTL_MAX);
        }
        return false;
    }
    const char *p = strchr(s, ':');
    if (!p) {
        fail(err, errLen, s, s + strlen(s), "no ':' after the name");
        return false;
    }
    p++;
    long defDur = 4, defOct = 6, bpm = 63;  // the RTTTL defaults
    // Defaults: d=, o=, b=, separated by commas, up to the second ':'.
    while (true) {
        p = skipSpaces(p);
        if (*p == ':') {
            p++;
            break;
        }
        char key = (char)tolower((unsigned char)*p);
        if ((key != 'd' && key != 'o' && key != 'b') || p[1] != '=') {
            fail(err, errLen, s, p, "expected d=, o=, b= or ':'");
            return false;
        }
        const char *at = p;
        p += 2;
        long v;
        if (!readNumber(p, v)) {
            fail(err, errLen, s, p, "expected a number");
            return false;
        }
        if (key == 'd' && !validDuration(v)) {
            fail(err, errLen, s, at, "d must be 1, 2, 4, 8, 16 or 32");
            return false;
        }
        if (key == 'o' && (v < 4 || v > 7)) {
            fail(err, errLen, s, at, "o must be 4 to 7");
            return false;
        }
        if (key == 'b' && (v < 10 || v > 900)) {
            fail(err, errLen, s, at, "b must be 10 to 900");
            return false;
        }
        (key == 'd' ? defDur : key == 'o' ? defOct : bpm) = v;
        p = skipSpaces(p);
        if (*p == ',') {
            p++;
        } else if (*p != ':') {
            fail(err, errLen, s, p, "expected ',' or ':'");
            return false;
        }
    }
    uint32_t whole = 240000UL / (uint32_t)bpm;
    size_t n = 0;
    uint32_t total = 0;
    uint16_t lastHz = 0;
    while (true) {
        p = skipSpaces(p);
        if (*p == '\0') {
            break;
        }
        const char *at = p;
        long dur = defDur;
        if (isdigit((unsigned char)*p)) {
            readNumber(p, dur);
            if (!validDuration(dur)) {
                fail(err, errLen, s, at, "a note length must be 1, 2, 4, 8, 16 or 32");
                return false;
            }
        }
        char letter = (char)tolower((unsigned char)*p);
        static const int8_t SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};  // a b c d e f g
        int semitone = -1;
        if (letter >= 'a' && letter <= 'g') {
            semitone = SEMITONE[letter - 'a'];
        } else if (letter == 'h') {  // some RTTTL texts write B as H
            semitone = 11;
        } else if (letter != 'p') {
            fail(err, errLen, s, p, "expected a note: a to g, or p for a rest");
            return false;
        }
        p++;
        if (*p == '#') {
            if (semitone < 0) {
                fail(err, errLen, s, p, "a rest cannot be sharp");
                return false;
            }
            semitone++;
            p++;
        }
        bool dotted = false;
        if (*p == '.') {
            dotted = true;
            p++;
        }
        long oct = defOct;
        if (isdigit((unsigned char)*p)) {
            oct = *p - '0';
            p++;
            if (oct < 4 || oct > 7) {
                fail(err, errLen, s, p - 1, "an octave must be 4 to 7");
                return false;
            }
        }
        if (*p == '.') {
            dotted = true;
            p++;
        }
        uint16_t hz = 0;
        if (semitone >= 0) {
            if (semitone == 12) {  // b# is the next C
                if (oct == 7) {
                    fail(err, errLen, s, at, "b# in octave 7 is above octave 7");
                    return false;
                }
                semitone = 0;
                oct++;
            }
            hz = (uint16_t)(OCTAVE4[semitone] << (oct - 4));
        }
        uint32_t ms = whole / (uint32_t)dur;
        if (dotted) {
            ms += ms / 2;
        }
        if (ms < 1) {
            ms = 1;
        }
        bool gap = hz && hz == lastHz && ms > REPEAT_GAP_MS;
        if (gap) {
            if (n + 1 > sound::TUNE_NOTES_MAX) {
                fail(err, errLen, s, at, "too many notes (64 at most)");
                return false;
            }
            if (out) {
                out[n] = {0, REPEAT_GAP_MS};
            }
            n++;
            ms -= REPEAT_GAP_MS;
        }
        if (n + 1 > sound::TUNE_NOTES_MAX) {
            fail(err, errLen, s, at, "too many notes (64 at most)");
            return false;
        }
        total += ms + (gap ? REPEAT_GAP_MS : 0);
        if (total > sound::TUNE_MS_MAX) {
            fail(err, errLen, s, at, "the tune would last over 10 s");
            return false;
        }
        if (out) {
            out[n] = {hz, (uint16_t)ms};
        }
        n++;
        lastHz = hz;
        p = skipSpaces(p);
        if (*p == ',') {
            p++;
        } else if (*p != '\0') {
            fail(err, errLen, s, p, "expected ',' between notes");
            return false;
        }
    }
    if (n == 0) {
        fail(err, errLen, s, p, "no notes");
        return false;
    }
    if (out) {
        out[n] = {0, 0};
    }
    return true;
}

void silence() {
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
}

void startNote(uint32_t now) {
    const Note &n = current[noteIndex];
    if (n.hz) {
        tone(PIN_BUZZER, n.hz);
    } else {
        silence();
    }
    noteEnd = now + n.ms;
}

void start(const Note *notes, bool isTune) {
    current = notes;
    currentIsTune = isTune;
    noteIndex = 0;
    startNote(millis());
}

bool startRtttl(const char *rtttl) {
    // Checked first, so a text that fails cannot overwrite the notes of a tune still playing.
    if (!parse(rtttl, nullptr, nullptr, 0)) {
        return false;
    }
    parse(rtttl, playBuf, nullptr, 0);
    start(playBuf, true);
    return true;
}

}  // namespace

namespace sound {

void begin() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    for (size_t i = 0; i < (size_t)Tune::Count_; i++) {
        snprintf(texts[i], sizeof(texts[i]), "%s", TUNES[i].rtttl);
    }
}

void update(uint32_t now) {
    if (!current) {
        return;
    }
    if ((int32_t)(now - noteEnd) < 0) {
        return;
    }
    noteIndex++;
    if (current[noteIndex].ms == 0) {
        current = nullptr;
        silence();
        return;
    }
    startNote(now);
}

void play(Tune tune) {
    if (soundOn) {
        playTest(tune);
    }
}

void playTest(Tune tune) {
    if ((size_t)tune < (size_t)Tune::Count_) {
        startRtttl(texts[(size_t)tune]);
    }
}

bool playRtttlTest(const char *rtttl) { return startRtttl(rtttl); }

void tick() {
    if (soundOn && !(current && currentIsTune)) {
        start(TICK, false);
    }
}

void click() {
    if (soundOn && !(current && currentIsTune)) {
        start(CLICK, false);
    }
}

void toneTest(uint16_t hz, uint16_t ms) {
    testNote[0] = {hz, ms};
    testNote[1] = {0, 0};
    start(testNote, true);
}

void stop() {
    current = nullptr;
    silence();
}

bool busy() { return current != nullptr; }

void setEnabled(bool on) {
    soundOn = on;
    if (!on) {
        stop();
    }
}

bool enabled() { return soundOn; }

const char *tuneName(Tune tune) {
    return (size_t)tune < (size_t)Tune::Count_ ? TUNES[(size_t)tune].name : "?";
}

bool tuneFromName(const char *name, Tune &out) {
    for (size_t i = 0; i < (size_t)Tune::Count_; i++) {
        if (strcmp(name, TUNES[i].name) == 0) {
            out = (Tune)i;
            return true;
        }
    }
    return false;
}

const char *tuneText(Tune tune) { return texts[(size_t)tune]; }
const char *defaultTuneText(Tune tune) { return TUNES[(size_t)tune].rtttl; }
bool tuneIsDefault(Tune tune) { return strcmp(texts[(size_t)tune], TUNES[(size_t)tune].rtttl) == 0; }

bool checkRtttl(const char *rtttl, char *err, size_t errLen) { return parse(rtttl, nullptr, err, errLen); }

bool setTuneText(Tune tune, const char *rtttl) {
    if ((size_t)tune >= (size_t)Tune::Count_ || !parse(rtttl, nullptr, nullptr, 0)) {
        return false;
    }
    // The text is shown on the portal as it is: only printable characters, so a damaged tune file
    // can never send control codes to the terminal. The portal itself accepts no others.
    for (const char *c = rtttl; *c; c++) {
        if (*c < 32 || *c > 126) {
            return false;
        }
    }
    // A tune playing keeps its notes: they were parsed into playBuf when it started.
    snprintf(texts[(size_t)tune], sizeof(texts[0]), "%s", rtttl);
    return true;
}

void resetTuneText(Tune tune) { setTuneText(tune, TUNES[(size_t)tune].rtttl); }

}  // namespace sound
