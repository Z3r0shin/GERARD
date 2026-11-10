# GERARD

A desk focus timer that counts your focus time and keeps reminding you to take breaks, drink and stretch.

![GERARD seen from the front: a black 3D-printed enclosure, a small white OLED display showing START, and a large knob on top.](docs/figures/gerard-front.jpg)

GERARD counts focus time up, or counts a set time down, on a small OLED display worked with one knob. After 30 minutes
of running time it asks for a break, and keeps asking every minute until you answer. It sits on the desk of the
person it keeps on track.

**Release version 1** (2026-11-09) · hardware revision A · firmware `v.1.0.20261109T161718Z`

## The name

All of the author's pets have names starting with G, and GERARD needed a butler's name, in the spirit of IGOR, the
focus timer that inspired it. The name is a French acronym first: *Gestionnaire Efficace Rappelant Assiduité,
Réhydratation et Discipline*. In English: **Gentleman's Efficient Reminder of Assiduity, Rehydration and Discipline**.

## Specifications

- **Microcontroller:** RP2040-Zero board (RP2040).
- **Power:** 5 V DC through a USB-C socket on the enclosure.
- **Display:** white SSD1306 OLED module on I2C, 128 x 64.
- **Input:** KY-040 rotary encoder; turn, press, or hold.
- **Sound:** passive magnetic buzzer, switched by a BC337 transistor.
- **Build:** through-hole parts on one perfboard, JST XH cables to the display and the encoder.
- **Enclosure:** two 3D-printed parts and a knob, PLA.
- **Firmware:** focus count-up, countdown from 1 to 240 minutes, break prompts, nine run animations, and a USB serial
  portal for settings, tunes and a hardware check.

## What is in this repository

| Path | What is there | Open first |
|---|---|---|
| [`hardware/`](hardware/) | The schematic (KiCad 9 and PDF) and the enclosure and knob (STL) | [`gerard - Schematic.pdf`](hardware/kicad/gerard%20-%20Schematic.pdf) |
| [`firmware/`](firmware/) | The firmware source, a PlatformIO project | [`src/pins.h`](firmware/src/pins.h) |
| [`docs/`](docs/) | The Hardware Bible and the Firmware Gospel, as PDF and Markdown, and their figures | [`GERARD - Hardware Bible.pdf`](docs/GERARD%20-%20Hardware%20Bible.pdf) |
| [`llms.txt`](llms.txt) | A map of this repository for an LLM | |
| `LICENSE-*` | The three licences, also in [`LICENSES/`](LICENSES/) | |

## Build it

1. Read the [Hardware Bible (PDF)](docs/GERARD%20-%20Hardware%20Bible.pdf): parts, circuits, wiring and enclosure.
2. Gather the parts in its parts table, also in [`docs/hardware-bible.md`](docs/hardware-bible.md).
3. Build the perfboard from the [schematic](hardware/kicad/gerard%20-%20Schematic.pdf).
4. Make the cables from the [cable drawing](docs/figures/gerard-cables.svg).
5. Print the [enclosure and knob](hardware/3d/).
6. Flash the firmware, as the [Firmware Gospel (PDF)](docs/GERARD%20-%20Firmware%20Gospel.pdf) says in "Build and
   flash".

## Flash the firmware

In a git clone of this repository (the version stamp reads git):

```sh
cd firmware
pio run
# Open and close the board's serial port at 1200 baud: it restarts into its USB bootloader.
picotool load -v -x .pio/build/gerard/firmware.uf2
```

The [Firmware Gospel (PDF)](docs/GERARD%20-%20Firmware%20Gospel.pdf), "Build and flash", gives the details.

## Documentation

| Book | For a person | For an LLM |
|---|---|---|
| Hardware Bible | [`GERARD - Hardware Bible.pdf`](docs/GERARD%20-%20Hardware%20Bible.pdf) | [`hardware-bible.md`](docs/hardware-bible.md) |
| Firmware Gospel | [`GERARD - Firmware Gospel.pdf`](docs/GERARD%20-%20Firmware%20Gospel.pdf) | [`firmware-gospel.md`](docs/firmware-gospel.md) |

An LLM agent can start from [`llms.txt`](llms.txt).

<details>
<summary>More pictures</summary>

![GERARD seen from the back: the knob on top and the USB-C socket in the lower back corner.](docs/figures/gerard-back.jpg)

An earlier stage, out of the enclosure, with the yellow-and-blue display that was later replaced:

![The perfboard with the RP2040-Zero and the buzzer, the encoder module, and a display module on its cable.](docs/figures/gerard-open-earlier.jpg)

![The whole GERARD schematic, revision A.](docs/figures/gerard-schematic.svg)

![The three cables of GERARD, by signal name.](docs/figures/gerard-cables.svg)

</details>

## Help

Found an error, or stuck on a step? Open an issue in this repository's [GitHub issues](../../issues).

## Credits

GERARD is inspired by IGOR, a focus timer by UrbanCircles (`github.com/UrbanCircles/igor`). GERARD's design is its
own and reuses no file from IGOR.

## Licence

| Part | Licence |
|---|---|
| Hardware design (KiCad files, STL) | [CERN-OHL-S-2.0](LICENSE-CERN-OHL-S-2.0) |
| Firmware (everything in `firmware/`) | [GPL-3.0-or-later](LICENSE-GPL-3.0-or-later) |
| Documents (`docs/`, the READMEs, `llms.txt`) | [CC-BY-SA-4.0](LICENSE-CC-BY-SA-4.0) |

© 2026 Zeroshin.
