// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: lifetime stats and settings, kept in the board's flash.
//
// The record is one small binary file on LittleFS, which spreads its writes over the whole
// file-system area (64 KB) so that no flash sector wears out early. A save writes a new file
// and then renames it over the old one, so a power cut during a save leaves the old record.
// Each record carries a magic number, a layout version and a CRC-32.
// A save is read back before it replaces the old record. A record found damaged at start-up
// is reported (storage::loaded()), never passed over in silence.
// Uploading new firmware does not touch the file-system area: the PlatformIO platform's normal
// upload loads the program only (its builder offsets to the file system for "uploadfs" alone).

#include "storage.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>

namespace {

constexpr const char *PATH = "/gerard.bin";
constexpr const char *TMP_PATH = "/gerard.tmp";
constexpr uint32_t MAGIC = 0x47455241;  // "GERA"
constexpr uint16_t LAYOUT = 1;

struct __attribute__((packed)) Record {
    uint32_t magic;
    uint16_t layout;
    uint16_t viewsOff;            // was reserved (0) in records saved before 2026-11-07
    Lifetime life;
    uint8_t soundOn;
    uint8_t knobReversed;         // was padding (0) in records saved before 2026-11-05
    uint8_t displaySequentialRows;  // same (written as "standard rows" by build 83ef6e8 only)
    uint8_t fonts;                // same: big font in the high 4 bits, small font in the low 4
    uint32_t crc;
};

bool mounted = false;
bool lastOk = true;
uint32_t saves = 0;
storage::Loaded loadState = storage::Loaded::Fresh;

// Writes a temporary file, reads it back and compares (close() reports no error), and only then
// renames it over the real one. Data of 256 bytes at most.
bool writeVerified(const char *path, const uint8_t *data, size_t len) {
    static uint8_t back[256];
    if (len > sizeof(back)) {
        return false;
    }
    File f = LittleFS.open(TMP_PATH, "w");
    bool ok = f && f.write(data, len) == len;
    if (f) {
        f.close();
    }
    if (ok) {
        File g = LittleFS.open(TMP_PATH, "r");
        ok = g && g.size() == len && (size_t)g.read(back, len) == len && memcmp(back, data, len) == 0;
        if (g) {
            g.close();
        }
    }
    return ok && LittleFS.rename(TMP_PATH, path);
}

uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

}  // namespace

namespace storage {

bool begin(Lifetime &life, Settings &settings) {
    memset(&life, 0, sizeof(life));
    memset(&settings, 0, sizeof(settings));  // a fresh board starts silent, every setting at its default
    mounted = LittleFS.begin();
    if (!mounted) {
        return false;
    }
    File f = LittleFS.open(PATH, "r");
    if (!f) {
        return true;
    }
    Record r;
    size_t n = f.read((uint8_t *)&r, sizeof(r));
    f.close();
    if (n != sizeof(r) || r.magic != MAGIC || r.layout != LAYOUT ||
        r.crc != crc32((const uint8_t *)&r, offsetof(Record, crc))) {
        loadState = Loaded::Damaged;
        return true;
    }
    loadState = Loaded::Ok;
    life = r.life;
    settings.soundOn = r.soundOn != 0;
    settings.knobReversed = r.knobReversed != 0;
    settings.displaySequentialRows = r.displaySequentialRows != 0;
    settings.bigFont = r.fonts >> 4;  // the display checks the range
    settings.smallFont = r.fonts & 0x0F;
    settings.viewsOff = r.viewsOff;  // the app checks the range
    return true;
}

bool save(const Lifetime &life, const Settings &settings) {
    if (!mounted) {
        lastOk = false;
        return false;
    }
    Record r;
    memset(&r, 0, sizeof(r));
    r.magic = MAGIC;
    r.layout = LAYOUT;
    r.viewsOff = settings.viewsOff;
    r.life = life;
    r.soundOn = settings.soundOn ? 1 : 0;
    r.knobReversed = settings.knobReversed ? 1 : 0;
    r.displaySequentialRows = settings.displaySequentialRows ? 1 : 0;
    r.fonts = (uint8_t)(((settings.bigFont & 0x0F) << 4) | (settings.smallFont & 0x0F));
    r.crc = crc32((const uint8_t *)&r, offsetof(Record, crc));
    bool ok = writeVerified(PATH, (const uint8_t *)&r, sizeof(r));
    lastOk = ok;
    if (ok) {
        saves++;
    }
    return ok;
}

bool loadTune(const char *name, char *out, size_t outLen) {
    char path[32];
    snprintf(path, sizeof(path), "/tune-%s.txt", name);
    if (!mounted || !LittleFS.exists(path)) {
        return false;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        return false;
    }
    size_t n = f.read((uint8_t *)out, outLen - 1);
    f.close();
    out[n] = '\0';
    return n > 0;
}

bool saveTune(const char *name, const char *rtttl) {
    char path[32];
    snprintf(path, sizeof(path), "/tune-%s.txt", name);
    // A failed tune save is reported on the tune's page; it does not touch lastOk, which is the
    // lifetime record's health (the "!" on the screen).
    return mounted && writeVerified(path, (const uint8_t *)rtttl, strlen(rtttl));
}

bool deleteTune(const char *name) {
    char path[32];
    snprintf(path, sizeof(path), "/tune-%s.txt", name);
    if (!mounted) {
        return false;
    }
    return !LittleFS.exists(path) || LittleFS.remove(path);
}

bool ok() { return mounted; }
bool lastSaveOk() { return lastOk; }
uint32_t saveCount() { return saves; }
Loaded loaded() { return loadState; }
bool healthy() { return mounted && lastOk && loadState != Loaded::Damaged; }

}  // namespace storage
