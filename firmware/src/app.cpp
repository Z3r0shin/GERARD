// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: what the device does. Screens, timers, breaks and stats.
//
// One knob does everything: turn to move between choices, press to choose, hold for "back".
// Focus time is counted in milliseconds and shown in minutes, so a pause loses nothing.
// Nothing here waits: every screen that shows for a while ends from update().

#include "app.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "display.h"
#include "input.h"
#include "sound.h"
#include "timings.h"
#include "views.h"

namespace {

enum class Screen : uint8_t {
    Boot,          // version, for a moment after start-up
    Menu,          // START, DOWN, RESET, POWER
    Focus,         // counting focus time up
    FocusPaused,   // RESUME, STOP, DOWN
    DownSet,       // choosing the countdown length
    Down,          // counting down
    DownPaused,    // RESUME, STOP
    Done,          // the countdown reached zero
    Break,         // the break prompt
    BreakCheck,    // "U GOOD?"
    ResetConfirm,  // SURE?, NO!
    Poof,          // the session was cleared
    PowerConfirm,  // SURE?, NO!
    SleepMessage,  // "SLEEP..."
    Sleeping,      // screen off until a press
};

const char *const SCREEN_NAMES[] = {
    "boot", "menu", "focus", "focus paused", "countdown setting", "countdown", "countdown paused",
    "countdown done", "break prompt", "break check", "reset confirm", "poof", "power confirm",
    "sleep message", "sleeping",
};

const char *const MENU[] = {"START", "DOWN", "RESET", "POWER"};
constexpr int MENU_COUNT = 4;
const char *const FOCUS_PAUSE[] = {"RESUME", "STOP", "DOWN"};
constexpr int FOCUS_PAUSE_COUNT = 3;
const char *const DOWN_PAUSE[] = {"RESUME", "STOP"};
constexpr int DOWN_PAUSE_COUNT = 2;
const char *const CONFIRM[] = {"SURE?", "NO!"};
constexpr int CONFIRM_NO = 1;
const char *const BREAK_TEXTS[] = {"BREAK!", "3 SIPS!", "BREAK!", "STRETCH!", "BREAK!", "20' 20sec"};
constexpr int BREAK_TEXT_COUNT = 6;

Screen screen = Screen::Boot;
uint32_t screenSince = 0;
uint32_t lastUpdate = 0;
uint32_t lastInput = 0;
uint32_t brightSince = 0;  // full brightness since the last turn, press, break prompt or countdown end
bool swallowPress = false;  // the press that woke the screen does nothing else

int menuIndex = 0;
int pauseIndex = 0;
int confirmIndex = CONFIRM_NO;
int downSetMin = 30;  // from the timings at start-up and RESET
uint32_t lastDetentAt = 0;

// 64 bits: a session kept for weeks, or a run never stopped, counts past the 49.7 days of 32 bits.
uint64_t sessionMs = 0;      // focus time this session (up and down), until RESET or POWER (portal only)
uint64_t runMsCount = 0;     // the current run, up or down
uint32_t downTotalMs = 0;    // length of the current countdown
uint32_t sinceBreak = 0;     // running time since the last break
Screen breakFrom = Screen::Focus;  // the run a break interrupted
int breakText = 0;
uint32_t breakTextAt = 0;
uint32_t lastReminder = 0;

Lifetime life;
Settings settings;
uint32_t unsavedMs = 0;
uint32_t unreadableTunes = 0;  // bit n: tune n's saved text did not parse at start-up

// Run views: during a run the count shows for its time (timings.h), then a view takes its place.
uint32_t countUntil = 0;       // the count shows until then
uint32_t wipeStart = 0;        // the view is being revealed since then (0: not yet)
uint8_t viewIndex = 0;         // the view of the run on, paused or last
uint8_t upcomingView = 0;      // the view the next run or break will bring, drawn at random in advance
uint32_t viewDrawnAt = 0;
uint32_t viewTick = 0;         // frames drawn; changes the frame so the display redraws
uint8_t viewCutNow = 0;        // the revealed width, set when a view frame is drawn
uint32_t sessionBreaks = 0;    // breaks answered this session (the plant's flowers), until RESET

bool isRunning(Screen s) { return s == Screen::Focus || s == Screen::Down; }

bool idleEligible(Screen s) {
    return s == Screen::Menu || s == Screen::DownSet || s == Screen::FocusPaused || s == Screen::DownPaused ||
           s == Screen::ResetConfirm || s == Screen::PowerConfirm;
}

// The break prompt never goes off; "U GOOD?" does, after the Screen off timing from the press that showed it.
bool offWhenIdle(Screen s) { return idleEligible(s) || s == Screen::BreakCheck; }

void save() {
    storage::save(life, settings);
    unsavedMs = 0;
}

// A pause or a stop never counts as a break: only ending a break prompt (endBreak) restarts the
// time to the next one.
void go(Screen s, uint32_t now) {
    screen = s;
    screenSince = now;
}

int wrap(int value, int count) { return ((value % count) + count) % count; }

// The count shows for 5 s; a turn or press while it shows starts the 5 s again.
void showCount(uint32_t now) {
    countUntil = now + timings::ms(timings::CountShows);
    wipeStart = 0;
}

bool viewShowing(uint32_t now) { return (int32_t)(now - countUntil) >= 0; }

constexpr uint16_t ALL_VIEWS = (1u << views::COUNT) - 1;

bool viewOn(uint8_t index) { return index < views::COUNT && !((settings.viewsOff >> index) & 1); }

// A view at random among those switched on, never the same as the one before while another is on.
// current may be views::COUNT (none before).
uint8_t randomViewAfter(uint8_t current) {
    uint8_t pick[views::COUNT];
    uint8_t n = 0;
    for (uint8_t i = 0; i < views::COUNT; i++) {
        if (viewOn(i) && i != current) {
            pick[n++] = i;
        }
    }
    if (n == 0) {
        return current;  // the only view on
    }
    return pick[rp2040.hwrand32() % n];
}

// The view changes at each run start and at each break, to one drawn at random.
void nextView() {
    viewIndex = upcomingView;
    upcomingView = randomViewAfter(viewIndex);
}

void resume(Screen run, uint32_t now) {
    go(run, now);
    sound::play(Tune::Resume);
    showCount(now);
}

void startFocus(uint32_t now) {
    runMsCount = 0;
    life.runs++;
    go(Screen::Focus, now);
    sound::play(Tune::Resume);
    nextView();
    showCount(now);
}

void startDown(uint32_t now) {
    runMsCount = 0;
    downTotalMs = (uint32_t)downSetMin * 60 * 1000;
    life.runs++;
    go(Screen::Down, now);
    sound::play(Tune::Resume);
    nextView();
    showCount(now);
}

void pause(Screen paused, uint32_t now) {
    pauseIndex = 0;
    go(paused, now);
    sound::play(Tune::Pause);
    save();
}

void endRun(uint32_t now) {
    runMsCount = 0;
    go(Screen::Menu, now);
    save();
}

void enterBreak(uint32_t now) {
    breakFrom = screen;
    breakText = 0;
    breakTextAt = now;
    lastReminder = now;
    life.breaksPrompted++;
    brightSince = now;
    go(Screen::Break, now);
    sound::play(Tune::Break);
    save();
}

void endBreak(uint32_t now, bool taken) {
    if (taken) {
        life.breaksTaken++;
        sessionBreaks++;
    }
    sinceBreak = 0;
    nextView();
    resume(breakFrom, now);
}

// Running time: counted here, between two calls to update().
void count(uint32_t dt) {
    if (!isRunning(screen)) {
        return;
    }
    if (screen == Screen::Down && runMsCount + dt > downTotalMs) {
        dt = downTotalMs - runMsCount;
    }
    sessionMs += dt;
    runMsCount += dt;
    sinceBreak += dt;
    life.focusMs += dt;
    unsavedMs += dt;
    if (runMsCount > life.longestRunMs) {
        // The saved record keeps 32 bits: a longer run is kept as the most it holds (49.7 days).
        life.longestRunMs = runMsCount < UINT32_MAX ? (uint32_t)runMsCount : UINT32_MAX;
    }
}

void onTurn(int detents, uint32_t now) {
    bool fast = now - lastDetentAt < FAST_TURN_MS;
    lastDetentAt = now;
    switch (screen) {
        case Screen::Menu:
            menuIndex = wrap(menuIndex + detents, MENU_COUNT);
            sound::tick();
            break;
        case Screen::FocusPaused:
            pauseIndex = wrap(pauseIndex + detents, FOCUS_PAUSE_COUNT);
            sound::tick();
            break;
        case Screen::DownPaused:
            pauseIndex = wrap(pauseIndex + detents, DOWN_PAUSE_COUNT);
            sound::tick();
            break;
        case Screen::ResetConfirm:
        case Screen::PowerConfirm:
            confirmIndex = wrap(confirmIndex + detents, 2);
            sound::tick();
            break;
        case Screen::DownSet: {
            int step = fast ? COUNTDOWN_FAST_STEP : 1;
            downSetMin = constrain(downSetMin + detents * step, (int)timings::info(timings::CountdownStart).min,
                                   (int)timings::info(timings::CountdownStart).max);
            sound::tick();
            break;
        }
        case Screen::Focus:
        case Screen::Down:
            showCount(now);  // shows the count, or keeps it 5 s more
            sound::tick();
            break;
        case Screen::Break:
            endBreak(now, false);
            break;
        case Screen::BreakCheck:
            endBreak(now, true);
            break;
        case Screen::Boot:
        case Screen::Done:
        case Screen::Poof:
            go(Screen::Menu, now);
            break;
        default:  // the sleep message and sleeping ignore turns
            break;
    }
}

void onShort(uint32_t now) {
    switch (screen) {
        case Screen::Boot:
        case Screen::Done:
        case Screen::Poof:
            go(Screen::Menu, now);
            break;
        case Screen::Menu:
            if (menuIndex == 0) {
                startFocus(now);
            } else if (menuIndex == 1) {
                go(Screen::DownSet, now);
            } else if (menuIndex == 2) {
                confirmIndex = CONFIRM_NO;
                go(Screen::ResetConfirm, now);
            } else {
                confirmIndex = CONFIRM_NO;
                go(Screen::PowerConfirm, now);
            }
            break;
        case Screen::Focus:
            if (viewShowing(now)) {
                showCount(now);
            } else {
                pause(Screen::FocusPaused, now);
            }
            break;
        case Screen::FocusPaused:
            if (pauseIndex == 0) {
                resume(Screen::Focus, now);
            } else if (pauseIndex == 1) {
                endRun(now);
            } else {
                endRun(now);
                go(Screen::DownSet, now);
            }
            break;
        case Screen::DownSet:
            startDown(now);
            break;
        case Screen::Down:
            if (viewShowing(now)) {
                showCount(now);
            } else {
                pause(Screen::DownPaused, now);
            }
            break;
        case Screen::DownPaused:
            if (pauseIndex == 0) {
                resume(Screen::Down, now);
            } else {
                endRun(now);
            }
            break;
        case Screen::Break:
            go(Screen::BreakCheck, now);
            break;
        case Screen::BreakCheck:
            endBreak(now, true);
            break;
        case Screen::ResetConfirm:
            if (confirmIndex == CONFIRM_NO) {
                go(Screen::Menu, now);
            } else {
                sessionMs = 0;
                runMsCount = 0;
                sinceBreak = 0;
                sessionBreaks = 0;
                downSetMin = timings::get(timings::CountdownStart);
                go(Screen::Poof, now);
                sound::play(Tune::Poof);
                save();
            }
            break;
        case Screen::PowerConfirm:
            if (confirmIndex == CONFIRM_NO) {
                go(Screen::Menu, now);
            } else {
                sessionMs = 0;  // POWER clears the session's focus time, as RESET does
                go(Screen::SleepMessage, now);
                sound::play(Tune::Sleep);
                save();
            }
            break;
        default:
            break;
    }
}

void onLong(uint32_t now) {
    switch (screen) {
        case Screen::FocusPaused:
            resume(Screen::Focus, now);
            break;
        case Screen::DownPaused:
            resume(Screen::Down, now);
            break;
        case Screen::DownSet:
        case Screen::ResetConfirm:
        case Screen::PowerConfirm:
        case Screen::Done:
        case Screen::Poof:
        case Screen::Boot:
            go(Screen::Menu, now);
            break;
        case Screen::Break:
            go(Screen::BreakCheck, now);
            break;
        case Screen::BreakCheck:
            endBreak(now, true);
            break;
        default:  // running screens and the menu have nothing to go back to
            break;
    }
}

void timed(uint32_t now) {
    uint32_t inScreen = now - screenSince;
    switch (screen) {
        case Screen::Boot:
            if (inScreen >= BOOT_SCREEN_MS) {
                go(Screen::Menu, now);
            }
            break;
        case Screen::Poof:
            if (inScreen >= POOF_SCREEN_MS) {
                go(Screen::Menu, now);
            }
            break;
        case Screen::Done:
            if (inScreen >= timings::ms(timings::DoneShows)) {
                go(Screen::Menu, now);
            }
            break;
        case Screen::SleepMessage:
            if (inScreen >= SLEEP_SCREEN_MS) {
                go(Screen::Sleeping, now);
            }
            break;
        case Screen::Down:
            if (runMsCount >= downTotalMs) {
                life.countdownsDone++;
                runMsCount = 0;
                brightSince = now;
                go(Screen::Done, now);
                sound::play(Tune::Done);
                save();
            } else if (sinceBreak >= timings::ms(timings::BreakEvery)) {
                enterBreak(now);
            }
            break;
        case Screen::Focus:
            if (sinceBreak >= timings::ms(timings::BreakEvery)) {
                enterBreak(now);
            }
            break;
        case Screen::Break:
            if (now - breakTextAt >= BREAK_TEXT_PERIOD_MS) {
                breakTextAt = now;
                breakText = (breakText + 1) % BREAK_TEXT_COUNT;
            }
            if (now - lastReminder >= timings::ms(timings::ReminderEvery)) {
                lastReminder = now;
                brightSince = now;  // each reminder brings the screen back to full brightness
                sound::play(Tune::Break);
            }
            break;
        default:
            break;
    }
}

uint32_t idleFor(uint32_t now) { return now - lastInput; }

const char *idleText(uint32_t now) {
    if (!idleEligible(screen)) {
        return nullptr;
    }
    uint32_t t = idleFor(now);
    if (t >= timings::ms(timings::Idle3)) return "IDLE?!?!";
    if (t >= timings::ms(timings::Idle2)) return "IDLE?!";
    if (t >= timings::ms(timings::Idle1)) return "IDLE?";
    return nullptr;
}

bool screenOff(uint32_t now) {
    if (screen == Screen::Sleeping) {
        return true;
    }
    return offWhenIdle(screen) && idleFor(now) >= timings::ms(timings::OffAfter);
}

bool asleepToInput(uint32_t now) { return screen == Screen::Sleeping || screenOff(now) || idleText(now); }

// Whole minutes only, with no unit: "25", "130".
void fmtMinutes(char *out, size_t n, uint64_t ms) { snprintf(out, n, "%llu", (unsigned long long)(ms / 60000)); }

void fmtClock(char *out, size_t n, uint64_t ms) {
    uint64_t s = ms / 1000;
    if (s >= 3600) {
        snprintf(out, n, "%llu:%02llu:%02llu", (unsigned long long)(s / 3600), (unsigned long long)(s / 60 % 60),
                 (unsigned long long)(s % 60));
    } else {
        snprintf(out, n, "%02llu:%02llu", (unsigned long long)(s / 60), (unsigned long long)(s % 60));
    }
}

views::Input viewInput() {
    views::Input in;
    if (screen == Screen::Down) {
        in.posMs = runMsCount;
        in.lenMs = downTotalMs;
    } else {
        uint32_t breakMs = timings::ms(timings::BreakEvery);
        in.posMs = sinceBreak < breakMs ? sinceBreak : breakMs;
        in.lenMs = breakMs;
    }
    in.p = in.lenMs > 0 ? in.posMs / in.lenMs : 0;
    in.breaks = sessionBreaks;
    return in;
}

// While a run's view shows: draw it at 10 frames a second, and reveal it from the left over
// VIEW_REVEAL_MS after the count.
void runView(uint32_t now) {
    if (!isRunning(screen) || !viewShowing(now)) {
        return;
    }
    if (wipeStart == 0) {
        wipeStart = now ? now : 1;
        viewDrawnAt = now - VIEW_FRAME_MS;
    }
    if (now - viewDrawnAt >= VIEW_FRAME_MS) {
        viewDrawnAt = now;
        views::render(viewIndex, viewInput(), now);
        viewTick++;
        // The reveal moves only with the view's frames: 0, then about a third, two thirds, all.
        uint32_t inWipe = now - wipeStart;
        viewCutNow = inWipe >= VIEW_REVEAL_MS ? 128 : (uint8_t)((inWipe * 128 + VIEW_REVEAL_MS / 2) / VIEW_REVEAL_MS);
    }
}

// The view over the count, in rows 16 to 63: the count stays under the part not yet revealed.
void addView(Frame &f, uint32_t now) {
    if (!viewShowing(now) || wipeStart == 0) {
        return;
    }
    f.viewCut = viewCutNow;  // 0 on the first frame after the count: the count still shows whole
    f.viewTick = viewTick;
}

void buildScreen(Frame &f, uint32_t now) {
    memset(&f, 0, sizeof(f));
    char t[24];  // room for any 64-bit time; f.top truncates past its own size
    // The current run's minutes only, shown during runs, on the pause menus and on the break
    // prompt and "U GOOD?". The pause menus put the run's time there instead, except while an
    // idle prompt shows. Elsewhere the band is empty or holds the screen's title. The storage-fault mark is
    // added by build().
    bool paused = screen == Screen::FocusPaused || screen == Screen::DownPaused;
    bool onBreak = screen == Screen::Break || screen == Screen::BreakCheck;
    if (isRunning(screen) || paused || onBreak) {
        fmtMinutes(t, sizeof(t), runMsCount);
        snprintf(f.top, sizeof(f.top), "%s", t);
    }
    f.off = screenOff(now);
    // The break prompt never dims, whatever the timings.
    f.dim = !f.off && screen != Screen::Break && now - brightSince >= timings::ms(timings::DimAfter);
    const char *idle = idleText(now);
    if (idle) {
        snprintf(f.big, sizeof(f.big), "%s", idle);
        return;
    }
    // Countdown time left, rounded up to the second.
    uint64_t left = downTotalMs > runMsCount ? downTotalMs - runMsCount + 999 : 0;
    switch (screen) {
        case Screen::Boot: {
            snprintf(f.top, sizeof(f.top), "GERARD");
            const char *v = FW_VERSION_SHORT;
            const char *space = strchr(v, ' ');
            size_t len = space ? (size_t)(space - v) : strlen(v);
            snprintf(f.big, sizeof(f.big), "%.*s", (int)len, v);
            snprintf(f.small, sizeof(f.small), "%s %s", space ? space + 1 : "", FW_SHA);
            break;
        }
        case Screen::Menu:
            snprintf(f.big, sizeof(f.big), "%s", MENU[menuIndex]);
            f.dots = MENU_COUNT;
            f.dot = menuIndex;
            break;
        case Screen::Focus:
            fmtClock(f.big, sizeof(f.big), runMsCount);
            addView(f, now);
            break;
        case Screen::FocusPaused:
            fmtClock(t, sizeof(t), runMsCount);
            snprintf(f.top, sizeof(f.top), "PAUSED %s", t);
            snprintf(f.big, sizeof(f.big), "%s", FOCUS_PAUSE[pauseIndex]);
            f.dots = FOCUS_PAUSE_COUNT;
            f.dot = pauseIndex;
            break;
        case Screen::DownSet:
            snprintf(f.top, sizeof(f.top), "COUNTDOWN");
            snprintf(f.big, sizeof(f.big), "%d MIN", downSetMin);
            break;
        case Screen::Down:
            fmtClock(f.big, sizeof(f.big), left);
            snprintf(f.small, sizeof(f.small), "OF %lu MIN", (unsigned long)(downTotalMs / 60000));
            addView(f, now);
            break;
        case Screen::DownPaused:
            fmtClock(t, sizeof(t), left);
            snprintf(f.top, sizeof(f.top), "PAUSED %s", t);
            snprintf(f.big, sizeof(f.big), "%s", DOWN_PAUSE[pauseIndex]);
            f.dots = DOWN_PAUSE_COUNT;
            f.dot = pauseIndex;
            break;
        case Screen::Done:
            snprintf(f.top, sizeof(f.top), "COUNTDOWN");
            snprintf(f.big, sizeof(f.big), "DONE!");
            snprintf(f.small, sizeof(f.small), "%lu MIN", (unsigned long)(downTotalMs / 60000));
            break;
        case Screen::Break:
            snprintf(f.big, sizeof(f.big), "%s", BREAK_TEXTS[breakText]);
            break;
        case Screen::BreakCheck:
            snprintf(f.big, sizeof(f.big), "U GOOD?");
            break;
        case Screen::ResetConfirm:
            snprintf(f.top, sizeof(f.top), "RESET SESSION");
            snprintf(f.big, sizeof(f.big), "%s", CONFIRM[confirmIndex]);
            f.dots = 2;
            f.dot = confirmIndex;
            break;
        case Screen::Poof:
            snprintf(f.big, sizeof(f.big), "POOF!");
            break;
        case Screen::PowerConfirm:
            snprintf(f.top, sizeof(f.top), "SLEEP");
            snprintf(f.big, sizeof(f.big), "%s", CONFIRM[confirmIndex]);
            f.dots = 2;
            f.dot = confirmIndex;
            break;
        case Screen::SleepMessage:
            snprintf(f.big, sizeof(f.big), "SLEEP...");
            break;
        case Screen::Sleeping:
            break;
    }
}

// The screen, then the storage-fault mark on every screen: "! " before the
// top line's text, whether minutes, a clock or a title, or "!" alone where the top line is empty.
void build(Frame &f, uint32_t now) {
    buildScreen(f, now);
    if (!storage::healthy()) {
        char marked[sizeof(f.top)] = {};  // zeroed: the whole frame is compared to decide a redraw
        if (f.top[0]) {
            snprintf(marked, sizeof(marked), "! %s", f.top);
        } else {
            snprintf(marked, sizeof(marked), "!");
        }
        memcpy(f.top, marked, sizeof(f.top));
    }
}

}  // namespace

namespace app {

void begin(uint32_t now, bool watchdogReset) {
    storage::begin(life, settings);
    storage::loadTimings();  // the defaults when none are saved or the file is damaged
    input::setHoldMs(timings::ms(timings::HoldMs));
    downSetMin = timings::get(timings::CountdownStart);
    sound::setEnabled(settings.soundOn);
    input::setReversed(settings.knobReversed);
    display::setSequentialRows(settings.displaySequentialRows);
    display::setBigFont(settings.bigFont);
    display::setSmallFont(settings.smallFont);
    settings.bigFont = display::bigFont();  // an index out of range was replaced by the first font
    settings.smallFont = display::smallFont();
    // Tunes edited from the portal; one that no longer parses is left at its built-in default
    // and reported on the portal.
    for (size_t i = 0; i < (size_t)Tune::Count_; i++) {
        char text[sound::RTTTL_MAX + 2];
        if (storage::loadTune(sound::tuneName((Tune)i), text, sizeof(text)) && !sound::setTuneText((Tune)i, text)) {
            unreadableTunes |= 1u << i;
        }
    }
    life.boots++;
    if (watchdogReset) {
        life.watchdogResets++;
    }
    save();
    lastUpdate = lastInput = brightSince = now;
    // Bits past the last view are dropped; a record with every view off (none can be saved so)
    // switches them all on.
    settings.viewsOff &= ALL_VIEWS;
    if (settings.viewsOff == ALL_VIEWS) {
        settings.viewsOff = 0;
    }
    upcomingView = randomViewAfter(views::COUNT);
    viewIndex = upcomingView;
    go(Screen::Boot, now);
    sound::play(Tune::Boot);
}

void update(uint32_t now) {
    count(now - lastUpdate);
    lastUpdate = now;

    // Keep the input ages at most 2^30 ms (about 12 days), so a very long time without input never wraps
    // round to "just now" (millis() wraps after 49.7 days).
    const uint32_t AGE_CAP = 1UL << 30;
    if (now - lastInput > AGE_CAP) lastInput = now - AGE_CAP;
    if (now - brightSince > AGE_CAP) brightSince = now - AGE_CAP;
    if (now - lastDetentAt > AGE_CAP) lastDetentAt = now - AGE_CAP;        // a turn after weeks is not "fast"

    int detents = input::takeDetents();
    ButtonEvent ev = input::pollButton(now);

    // Input while the screen is asleep (sleeping, idle prompt or blanked) only wakes it. On a dimmed
    // screen it brightens the screen and acts as usual.
    if (detents || ev == ButtonEvent::Down) {
        bool wasAsleep = asleepToInput(now);
        lastInput = brightSince = now;
        if (wasAsleep) {
            if (screen == Screen::Sleeping) {
                if (ev == ButtonEvent::Down) {  // only a press wakes from sleep
                    go(Screen::Menu, now);
                    sound::play(Tune::Wake);
                    swallowPress = true;
                }
            } else {
                swallowPress = ev == ButtonEvent::Down;
                if (swallowPress) {
                    sound::click();
                } else {
                    sound::tick();
                }
            }
            detents = 0;
            ev = ButtonEvent::None;
        }
    }
    if (ev == ButtonEvent::Down) {
        sound::click();
    } else if (ev == ButtonEvent::Short || ev == ButtonEvent::Long) {
        if (swallowPress) {
            swallowPress = false;
        } else if (ev == ButtonEvent::Short) {
            onShort(now);
        } else {
            onLong(now);
        }
    }
    if (detents) {
        onTurn(detents, now);
    }

    timed(now);
    runView(now);
    if (unsavedMs >= SAVE_AFTER_MS) {
        save();
    }

    Frame f;
    build(f, now);
    display::update(now, f);
    sound::update(now);
}

const char *screenName() { return SCREEN_NAMES[(size_t)screen]; }
// While a run is on or paused, its own view; otherwise the view the next run will start with.
uint8_t runViewIndex() {
    bool paused = screen == Screen::FocusPaused || screen == Screen::DownPaused;
    return isRunning(screen) || paused ? viewIndex : upcomingView;
}
const char *runViewState() {
    if (isRunning(screen)) return viewShowing(millis()) ? "showing now" : "count showing";
    if (screen == Screen::FocusPaused || screen == Screen::DownPaused) return "run paused";
    return "for the next run";
}
const char *screenLevel() {
    uint32_t now = millis();
    if (screenOff(now)) return "off";
    return screen != Screen::Break && now - brightSince >= timings::ms(timings::DimAfter) ? "dimmed" : "full";
}
uint32_t sessionBreakCount() { return sessionBreaks; }
uint64_t sessionFocusMs() { return sessionMs; }
uint64_t runMs() { return runMsCount; }
uint32_t sinceBreakMs() { return sinceBreak; }
const Lifetime &lifetime() { return life; }
bool soundOn() { return settings.soundOn; }

void setSound(bool on) {
    settings.soundOn = on;
    sound::setEnabled(on);
    save();
}

const Settings &currentSettings() { return settings; }

void setKnobReversed(bool reversed) {
    settings.knobReversed = reversed;
    input::setReversed(reversed);
    save();
}

void setDisplaySequentialRows(bool sequential) {
    settings.displaySequentialRows = sequential;
    display::setSequentialRows(sequential);
    save();
}

void setBigFont(uint8_t index) {
    display::setBigFont(index);
    settings.bigFont = display::bigFont();
    save();
}

void setSmallFont(uint8_t index) {
    display::setSmallFont(index);
    settings.smallFont = display::smallFont();
    save();
}

bool setViewOn(uint8_t index, bool on) {
    if (index >= views::COUNT) {
        return false;
    }
    uint16_t off = on ? (uint16_t)(settings.viewsOff & ~(1u << index)) : (uint16_t)(settings.viewsOff | (1u << index));
    if (off == ALL_VIEWS) {
        return false;  // one view at least stays on
    }
    settings.viewsOff = off;
    // The next view is drawn again among those on. A view on screen now stays until the next change.
    if (!viewOn(upcomingView) || on) {
        upcomingView = randomViewAfter(viewIndex);
    }
    save();
    return true;
}

void setAllViewsOn() {
    settings.viewsOff = 0;
    upcomingView = randomViewAfter(viewIndex);
    save();
}

bool setTiming(uint8_t id, uint16_t value) {
    if (id >= timings::COUNT || !timings::set((timings::Id)id, value)) {
        return false;
    }
    if (id == timings::HoldMs) {
        input::setHoldMs(timings::ms(timings::HoldMs));
    }
    return storage::saveTimings();
}

bool resetTimings() {
    timings::resetAll();
    input::setHoldMs(timings::ms(timings::HoldMs));
    return storage::saveTimings();
}

void clearLifetime() {
    uint32_t boots = life.boots;
    memset(&life, 0, sizeof(life));
    life.boots = boots;
    save();
}

void saveNow() { save(); }

uint32_t unreadableTuneMask() { return unreadableTunes; }

bool keepTune(Tune tune, const char *rtttl) {
    if (!sound::setTuneText(tune, rtttl)) {
        return false;
    }
    unreadableTunes &= ~(1u << (size_t)tune);
    return storage::saveTune(sound::tuneName(tune), rtttl);
}

bool restoreTune(Tune tune) {
    sound::resetTuneText(tune);
    unreadableTunes &= ~(1u << (size_t)tune);
    return storage::deleteTune(sound::tuneName(tune));
}

}  // namespace app
