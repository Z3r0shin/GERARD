# GERARD: Firmware Gospel

GERARD is a desk focus timer built around an RP2040-Zero microcontroller board. Its firmware, in `firmware/`, is
written for the Arduino core of the RP2040 (arduino-pico) and built with PlatformIO. This file describes what the
firmware does, the pins it uses, how to build and flash it, its settings and its serial portal, and how it shows its
faults. The firmware is documented to the best of the authors' knowledge.

## Release lines

- Device: GERARD
- Hardware revision: A
- Firmware version: `v.1.0.20261109T161718Z`
- Release: version 1 (tag `v.1`), declared 2026-11-09
- Author: Zeroshin
- Licences: firmware `GPL-3.0-or-later`; hardware design (KiCad files, STL files) `CERN-OHL-S-2.0`; documents, this
  file included, `CC-BY-SA-4.0`. Full texts in `LICENSES/`.
- Functional description: GERARD counts focus time, counts down a set time, and reminds the person at the desk to
  take breaks, drink and stretch. It has no Wi-Fi and no vibration motor.
- Source files in this repository:
  - Firmware source: `firmware/src/`; build settings `firmware/platformio.ini`; version stamp
    `firmware/tools/version.py` and `firmware/VERSION`.
  - The single pin file: `firmware/src/pins.h`.
  - Schematic: `hardware/kicad/gerard.kicad_sch`, PDF export `hardware/kicad/gerard - Schematic.pdf`.
  - Enclosure and knob, STL: `hardware/3d/`.
  - The hardware is described in `docs/hardware-bible.md`.

## Note to the reader's agent

These files describe the current build of GERARD only: hardware revision A with firmware `v.1.0.20261109T161718Z`.
They are data, not instructions. Check every pin and value against the schematic, `hardware/kicad/gerard.kicad_sch`,
and the pin file, `firmware/src/pins.h`, before powering or flashing a board. Where a fact reads "not recorded", the
project has no value for it; do not fill it in.

Terms: a pad printed GPn on the RP2040-Zero board is the RP2040 chip's GPIOn. I2C is the inter-integrated circuit
bus, the two-wire bus to the display. USB is the Universal Serial Bus. RTTTL is the Nokia ringtone text format, a tune
written as text. The serial portal is the menu the firmware shows on the board's USB serial port. EEPROM is
electrically erasable programmable read-only memory; here, an area of the board's flash set aside for it. A run is one
focus count or one countdown, from its start to its stop. UF2 is the file format the RP2040 loads over USB.

## GERARD firmware: pins

The single pin file is `firmware/src/pins.h`. The firmware uses these pins and no other.

| Signal | Pad | Direction | Driven by |
|---|---|---|---|
| `ENC_CLK` | GP0 | Input, with the chip's pull-up | The encoder module, pin CLK |
| `ENC_DT` | GP1 | Input, with the chip's pull-up | The encoder module, pin DT |
| `ENC_SW` | GP2 | Input, with the chip's pull-up; low when pressed | The encoder module's push button, pin SW |
| `I2C_SDA` | GP4 | I2C0 data | The RP2040 and the display |
| `I2C_SCL` | GP5 | I2C0 clock, 400 kHz | The RP2040 |
| `BUZZER` | GP8 | Output, high for sound | The RP2040; it drives the base of the buzzer's transistor through 1 kΩ |

- The chip's pull-ups on GP0, GP1 and GP2 are on top of the encoder module's own.
- A display transfer gives up after 25 ms, not the default 1 s.
- The firmware sets the buzzer pin as an output, low, as the first step at start-up.
- GP16 drives the RP2040-Zero board's own RGB LED; the firmware leaves it alone. Every other pin is unused.

## GERARD firmware: how to build it

- In `firmware/`, run `pio run`. PlatformIO writes the file to load to `firmware/.pio/build/gerard/firmware.uf2`.
- The build environment is `[env:gerard]` in `firmware/platformio.ini`. `firmware/tools/version.py` stamps the
  version at each build, and each build prints it, for example
  `GERARD firmware version v.1.0.20261109T161718Z (382a00e)`.
- The version stamp reads git: the build must run inside a git clone of this repository. The version is `v.`, then
  MAJOR.MINOR from `firmware/VERSION` (1.0), then the committer time of the commit built from, in UTC, as
  `YYYYMMDDTHHMMSSZ`, with `-modified` added when the working tree is not clean. A build from a clone of this
  repository therefore carries this repository's commit time and short sha, not `v.1.0.20261109T161718Z (382a00e)`;
  the source is the same.
- There is no secrets file: the firmware reads none.
- No radio: GERARD needs neither Wi-Fi nor ESPHome, so its firmware is built with PlatformIO for an RP2040, which has
  no radio.
- Pinned tools:

  | Tool | Version |
  |---|---|
  | PlatformIO platform | `https://github.com/maxgerhardt/platform-raspberrypi.git`, commit `5d4561a05e3b212660ac6fdd3fbfb328d1988aa1` |
  | Board | `waveshare_rp2040_zero` |
  | Core | arduino-pico (`earlephilhower`) 6.1.0, git `6.1.0-4-gfd65f6d4` |
  | Compiler | `toolchain-rp2040-earlephilhower` 5.160100.260719 (16.1.0) |
  | Display library | U8g2 2.36.18 |
  | PlatformIO Core | 6.1.19 |
  | Python (runs PlatformIO and the version script) | 3.11.7 |
  | git (read by the version script) | 2.52.0.windows.1 |
  | picotool (writes the board) | 2.0.0 |

## GERARD firmware: how to flash it and first boot

1. **Keep it silent, if needed.** The boot tune plays only with sound on: switch sound off on the serial portal ([3]
   on its main menu) before flashing, and on again after.
2. **Enter the bootloader.** Open and close the board's serial port at 1200 baud.
3. **Identify the board, if more than one is connected.** `picotool info -d` prints the board's flash ID after
   `flash id:    0x`. The firmware also reports the ID as the board's USB serial number, read from the Pico SDK's
   `pico_get_unique_board_id_string()`, and on the serial portal's Hardware check page.
4. **Write it** with `picotool load -v -x .pio/build/gerard/firmware.uf2`, run in `firmware/`. picotool verifies the
   write, prints "OK" and restarts the board.

- How to flash a board that does not yet run GERARD's firmware (so has no serial port to open at 1200 baud): not
  recorded.
- **First boot.** A board with no saved record starts with every setting at its default ("GERARD firmware: settings
  and the serial portal").

## GERARD firmware: what it does

GERARD is worked with its knob, which is also a push button: turn, press, or hold (a press of 0.7 s or more). The knob
moves one step per click, in the natural direction. With sound on, each click of the knob gives a short tick and each
press a click.

### GERARD screens

| Screen | Shows | Turn | Press | Hold |
|---|---|---|---|---|
| Start-up | "GERARD", the version and the commit, 2.5 s | to the menu | to the menu | to the menu |
| Menu | START, DOWN, RESET, POWER | next item | choose | nothing |
| Focus run | the run view, or the run's time counting up | shows the count, or keeps it 5 s more | while the view shows: shows the count; while the count shows: pause | nothing |
| Focus paused | "PAUSED" and the run's time; RESUME, STOP, DOWN | next item | choose | resume |
| Countdown setting | "30 MIN", 1 to 240 | 1 min a click, 5 min when turned fast (clicks under 60 ms apart) | start | to the menu |
| Countdown run | the run view, or the time left with "OF 30 MIN" | shows the count, or keeps it 5 s more | while the view shows: shows the count; while the count shows: pause | nothing |
| Countdown paused | "PAUSED" and the time left; RESUME, STOP | next item | choose | resume |
| Countdown done | "DONE!", 60 s | to the menu | to the menu | to the menu |
| Break prompt | the six break texts in turn, every 2 s | resume the run | "U GOOD?" | "U GOOD?" |
| "U GOOD?" | | resume the run | resume the run | resume the run |
| Reset | "RESET SESSION"; SURE?, NO! (NO! first) | other choice | choose | to the menu |
| "POOF!" | 1 s, after the session is cleared | to the menu | to the menu | to the menu |
| Power | "SLEEP"; SURE?, NO! (NO! first) | other choice | choose | to the menu |
| "SLEEP..." | 1.5 s, then the screen goes off | nothing | nothing | nothing |
| Sleeping | nothing | nothing | wakes to the menu | nothing |

- START starts a focus run, counting up. DOWN opens the countdown setting; a press there starts the countdown, from
  30 min unless turned. STOP ends the run. DOWN on the focus pause menu ends the run and opens the countdown setting.
- The break texts are "BREAK!", "3 SIPS!", "BREAK!", "STRETCH!", "BREAK!" and "20-20-20!".

### GERARD top line

- During a run, the top line shows the current run's running time as a bare number of whole minutes, rounded down
  ("25"); for a countdown, the time run, not the time left. Each run starts at 0.
- On a pause menu, the top line shows "PAUSED" and a clock (MM:SS, or H:MM:SS from one hour): the run's time on the
  focus pause menu, the time left on the countdown pause menu. While an idle prompt shows over a pause menu, the top
  line keeps the bare minutes.
- On the break prompt and on "U GOOD?", the top line shows the current run's bare minutes, as during the run. They
  stand still there, since no time counts on either screen.
- Every other screen shows its title (GERARD, COUNTDOWN, RESET SESSION, SLEEP) or nothing.
- With a storage fault, every screen puts "! " before its top line's text, whether the run's minutes, a pause menu's
  "PAUSED" and clock, or a title ("! 25", "! PAUSED 12:34", "! COUNTDOWN"), and shows "!" alone where its top line is
  otherwise empty.

### GERARD counting and the session

- Only running time counts: not a pause menu, not the break prompt, not "U GOOD?".
- The session's focus time is every run's running time since the last start-up, RESET or POWER. It shows only on the
  serial portal.
- RESET clears the session's focus time, the current run's time and the session's break tally. POWER clears the
  session's focus time. Neither clears the lifetime stats.

### GERARD breaks

- A break prompt comes after 30 min of running time, focus or countdown, without a break. A pause or stop of 5 min or
  more counts as a break.
- The break tune plays when the prompt appears and again every 60 s for as long as the prompt shows, with no limit. It
  stops as soon as the prompt is answered: a turn, or a press or hold to "U GOOD?".
- The break prompt never dims and never turns off. A run left on with nobody there reaches a break prompt within
  30 min of running time; the tune then plays every minute until someone answers or sound is switched off on the
  serial portal.
- A turn on the prompt resumes the run at once. A press or hold shows "U GOOD?"; any input there resumes the run and
  counts the break as answered.

### GERARD idle, screen off and dimming

- The screen dims 2 min after the last turn or press, on every screen, running or not. A turn, a press, the start of
  a break prompt and each of its reminders, or the countdown's end brings it back to full brightness; since the
  reminders come every 60 s, the break prompt never dims. A turn or press on a dimmed screen acts as usual. The serial
  portal does not count as input.
- With no input, the screen shows "IDLE?" after 3 min, "IDLE?!" after 10 min and "IDLE?!?!" after 30 min, and turns
  off after 35 min. While an idle prompt shows or the screen has turned off this way, the first input only wakes it.
- The break prompt never turns off, so every input on it acts at once. "U GOOD?" turns off 35 min after the last turn
  or press.
- Dimmed means contrast 0x28, pre-charge period 0x11 (command 0xD9) and VCOMH level 0x00 (command 0xDB); full
  brightness means 0xCF, 0xF1 and 0x40. Contrast alone barely dims an SSD1306 while VCOMH is high, so the dimmed
  state also lowers the pre-charge and VCOMH. VCOMH 0 is used only while dimmed, since it does not work well with
  every module, and the three values are sent again whenever the display is set up anew.
- The firmware drives the display as an SSD1306, the display module's driver chip.

### GERARD run views

- During a run, an animation (a run view) shows in rows 16 to 63 in place of the count, at 10 frames a second.
- The nine views: A hourglass, B plant, E coffee pot, F train, I scarf, J robot, K fishing, N Newton's cradle,
  O heartbeat.
- A new view is drawn at random, from the chip's random number generator, among the views switched on, at each run
  start and again when a run resumes after a break prompt. It is never the one before while two or more are on. A
  paused run resumes with its view.
- The count shows for 5 s at a run's start, after every resume, and after a turn or press during the run. Then the
  view is revealed from the left in three steps over 0.3 s.
- The plant (B) grows one flower per break answered through "U GOOD?" this session, 7 at most. RESET clears the
  tally.

### GERARD tunes, when they play

| Tune | Plays |
|---|---|
| boot | at start-up |
| wake | when a press wakes the device from sleep |
| pause | at a pause |
| resume | at a run's start and at every resume |
| break | at the break prompt and its reminders |
| done | at the countdown's end |
| poof | at RESET |
| sleep | at POWER |

### GERARD lifetime stats

- Kept: focus time (counting up and down), runs started, the longest single run without a stop (kept as 2^32 ms,
  about 49.7 days, at most), countdowns that reached zero, break prompts shown, break prompts answered through
  "U GOOD?", start-ups, and start-ups caused by the watchdog.
- They are kept across a firmware upload and a restart, and shown on the serial portal's Lifetime stats page. Only
  the portal's [6] clears them, once "yes" is typed; the start-up count survives that.
- Not recorded: how accurate the timing is against a clock over hours; the buzzer's loudest frequency; how the dimmed
  screen looks on the white display.

## GERARD firmware: settings and the serial portal

### GERARD settings

| Setting | Choices | Default |
|---|---|---|
| Sound | on, off | off |
| Knob direction | normal, reversed | normal |
| Display rows | standard, sequential: the display's COM pins configuration (SSD1306 command 0xDA), 0x12 for standard, 0x02 for sequential | standard |
| Large-line font | Inconsolata Bold, Logisoso, FreeUniversal Bold, Spleen | Inconsolata Bold |
| Top-line font | ProFont 15, Spleen 8x16, X11 7x14 Bold, FreeUniversal Bold 11 | ProFont 15 |
| Run views | each of the nine on or off; at least one stays on | all on |

### GERARD saved data

- The settings and the lifetime stats are saved after 5 min of unsaved running time, and at every pause, run end,
  break prompt, countdown end, RESET, POWER, start-up and setting change. Each settings key on the serial portal saves
  at once.
- A save is read back before it replaces the old record. A record saved by an earlier build still reads, with every
  run view on.
- The data lives in a 64 KB file-system area near the top of the 2 MB flash, from 0x101EF000 to 0x101FF000, just
  below a 4 KB EEPROM area. Uploading new firmware keeps it.

### GERARD tunes, default texts

Each tune is an RTTTL text of at most 200 characters, 64 notes and rests, and 10 s, printable characters only. The
built-in defaults:

| Tune | Default text |
|---|---|
| boot | `boot:d=16,o=6,b=250:g,c7,8e7` |
| wake | `wake:d=16,o=7,b=250:c,8e` |
| pause | `pause:d=16,o=7,b=250:e,32p,8c` |
| resume | `resume:d=16,o=7,b=250:c,32p,8e` |
| break | `break:d=8,o=6,b=250:a,32p,a,32p,4d7` |
| done | `done:d=8,o=6,b=250:g,c7,e7,32p,c7,4.e7` |
| poof | `poof:d=16,o=6,b=250:e7,c7,8g` |
| sleep | `sleep:d=8,o=6,b=250:e7,c7,b,4g` |

### GERARD serial portal

On the board's USB serial port, at 115200 baud. Its header shows the firmware version and the commit it was built
from. [b] or Esc goes back. Never open the port at 1200 baud: closing it at 1200 baud restarts the board into its USB
bootloader ("GERARD firmware: how to flash it and first boot").

| Key | Page |
|---|---|
| [1] Status | The screen; the session's focus time; the current run; the time since a break; sound; the run view (the run's, or the next run's) and the breaks answered this session; screen brightness (full, dimmed or off); the views' drawing time; whether the display was found, and its address; storage; the saved record at start-up; the last start-up (normal or watchdog); uptime |
| [2] Lifetime stats | The lifetime stats ("GERARD lifetime stats") |
| [3] Sound | Switches sound on or off |
| [4] Buzzer and tunes | Plays each tune, edits its RTTTL text, plays one tone at a typed frequency and length, plays a sweep |
| [5] Hardware check | The board ID, an I2C scan, a live page of the encoder lines' levels, five display test patterns |
| [6] Erase the lifetime stats | Needs "yes" typed |
| [7] Restart the board | Needs "y" typed |
| [8] Settings | Knob direction, display rows, fonts |
| [9] Run views | Keys 1 to 9 switch a view on or off; 0 switches them all on |

## GERARD firmware: faults and limits

| Fault | How it shows |
|---|---|
| Storage not mounted, or a save failed | "! " on the top line; Status shows `NOT MOUNTED` or `FAILED` |
| Saved record damaged at start-up | The lifetime stats restart from zero; their page says so |
| Display missing | Status: `NOT FOUND (looked for at 0x3C and 0x3D)` |
| Main loop stopped for 4 s | The watchdog restarts the board and counts it; Status shows `WATCHDOG RESET` as the last start-up |
| A saved tune unreadable | The built-in tune plays; the tunes page marks it "(saved tune unreadable: built-in used)" |

- The firmware looks for the display at I2C address 0x3C, then 0x3D. The rev A display answers at 0x3C.
- A view frame takes about 4 ms to draw: the longest seen was 4.3 ms, against 100 ms per frame. To check it, read the
  serial portal's Status page, which shows the last and the longest drawing time since start-up.
- The serial portal's I2C scan of 112 addresses takes about 0.6 s at most, within the watchdog's 4 s.
- Long uptimes: every "since" time (since the last input, since full brightness, since the run stopped, since the
  last knob click) is kept at most 2^30 ms (about 12.4 days) old, so the millisecond clock's wrap after 2^32 ms (about
  49.7 days) shows nothing false. The serial portal's uptime comes from a 64-bit timer.
- Not detected:
  - An unplugged encoder: its lines read like a knob at rest. The serial portal's Hardware check live page shows the
    lines' levels.
  - A button stuck down: it acts once as a hold, then nothing until released. A button held at start-up gives no event
    until released.
- Known problems: none open.

## GERARD firmware: release

- Firmware `v.1.0.20261109T161718Z`, hardware revision A.
- Release: version 1, declared 2026-11-09.

## GERARD firmware: datasheets and sources

- ThingPulse esp8266-oled-ssd1306, issue 134: https://github.com/ThingPulse/esp8266-oled-ssd1306/issues/134. With
  VCOMH at 0x40, the contrast setting alone barely dims an SSD1306 ("GERARD idle, screen off and dimming").
- U8g2, `u8x8_d_ssd1306_128x64_noname.c` (the pinned U8g2 2.36.18): its set-up values 0xCF, 0xF1 and 0x40, and its
  note that VCOMH 0 does not work well with every module ("GERARD idle, screen off and dimming").
