# GERARD firmware

The firmware for GERARD's RP2040-Zero board, a PlatformIO project. Start with the pin file.

| File | What |
|---|---|
| [`src/pins.h`](src/pins.h) | Every pin the firmware uses. Open this first. |
| [`src/`](src/) | The source: screens and timing, encoder, display, buzzer and tunes, saved data, serial portal, run animations |
| [`platformio.ini`](platformio.ini) | The build environment `gerard`, every version pinned |
| [`VERSION`](VERSION), [`tools/version.py`](tools/version.py) | The version stamp, read from git at each build |

Build with `pio run` in this folder, in a git clone of the repository. Flashing is in the Firmware Gospel, in
[`../docs/`](../docs/). Back to the [main README](../README.md).
