// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the serial portal, over the board's USB port.
//
// Open the board's COM port in a terminal (PuTTY or any other) at 115200 baud, 8 data bits,
// no parity, 1 stop bit. Never open it at 1200 baud: on this board, closing the port at
// 1200 baud restarts it into its USB bootloader.
//
// The portal greets with its main menu as soon as the terminal opens the port. Every page is
// drawn whole, from the top of a cleared screen, and drawn again in place after each key: the
// result of a choice shows on the page's last line instead of scrolling the menu away. Every
// choice is one key; a frequency, a length, a tune or a confirmation is typed on the prompt
// line and ended with Enter (Esc cancels).
//
// Each page is built in memory, then sent a piece at a time: each loop writes only what the USB
// buffer takes at once, so a terminal that reads slowly or not at all never holds the main loop.
// Arrow keys and other escape sequences (Esc [ ... or Esc O x) are recognised and ignored; one
// not finished within 100 ms is dropped. Esc alone means back or cancel: it counts when another
// key follows it, or when nothing follows it for 50 ms.

#include "console.h"

#include <Arduino.h>
#include <ctype.h>
#include <pico/time.h>
#include <pico/unique_id.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "app.h"
#include "config.h"
#include "display.h"
#include "input.h"
#include "sound.h"
#include "storage.h"
#include "timings.h"
#include "views.h"

namespace {

enum class Page : uint8_t { Main, Status, Stats, Buzzer, Tune, Hardware, Watch, Settings, Views, Timings };

enum class Ask : uint8_t {
    None,
    Hz,      // a frequency for one tone
    Ms,      // its length
    Rtttl,   // a new tune
    Erase,   // "yes" to erase the lifetime stats
    Reboot,  // "y" to restart
    Timing,  // a new value for one timing
};

constexpr uint16_t SWEEP_FROM_HZ = 1000;
constexpr uint16_t SWEEP_TO_HZ = 4000;
constexpr uint16_t SWEEP_STEP_HZ = 250;
constexpr uint16_t SWEEP_TONE_MS = 400;
constexpr uint16_t SWEEP_GAP_MS = 150;
constexpr int SWEEP_STEPS = (SWEEP_TO_HZ - SWEEP_FROM_HZ) / SWEEP_STEP_HZ + 1;

Page page = Page::Main;
bool dirty = false;           // the page must be drawn again
bool connected = false;
uint32_t connectedAt = 0;
bool greeted = false;
bool wasWatchdogReset = false;
char message[160] = "";       // the result line at the foot of the page

// Typed input.
Ask ask = Ask::None;
char field[sound::RTTTL_MAX + 1];
size_t fieldLen = 0;
long askedHz = 0;
uint8_t editTiming = 0;  // the timing being typed (Ask::Timing)

// Buzzer page.
bool pickingTune = false;     // "e" was pressed: the next digit chooses the tune to edit
bool sweeping = false;
uint16_t sweepHz = 0;
uint32_t sweepNextAt = 0;

// Tune page.
Tune editTune = Tune::Boot;
char candidate[sound::RTTTL_MAX + 1] = "";  // a new tune typed and tried, not kept yet

// Watch page: the last changes of the encoder lines.
constexpr int WATCH_LINES = 10;
uint8_t watchLast = 0xFF;
uint8_t watchLevels[WATCH_LINES];
uint32_t watchTimes[WATCH_LINES];
int watchCount = 0;

// Output buffer: one page, sent a piece at a time.
char out[3072];
size_t outLen = 0;
size_t sentLen = 0;  // bytes of out already written to the port

// Escape sequences: 0 none, 1 Esc received, 2 inside a sequence (Esc [ or Esc O).
uint8_t escState = 0;
uint32_t escAt = 0;
constexpr uint32_t LONE_ESC_MS = 50;       // Esc with nothing after it for this long is Esc
constexpr uint32_t ESC_SEQUENCE_MS = 100;  // a sequence unfinished this long after its Esc is dropped

bool heavyKey = false;  // a key did slow work (an I2C scan): no more keys this loop

void put(const char *fmt, ...) {
    if (outLen >= sizeof(out) - 1) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + outLen, sizeof(out) - outLen, fmt, ap);
    va_end(ap);
    if (n > 0) {
        outLen += (size_t)n;
        if (outLen > sizeof(out) - 1) {
            outLen = sizeof(out) - 1;
        }
    }
}

bool sending() { return sentLen < outLen; }

// Writes as much of the page as the USB buffer takes now, without waiting.
void pump() {
    if (!connected || sentLen >= outLen) {
        outLen = sentLen = 0;
        return;
    }
    int room = Serial.availableForWrite();
    if (room <= 0) {
        return;
    }
    size_t n = outLen - sentLen;
    if (n > (size_t)room) {
        n = (size_t)room;
    }
    sentLen += Serial.write((const uint8_t *)out + sentLen, n);
    if (sentLen >= outLen) {
        outLen = sentLen = 0;
    }
}

void say(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    dirty = true;
}

void putDuration(uint64_t ms) {
    uint64_t s = ms / 1000;
    put("%lluh %02llum %02llus", s / 3600, s / 60 % 60, s % 60);
}

void header(const char *title) {
    outLen = sentLen = 0;  // a page starts from a cleared screen: nothing before it matters
    put("\x1b[2J\x1b[H");
    put("==============================================================================\r\n");
    put(" GERARD serial portal   firmware %s (%s)   %lu baud\r\n", FW_VERSION, FW_SHA, (unsigned long)SERIAL_BAUD);
    put("==============================================================================\r\n");
    if (title) {
        put(" %s\r\n\r\n", title);
    }
}

void footer(const char *keys) {
    if (keys) {
        put("\r\n %s\r\n", keys);
    }
    if (message[0]) {
        put("\r\n >> %s\r\n", message);
    }
    switch (ask) {
        case Ask::Hz: put("\r\n Frequency in Hz, 100 to 10000 (Enter to play, Esc to cancel): %s", field); break;
        case Ask::Ms: put("\r\n Length in ms, 1 to 5000, or Enter alone for 500: %s", field); break;
        case Ask::Rtttl: put("\r\n New tune (Enter to try it, Esc to cancel):\r\n %s", field); break;
        case Ask::Erase: put("\r\n Type yes and press Enter to erase; anything else keeps them: %s", field); break;
        case Ask::Reboot: put("\r\n Restart the board? [y/n] "); break;
        case Ask::Timing: {
            const timings::Info &in = timings::info((timings::Id)editTiming);
            put("\r\n New value for %s, in %s, %u to %u in steps of %u (Enter to set, Esc to cancel): %s", in.label,
                in.unit, (unsigned)in.min, (unsigned)in.max, (unsigned)in.step, field);
            break;
        }
        case Ask::None: break;
    }
}

const char *timingsState();  // below, with the Timings page

void drawMain() {
    header(nullptr);
    put(" Now: %s, session focus ", app::screenName());
    putDuration(app::sessionFocusMs());
    put("\r\n\r\n");
    put(" [1] Status\r\n");
    put(" [2] Lifetime stats\r\n");
    put(" [3] Sound: %s (press 3 to switch it %s)\r\n", app::soundOn() ? "ON" : "OFF", app::soundOn() ? "off" : "on");
    put(" [4] Buzzer and tunes\r\n");
    put(" [5] Hardware check\r\n");
    put(" [6] Erase the lifetime stats\r\n");
    put(" [7] Restart the board\r\n");
    put(" [8] Settings: knob direction, display rows, fonts\r\n");
    put(" [9] Run views: which animations play during a run\r\n");
    put(" [0] Timings: breaks, idle, dimming, countdown, hold\r\n");
    footer("Press a key. Enter draws this page again.");
}

void drawStatus() {
    header("STATUS");
    put(" Screen:                 %s\r\n", app::screenName());
    put(" Session focus:          ");
    putDuration(app::sessionFocusMs());
    put("\r\n Current run:            ");
    putDuration(app::runMs());
    put("\r\n Running since a break:  ");
    putDuration(app::sinceBreakMs());
    put("\r\n Sound:                  %s\r\n", app::soundOn() ? "on" : "off");
    put(" Run view:               %c, %s (%s); breaks answered this session: %lu\r\n",
        views::letter(app::runViewIndex()), views::name(app::runViewIndex()), app::runViewState(),
        (unsigned long)app::sessionBreakCount());
    put(" Screen brightness:      %s\r\n", app::screenLevel());
    put(" Timings:                %s\r\n", timingsState());
    put(" View drawing time:      last frame %lu us, longest since start-up %lu us\r\n",
        (unsigned long)views::lastRenderUs(), (unsigned long)views::maxRenderUs());
    if (display::present()) {
        put(" Display:                found at 0x%02X\r\n", display::address());
    } else {
        put(" Display:                NOT FOUND (looked for at 0x3C and 0x3D)\r\n");
    }
    put(" Storage:                %s, last save %s, %lu saves since start-up\r\n",
        storage::ok() ? "mounted" : "NOT MOUNTED", storage::lastSaveOk() ? "ok" : "FAILED",
        (unsigned long)storage::saveCount());
    const char *rec = !storage::ok()                                ? "not read (storage not mounted)"
                      : storage::loaded() == storage::Loaded::Ok    ? "read, ok"
                      : storage::loaded() == storage::Loaded::Fresh ? "none yet (new board or new storage)"
                                                                    : "DAMAGED: lifetime stats restarted from zero";
    put(" Saved record at start:  %s\r\n", rec);
    put(" Last start-up:          %s\r\n", wasWatchdogReset ? "WATCHDOG RESET" : "normal");
    put(" Up for:                 ");
    putDuration(time_us_64() / 1000);  // 64-bit: millis() wraps after 49.7 days
    put("\r\n");
    footer("[b] back   [Enter] refresh");
}

void drawStats() {
    const Lifetime &l = app::lifetime();
    header("LIFETIME STATS");
    if (storage::loaded() == storage::Loaded::Damaged) {
        put(" !! The saved record was damaged at start-up: these stats restarted from zero.\r\n\r\n");
    }
    put(" Focus time:             ");
    putDuration(l.focusMs);
    put("\r\n Runs started:           %lu\r\n", (unsigned long)l.runs);
    put(" Longest run:            ");
    putDuration(l.longestRunMs);
    put("\r\n Countdowns completed:   %lu\r\n", (unsigned long)l.countdownsDone);
    put(" Break prompts:          %lu\r\n", (unsigned long)l.breaksPrompted);
    put("   answered (U GOOD?):   %lu\r\n", (unsigned long)l.breaksTaken);
    put(" Start-ups:              %lu\r\n", (unsigned long)l.boots);
    put("   by the watchdog:      %lu\r\n", (unsigned long)l.watchdogResets);
    footer("[b] back   [Enter] refresh");
}

void drawBuzzer() {
    header(app::soundOn() ? "BUZZER AND TUNES (sound is on)" : "BUZZER AND TUNES (sound is off: these play anyway)");
    uint32_t bad = app::unreadableTuneMask();
    for (size_t i = 0; i < (size_t)Tune::Count_; i++) {
        Tune t = (Tune)i;
        put(" [%u] %-7s %s %s%s\r\n", (unsigned)(i + 1), sound::tuneName(t), sound::tuneIsDefault(t) ? " " : "*",
            sound::tuneText(t), bad & (1u << i) ? "   (saved tune unreadable: built-in used)" : "");
    }
    put("\r\n     * = your own tune, kept in flash\r\n");
    put("\r\n [e] edit a tune    [t] one tone    [s] sweep %u to %u Hz    [x] stop\r\n", SWEEP_FROM_HZ, SWEEP_TO_HZ);
    if (sweeping) {
        put("\r\n Sweep: %u Hz now (step %d of %d). Note the loudest. Any key stops.\r\n", sweepHz - SWEEP_STEP_HZ,
            (sweepHz - SWEEP_STEP_HZ - SWEEP_FROM_HZ) / SWEEP_STEP_HZ + 1, SWEEP_STEPS);
    }
    footer(pickingTune ? "Which tune to edit? Press 1 to 8 (b cancels)." : "[1]-[8] play a tune   [b] back");
}

void drawTune() {
    char title[48];
    snprintf(title, sizeof(title), "TUNE: %s", sound::tuneName(editTune));
    header(title);
    put(" In use:    %s%s\r\n", sound::tuneText(editTune), sound::tuneIsDefault(editTune) ? "   (built-in)" : "   (yours)");
    put(" Built-in:  %s\r\n", sound::defaultTuneText(editTune));
    if (candidate[0]) {
        put(" New:       %s   (tried, not kept)\r\n", candidate);
    }
    put("\r\n RTTTL: name:d=LENGTH,o=OCTAVE,b=TEMPO:notes\r\n");
    put("   LENGTH 1, 2, 4, 8, 16 or 32 (4 = quarter note); OCTAVE 4 to 7; TEMPO in beats a minute.\r\n");
    put("   A note: [length]letter[#][.][octave], for example 8c, 4e., 16g#7; p is a rest.\r\n");
    put("   At most %u characters, %u notes and %lu s.\r\n", (unsigned)sound::RTTTL_MAX, (unsigned)sound::TUNE_NOTES_MAX,
        (unsigned long)(sound::TUNE_MS_MAX / 1000));
    put("\r\n [p] play the one in use   [n] type a new one%s   [r] back to built-in   [b] back\r\n",
        candidate[0] ? "   [k] keep the new one   [y] play the new one" : "");
    footer(nullptr);
}

void drawHardware() {
    char id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(id, sizeof(id));
    header("HARDWARE CHECK");
    put(" Board ID:   %s\r\n", id);
    if (display::present()) {
        put(" Display:    found at 0x%02X\r\n", display::address());
    } else {
        put(" Display:    NOT FOUND (looked for at 0x3C and 0x3D)\r\n");
    }
    if (display::testPattern()) {
        put("\r\n Display test: pattern %u of %u, %s.\r\n", (unsigned)display::testPattern(),
            (unsigned)display::TEST_PATTERNS, display::testPatternName(display::testPattern()));
        put(" The device's screens come back when the test is stopped.\r\n");
    }
    footer(display::testPattern() ? "[3] next pattern   [0] stop the test   [b] back (stops the test)"
                                  : "[1] scan the display bus (I2C)   [2] watch the encoder and its button   "
                                    "[3] display test   [b] back");
}

void drawWatch() {
    header("ENCODER AND BUTTON, LIVE");
    put(" Turn the knob one click at a time, then press it. 1 = high, 0 = low.\r\n\r\n");
    put(" Now:   CLK %d   DT %d   SW %d\r\n\r\n", input::clkLevel(), input::dtLevel(), input::swLevel());
    put(" Last changes, newest last:\r\n");
    int first = watchCount > WATCH_LINES ? watchCount - WATCH_LINES : 0;
    for (int i = first; i < watchCount; i++) {
        uint8_t v = watchLevels[i % WATCH_LINES];
        put("   %8lu ms   CLK %d   DT %d   SW %d\r\n", (unsigned long)watchTimes[i % WATCH_LINES], (v >> 2) & 1,
            (v >> 1) & 1, v & 1);
    }
    footer("Any key goes back.");
}

void drawSettings() {
    const Settings &s = app::currentSettings();
    header("SETTINGS (each change shows at once on the device and is saved)");
    put(" [1] Knob direction:   %s\r\n", s.knobReversed ? "reversed" : "normal");
    put(" [2] Display rows:     %s\r\n", s.displaySequentialRows ? "sequential (ALT0)" : "standard");
    put(" [3] Large-line font:  %s   (%u of %u)\r\n", display::bigFontName(display::bigFont()),
        (unsigned)display::bigFont() + 1, (unsigned)display::bigFontCount());
    put(" [4] Top-line font:    %s   (%u of %u)\r\n", display::smallFontName(display::smallFont()),
        (unsigned)display::smallFont() + 1, (unsigned)display::smallFontCount());
    put("\r\n Keys 1 and 2 switch the setting; keys 3 and 4 move to the next font.\r\n");
    put(" If the display's text is scrambled, press 2. Standard suits the rev A display.\r\n");
    if (!display::present()) {
        put(" The display is NOT FOUND: the font and row settings show once it answers.\r\n");
    }
    footer("[b] back");
}

void drawViews() {
    const Settings &s = app::currentSettings();
    header("RUN VIEWS (each change is saved)");
    put(" During a run, a view shows in place of the count. A new one, at random among those ON, at each run\r\n");
    put(" start and after each break prompt; never the same twice in a row while two or more are ON.\r\n\r\n");
    for (uint8_t i = 0; i < views::COUNT; i++) {
        put(" [%u] %c, %-16s %s\r\n", (unsigned)i + 1, views::letter(i), views::name(i),
            (s.viewsOff >> i) & 1 ? "off" : "ON");
    }
    put("\r\n Run view: %c, %s (%s)\r\n", views::letter(app::runViewIndex()), views::name(app::runViewIndex()),
        app::runViewState());
    put("\r\n Keys 1 to %u switch a view on or off; 0 switches them all on. One view at least stays on.\r\n",
        (unsigned)views::COUNT);
    footer("[b] back");
}

const char *timingsState() {
    if (!storage::timingsSaveOk()) return "the last save FAILED";
    switch (storage::timingsLoaded()) {
        case storage::TimingsLoaded::Ok: return "saved";
        case storage::TimingsLoaded::Damaged: return "saved file DAMAGED at start-up: defaults in use until a change is saved";
        default: return "none saved at start-up: defaults";
    }
}

void drawTimings() {
    header("TIMINGS (each change is used at once and saved)");
    for (uint8_t i = 0; i < timings::COUNT; i++) {
        const timings::Info &in = timings::info((timings::Id)i);
        uint16_t v = timings::get((timings::Id)i);
        put(" [%c] %-26s %4u %-3s  (%u to %u, steps of %u%s)\r\n", in.key, in.label, (unsigned)v, in.unit,
            (unsigned)in.min, (unsigned)in.max, (unsigned)in.step, v == in.def ? "" : "; changed");
    }
    put("\r\n The idle times go in order: IDLE?, then IDLE?!, then IDLE?!?!, then the screen off.\r\n");
    put(" A pause or a stop never counts as a break; the break prompt never dims and never goes off.\r\n");
    put(" Countdown starts at: the countdown setting's value after a start-up or RESET.\r\n");
    put(" Saved timings: %s.\r\n", timingsState());
    footer("A letter changes that timing.   [r] every timing back to its default   [b] back");
}

void draw() {
    switch (page) {
        case Page::Main: drawMain(); break;
        case Page::Status: drawStatus(); break;
        case Page::Stats: drawStats(); break;
        case Page::Buzzer: drawBuzzer(); break;
        case Page::Tune: drawTune(); break;
        case Page::Hardware: drawHardware(); break;
        case Page::Watch: drawWatch(); break;
        case Page::Settings: drawSettings(); break;
        case Page::Views: drawViews(); break;
        case Page::Timings: drawTimings(); break;
    }
    dirty = false;
}

void go(Page p) {
    page = p;
    message[0] = '\0';
    pickingTune = false;
    dirty = true;
}

void startAsk(Ask a) {
    ask = a;
    fieldLen = 0;
    field[0] = '\0';
    dirty = true;
}

void stopSweep() {
    if (sweeping) {
        sweeping = false;
        sound::stop();
    }
}

// Parses a whole decimal number within [lo, hi].
bool parseNumber(const char *s, long lo, long hi, long &v) {
    if (!*s) {
        return false;
    }
    char *end = nullptr;
    long n = strtol(s, &end, 10);
    if (*end != '\0' || n < lo || n > hi) {
        return false;
    }
    v = n;
    return true;
}

// The typed line was ended with Enter.
void submit() {
    Ask a = ask;
    ask = Ask::None;
    dirty = true;
    switch (a) {
        case Ask::Hz:
            if (parseNumber(field, 100, 10000, askedHz)) {
                startAsk(Ask::Ms);
            } else {
                say("\"%s\" is not a frequency from 100 to 10000 Hz.", field);
            }
            break;
        case Ask::Ms: {
            long ms = 500;
            if (fieldLen == 0 || parseNumber(field, 1, 5000, ms)) {
                sound::toneTest((uint16_t)askedHz, (uint16_t)ms);
                say("Tone: %ld Hz for %ld ms.", askedHz, ms);
            } else {
                say("\"%s\" is not a length from 1 to 5000 ms.", field);
            }
            break;
        }
        case Ask::Rtttl: {
            char err[64];
            if (!sound::checkRtttl(field, err, sizeof(err))) {
                say("Not a tune: %s. Nothing changed.", err);
            } else {
                snprintf(candidate, sizeof(candidate), "%s", field);
                sound::playRtttlTest(candidate);
                say("Playing the new tune. [k] keeps it, [y] plays it again, [n] types another.");
            }
            break;
        }
        case Ask::Timing: {
            const timings::Info &in = timings::info((timings::Id)editTiming);
            long v = 0;
            const char *why = nullptr;
            if (!parseNumber(field, 0, 65535, v)) {
                why = "not a whole number";
            } else {
                why = timings::check((timings::Id)editTiming, v);
            }
            if (why) {
                say("\"%s\" not used for %s: %s. Nothing changed.", field, in.label, why);
            } else if (app::setTiming(editTiming, (uint16_t)v)) {
                say("%s: %ld %s.", in.label, v, in.unit);
            } else {
                say("%s: %ld %s, in use, but the save FAILED.", in.label, v, in.unit);
            }
            break;
        }
        case Ask::Erase:
            if (strcasecmp(field, "yes") == 0) {
                app::clearLifetime();
                say("Lifetime stats erased.");
            } else {
                say("Lifetime stats kept.");
            }
            break;
        default:
            break;
    }
}

// A key while a typed line is open.
void onAskKey(int ch) {
    if (ask == Ask::Reboot) {
        ask = Ask::None;
        if (ch == 'y' || ch == 'Y') {
            app::saveNow();
            header(nullptr);
            put(" Restarting.\r\n");
            // The page may be larger than the USB buffer: send it all, for half a second at most.
            uint32_t until = millis() + 500;
            while (sending() && (int32_t)(millis() - until) < 0) {
                pump();
            }
            Serial.flush();
            delay(50);
            rp2040.reboot();
        }
        say("Not restarted.");
        return;
    }
    if (ch == 27) {
        ask = Ask::None;
        say("Cancelled.");
        return;
    }
    if (ch == '\r' || ch == '\n') {
        field[fieldLen] = '\0';
        submit();
        return;
    }
    size_t max = ask == Ask::Rtttl ? sound::RTTTL_MAX : 8;
    if ((ch == 8 || ch == 127) && fieldLen > 0) {
        field[--fieldLen] = '\0';
        dirty = true;
    } else if (ch >= 32 && ch < 127 && fieldLen < max) {
        field[fieldLen++] = (char)ch;
        field[fieldLen] = '\0';
        dirty = true;
    }
}

bool isBack(int ch) { return ch == 'b' || ch == 'B' || ch == 27; }

void onKey(int ch, uint32_t now) {
    if (ask != Ask::None) {
        onAskKey(ch);
        return;
    }
    bool enter = ch == '\r' || ch == '\n';
    if (enter) {
        dirty = true;  // Enter draws the page again
    }
    switch (page) {
        case Page::Main:
            switch (ch) {
                case '1': go(Page::Status); break;
                case '2': go(Page::Stats); break;
                case '3':
                    app::setSound(!app::soundOn());
                    say("Sound is now %s.", app::soundOn() ? "on" : "off");
                    break;
                case '4': go(Page::Buzzer); break;
                case '5': go(Page::Hardware); break;
                case '6':
                    say("Erase every lifetime stat? This cannot be undone.");
                    startAsk(Ask::Erase);
                    break;
                case '7':
                    message[0] = ' ';  // the last result is not about the restart
                    startAsk(Ask::Reboot);
                    break;
                case '8': go(Page::Settings); break;
                case '9': go(Page::Views); break;
                case '0': go(Page::Timings); break;
                default: break;
            }
            break;
        case Page::Status:
        case Page::Stats:
            if (isBack(ch)) {
                go(Page::Main);
            }
            break;
        case Page::Buzzer:
            if (sweeping) {
                stopSweep();
                say("Sweep stopped.");
                break;
            }
            if (pickingTune) {
                pickingTune = false;
                if (ch >= '1' && ch < '1' + (int)Tune::Count_) {
                    editTune = (Tune)(ch - '1');
                    candidate[0] = '\0';
                    go(Page::Tune);
                } else {
                    say("No tune chosen.");
                }
                break;
            }
            if (ch >= '1' && ch < '1' + (int)Tune::Count_) {
                Tune t = (Tune)(ch - '1');
                sound::playTest(t);
                say("Playing the %s tune.", sound::tuneName(t));
            } else if (ch == 'e' || ch == 'E') {
                pickingTune = true;
                message[0] = '\0';
                dirty = true;
            } else if (ch == 't' || ch == 'T') {
                message[0] = '\0';
                startAsk(Ask::Hz);
            } else if (ch == 's' || ch == 'S') {
                sweeping = true;
                sweepHz = SWEEP_FROM_HZ;
                sweepNextAt = now;
                message[0] = '\0';
            } else if (ch == 'x' || ch == 'X') {
                sound::stop();
                say("Stopped.");
            } else if (isBack(ch)) {
                go(Page::Main);
            }
            break;
        case Page::Tune:
            if (ch == 'p' || ch == 'P') {
                sound::playTest(editTune);
                say("Playing the tune in use.");
            } else if (ch == 'n' || ch == 'N') {
                message[0] = '\0';
                startAsk(Ask::Rtttl);
            } else if ((ch == 'y' || ch == 'Y') && candidate[0]) {
                sound::playRtttlTest(candidate);
                say("Playing the new tune.");
            } else if ((ch == 'k' || ch == 'K') && candidate[0]) {
                if (app::keepTune(editTune, candidate)) {
                    candidate[0] = '\0';
                    say("Kept: the %s tune is now yours, saved in flash.", sound::tuneName(editTune));
                } else {
                    say("SAVE FAILED: the tune is in use until the next start-up only.");
                }
            } else if (ch == 'r' || ch == 'R') {
                if (app::restoreTune(editTune)) {
                    say("The %s tune is back to the built-in one.", sound::tuneName(editTune));
                } else {
                    say("The built-in tune is in use, but the saved one COULD NOT BE DELETED: it returns at start-up.");
                }
            } else if (isBack(ch)) {
                sound::stop();
                go(Page::Buzzer);
            }
            break;
        case Page::Hardware:
            if (display::testPattern() && (ch == '1' || ch == '2')) {
                break;  // during a display test only 3, 0 and b act, as the footer says
            }
            if (ch == '1') {
                uint8_t found[16];
                int n = display::scan(found, 16);
                char list[80] = "";
                size_t len = 0;
                for (int i = 0; i < n && len < sizeof(list) - 6; i++) {
                    len += (size_t)snprintf(list + len, sizeof(list) - len, " 0x%02X", found[i]);
                }
                say("I2C scan: %d device(s) answered%s%s.", n, n ? ":" : "", list);
                heavyKey = true;
            } else if (ch == '3') {
                display::setTestPattern((uint8_t)(display::testPattern() % display::TEST_PATTERNS + 1));
                dirty = true;
            } else if (ch == '0') {
                display::setTestPattern(0);
                dirty = true;
            } else if (ch == '2') {
                watchCount = 0;
                watchLast = 0xFF;
                go(Page::Watch);
            } else if (isBack(ch)) {
                display::setTestPattern(0);
                go(Page::Main);
            }
            break;
        case Page::Settings: {
            const Settings &s = app::currentSettings();
            if (ch == '1') {
                app::setKnobReversed(!s.knobReversed);
                say("Knob direction: %s.", app::currentSettings().knobReversed ? "reversed" : "normal");
            } else if (ch == '2') {
                app::setDisplaySequentialRows(!s.displaySequentialRows);
                say("Display rows: %s.", app::currentSettings().displaySequentialRows ? "sequential (ALT0)" : "standard");
            } else if (ch == '3') {
                app::setBigFont((uint8_t)((display::bigFont() + 1) % display::bigFontCount()));
                say("Large-line font: %s.", display::bigFontName(display::bigFont()));
            } else if (ch == '4') {
                app::setSmallFont((uint8_t)((display::smallFont() + 1) % display::smallFontCount()));
                say("Top-line font: %s.", display::smallFontName(display::smallFont()));
            } else if (isBack(ch)) {
                go(Page::Main);
            }
            break;
        }
        case Page::Timings: {
            bool found = false;
            for (uint8_t i = 0; i < timings::COUNT; i++) {
                if (ch == timings::info((timings::Id)i).key) {
                    editTiming = i;
                    message[0] = '\0';  // the last result is not about this one
                    startAsk(Ask::Timing);
                    found = true;
                }
            }
            if (found) {
                break;
            }
            if (ch == 'r' || ch == 'R') {
                say(app::resetTimings() ? "Every timing is back to its default." : "Back to the defaults, but the save FAILED.");
            } else if (isBack(ch)) {
                go(Page::Main);
            }
            break;
        }
        case Page::Views:
            if (ch >= '1' && ch < '1' + views::COUNT) {
                uint8_t i = (uint8_t)(ch - '1');
                bool on = (app::currentSettings().viewsOff >> i) & 1;  // off now: switch it on
                if (app::setViewOn(i, on)) {
                    say("%c, %s: %s.", views::letter(i), views::name(i), on ? "on" : "off");
                } else {
                    say("%c, %s stays on: one view at least stays on.", views::letter(i), views::name(i));
                }
            } else if (ch == '0') {
                app::setAllViewsOn();
                say("Every view is on.");
            } else if (isBack(ch)) {
                go(Page::Main);
            }
            break;
        case Page::Watch:
            go(Page::Hardware);
            break;
    }
}

void runSweep(uint32_t now) {
    if (!sweeping || (int32_t)(now - sweepNextAt) < 0) {
        return;
    }
    if (sweepHz > SWEEP_TO_HZ) {
        sweeping = false;
        say("Sweep done.");
        return;
    }
    sound::toneTest(sweepHz, SWEEP_TONE_MS);
    sweepHz += SWEEP_STEP_HZ;
    sweepNextAt = now + SWEEP_TONE_MS + SWEEP_GAP_MS;
    dirty = true;
}

void runWatch(uint32_t now) {
    if (page != Page::Watch) {
        return;
    }
    uint8_t v = (uint8_t)((input::clkLevel() ? 4 : 0) | (input::dtLevel() ? 2 : 0) | (input::swLevel() ? 1 : 0));
    if (v != watchLast) {
        watchLast = v;
        watchLevels[watchCount % WATCH_LINES] = v;
        watchTimes[watchCount % WATCH_LINES] = now;
        watchCount++;
        dirty = true;
    }
}

}  // namespace

namespace console {

void begin(bool watchdogReset) {
    wasWatchdogReset = watchdogReset;
    Serial.begin(SERIAL_BAUD);
}

void update(uint32_t now) {
    bool isConnected = (bool)Serial;
    if (isConnected && !connected) {
        connected = true;
        connectedAt = now;
        greeted = false;
    } else if (!isConnected && connected) {
        connected = false;
        outLen = sentLen = 0;
        escState = 0;
        stopSweep();
        display::setTestPattern(0);  // the device's screens come back when the terminal closes
        ask = Ask::None;
        go(Page::Main);
    }
    if (!connected) {
        while (Serial.available() > 0) {
            Serial.read();
        }
        return;
    }
    // Give the terminal a moment to settle before greeting it.
    if (!greeted && now - connectedAt >= 300) {
        greeted = true;
        go(Page::Main);
    }
    // A few keys per loop, so pasted text cannot hold the loop.
    static int lastCh = 0;
    heavyKey = false;
    for (int i = 0; i < 16 && !heavyKey && Serial.available() > 0; i++) {
        int ch = Serial.read();
        if (ch < 0) {
            break;
        }
        if (!greeted) {  // a key before the greeting greets, then counts
            greeted = true;
            go(Page::Main);
        }
        if (escState == 1) {
            if (ch == '[' || ch == 'O') {
                escState = 2;
                continue;
            }
            escState = 0;
            onKey(27, now);  // a lone Esc, then this key
        }
        if (escState == 2) {
            if (ch >= 0x40 && ch <= 0x7E) {  // the sequence's last byte
                escState = 0;
            }
            continue;
        }
        if (ch == 27) {
            escState = 1;
            escAt = now;
            continue;
        }
        // A terminal may send "\r\n" for one Enter: ignore the "\n" after a "\r".
        if (ch == '\n' && lastCh == '\r') {
            lastCh = ch;
            continue;
        }
        lastCh = ch;
        onKey(ch, now);
    }
    if (escState == 1 && now - escAt >= LONE_ESC_MS) {
        escState = 0;
        onKey(27, now);
    }
    // A sequence that never sends its last byte is dropped, so it cannot swallow every key after it.
    if (escState == 2 && now - escAt >= ESC_SEQUENCE_MS) {
        escState = 0;
    }
    runSweep(now);
    runWatch(now);
    if (dirty && !sending()) {
        draw();
    }
    pump();
}

}  // namespace console
