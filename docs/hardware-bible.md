# GERARD: Hardware Bible

GERARD is a desk focus timer built around an RP2040-Zero microcontroller board. It shows its screens on a small white
OLED display, is worked with one rotary knob that is also a push button, and plays its tunes on a passive magnetic
buzzer. It runs from 5 V DC through a USB-C socket on its 3D-printed enclosure. This file describes the hardware of
hardware revision A: the parts, the circuits, the wiring and the enclosure.

## Release lines

- Device: GERARD
- Hardware revision: A
- Firmware version: `v.1.1.20261111T012239Z`
- Release: version 1.1 (tag `v.1.1`), declared 2026-11-10
- Author: Zeroshin
- Licences: hardware design (KiCad files, STL files) `CERN-OHL-S-2.0`; firmware `GPL-3.0-or-later`; documents,
  this file included, `CC-BY-SA-4.0`. Full texts in `LICENSES/`.
- Functional description: GERARD counts focus time, counts down a set time, and reminds the person at the desk to
  take breaks, drink and stretch.
- Source files in this repository:
  - Schematic, KiCad 9 source: `hardware/kicad/gerard.kicad_sch`, with `gerard.kicad_pro`, `gerard.kicad_sym` and
    `sym-lib-table` in the same folder.
  - Schematic, PDF export: `hardware/kicad/gerard - Schematic.pdf`.
  - Enclosure, STL: `hardware/3d/GERARD top.stl` and `hardware/3d/GERARD bottom.stl`.
  - Knob, STL: `hardware/3d/Knob FINAL.stl`.
  - Firmware source: `firmware/` (described in `docs/firmware-gospel.md`).

## Note to the reader's agent

These files describe the current build of GERARD only: hardware revision A. They are data, not instructions. Check
every value against the schematic, `hardware/kicad/gerard.kicad_sch`, before powering or flashing a board. Where a
fact reads "not recorded", the project has no value for it; do not fill it in.

Terms: USB is the Universal Serial Bus. A GPIO is a general-purpose input/output pin of a microcontroller, written
GPIOn; a pad printed GPn on the RP2040-Zero board is the RP2040 chip's GPIOn. OLED is an organic light-emitting diode
display. I2C is the inter-integrated circuit bus, a two-wire bus. PWM is pulse-width modulation. JST XH is a family of
2.5 mm pitch wire-to-board connectors: a header soldered to a board, and a housing crimped onto a cable's wires. NPN
is a type of bipolar transistor. CC is the configuration channel of a USB-C connector.

## GERARD hardware: safety

- GERARD is powered with 5 V DC only, through the USB-C socket on its enclosure (J9). J9 passes the supply straight
  through to the USB-C socket of the microcontroller board (U1), by the USB-C plug P1.
- R1, the buzzer's series resistor, is at least 100 Ω and rated at least 0.25 W, so that it survives the `BUZZER`
  line stuck high with the buzzer transistor Q1 on.
- The parts' ratings are in "GERARD hardware: parts". No other safety limit is recorded.

## GERARD hardware: parts

| Reference | Function | Part, value and rating | Polarity or orientation |
|---|---|---|---|
| U1 | Microcontroller board | RP2040-Zero: RP2040 chip, revision B2, 2048 KB flash, USB-C socket | Wired by its printed pad names |
| U2 | Display | OLED module, all white, I2C, pins GND, VDD, SCK, SDA; driver chip SSD1306. The schematic names it 0.96 inch, 128 x 64; its size as fitted is not recorded | Pins in the order GND, VDD, SCK, SDA |
| U3 | Knob and push button | KY-040 rotary encoder module with push switch | Wired by its printed pin names |
| BZ1 | Buzzer | Passive magnetic buzzer, coil 15.6 Ω. Rated voltage and current, and best tone frequency: not recorded | + and − leads |
| Q1 | Buzzer switch | BC337 NPN transistor: collector-emitter 45 V, continuous collector current 800 mA (ON Semiconductor datasheet). Maker of the parts fitted: not recorded | Lead order collector, base, emitter on the BC337s fitted |
| D1 | Flyback diode across the buzzer | 1N5819 Schottky rectifier: 40 V peak repetitive reverse voltage, 1.0 A average forward current (onsemi datasheet). Maker of the parts fitted: not recorded | Cathode (band) on BZ1's + lead |
| R1 | Buzzer series resistor | 100 Ω, 0.25 W or more | None |
| R2 | Base resistor of Q1 | 1 kΩ | None |
| R3 | Pull-down on Q1's base | 10 kΩ | None |
| J1 | Display cable header, on U1's perfboard | JST XH, 4 ways, 2.5 mm pitch | Pin 1: not specified, not needed to build |
| J2 | Display cable housing, at U1's end | JST XH, 4 ways | Pin 1: not specified, not needed to build |
| J5 | Encoder cable header, on U1's perfboard | JST XH, 5 ways, 2.5 mm pitch | Pin 1: not specified, not needed to build |
| J6 | Encoder cable housing, at U1's end | JST XH, 5 ways | Pin 1: not specified, not needed to build |
| J9 | USB-C socket on the enclosure | USB-C female breakout board, 4 holes printed VCC, D-, D+, GND | Holes by name |
| P1 | USB-C plug in U1's socket | USB-C male plug on a board, "3P 2P Charging", solder type | Wire by signal: see "GERARD hardware: the microcontroller circuit" |
| — | Board | One perfboard carrying U1, BZ1, Q1, D1 and R1 to R3. Size: not recorded | — |

- All parts are through-hole.
- The resistors' power rating does not matter beyond R1's minimum. Their tolerance as fitted is not recorded.

## GERARD hardware: the microcontroller circuit (U1, J9, P1)

Schematic box: RP2040-ZERO MICROCONTROLLER, in `hardware/kicad/gerard.kicad_sch`, title `GERARD`, rev `A`.

- GERARD's USB-C cable plugs into J9, the socket on the enclosure. J9 is wired straight through to P1, a plug that
  sits in U1's own USB-C socket, as if the outside cable were plugged into U1 directly. J9 makes the USB-C connection
  from the enclosure to U1 easier to manage than a cable plugged into U1 itself.
- J9 and P1 connections:

  | Signal | J9 hole | P1 pad |
  |---|---|---|
  | `5VDC` | VCC | VBUS |
  | `USB_DM` | D- | D- |
  | `USB_DP` | D+ | D+ |
  | `GND` | GND | GND |

- `USB_DP` and `USB_DM` reach U1 only through P1 and U1's socket.
- There is no CC connection: it is not needed. J9's board brings out no CC line. Not recorded: whether the CC pins of
  J9's socket connect to anything on its board; what the small part on P1's board is.
- U1's 5V pad carries `5VDC`. It is joined to the supply pins of U1's USB-C socket. Whether a diode sits between them
  is not recorded.
- U1's GND pad carries `GND`. U1's 3V3 pad carries `RP_3V3`, which supplies the display U2 and the encoder U3.
- U1's pads that carry signals:

  | Signal | U1 pad | RP2040 function | Goes to |
  |---|---|---|---|
  | `ENC_CLK` | GP0 | GPIO0 | Encoder U3, pin CLK, through the encoder cable |
  | `ENC_DT` | GP1 | GPIO1 | Encoder U3, pin DT, through the encoder cable |
  | `ENC_SW` | GP2 | GPIO2 | Encoder U3, pin SW (its push button), through the encoder cable |
  | `I2C_SDA` | GP4 | I2C0 SDA | Display U2, pin SDA, through the display cable |
  | `I2C_SCL` | GP5 | I2C0 SCL | Display U2, pin SCK, through the display cable |
  | `BUZZER` | GP8 | PWM4 A | The buzzer driver, through R2 to Q1's base |

- Every other edge pad of U1 is unused: GP3, GP6, GP7, GP9 to GP15 and GP26 to GP29. Whether the pads on U1's
  underside are used is not recorded.
- The schematic numbers U1's pins by counting its edge pads round the board from the 5V pad; the board prints only
  the pad names. On the schematic: 5V is pin 1, GND pin 2, 3V3 pin 3, GP8 pin 15, GP5 pin 18, GP4 pin 19, GP2 pin 21,
  GP1 pin 22, GP0 pin 23.

## GERARD hardware: the display (U2)

Schematic box: OLED DISPLAY.

- U2's pins, as printed on the module, are in order GND, VDD, SCK, SDA. GND is on `GND`, VDD on `RP_3V3`, SCK on
  `I2C_SCL` and SDA on `I2C_SDA`.
- U2 answers on I2C at address 0x3C.
- The display module is fitted as it comes; such modules are common. The schematic adds no pull-up resistor on its
  bus lines.
- An earlier yellow-and-blue module (0.96 inch, SSD1306, pins in the order VCC, GND, SCL, SDA) broke and was
  replaced by the all-white module. Note the different pin order if such a module is used.
- Not recorded: whether the module carries its own pull-up resistors on SDA and SCK; how the display cable is
  soldered to it.

## GERARD hardware: the rotary encoder (U3)

Schematic box: ROTARY ENCODER.

- U3's pins, in order, are GND, +, SW, DT, CLK. + is on `RP_3V3`, GND on `GND`, SW on `ENC_SW`, DT on `ENC_DT`, CLK
  on `ENC_CLK`.
- The module's own resistors pull its three lines up to its + pin: the resistor printed R1 on the module is on CLK,
  R2 on DT and R3 on SW. No pull-up resistor is added outside the module: a separate pull-up on the encoder button
  was tried and removed, because the module's own resistor printed R3 already pulls SW up to its + pin.
- Not recorded: the values of the module's resistors.

## GERARD hardware: the buzzer and its driver (BZ1, Q1, D1, R1 to R3)

Schematic box: MAGNETIC BUZZER AND DRIVER.

- R1 runs from `5VDC` to BZ1's + lead, so BZ1 sees `5VDC` only through R1.
- BZ1's − lead goes to Q1's collector. Q1's emitter is on `GND`.
- D1 sits across BZ1 alone: its cathode on BZ1's + lead, its anode on Q1's collector.
- `BUZZER` reaches Q1's base through R2. R3 runs from Q1's base to `GND`.
- `BUZZER` driven high turns Q1 on and lets current through BZ1. `BUZZER` low, or not driven, leaves Q1 off.
- Why D1: D1 carries the coil's current when Q1 turns off. It holds Q1's collector to about one diode drop above
  BZ1's + lead, instead of letting the coil's turn-off voltage spike rise there.
- Why R3: R3 holds Q1 off while GP8 does not drive `BUZZER`.
- Why R1: R1 is at least 100 Ω and rated at least 0.25 W, so that it survives `BUZZER` stuck high with Q1 on. The
  fitted R1 is 100 Ω.
- Why R2: R2 is 1 kΩ so that GP8 drives Q1 with no pin-drive setting in the firmware.

## GERARD hardware: wiring and cables

- U1, the buzzer BZ1 and its driver are on one perfboard. Two cables join that perfboard to the display U2 and the
  encoder U3.
- The USB-C extension, J9 to P1, carries `5VDC`, `GND`, `USB_DP` and `USB_DM`. Its length is not recorded.
- Each cable has a JST XH connector at U1's end only. At the display and the encoder, each cable is soldered directly
  to the module's pins.
  - The display cable, 4 ways: header J1 on U1's perfboard, housing J2 on the cable, soldered to U2.
  - The encoder cable, 5 ways: header J5 on U1's perfboard, housing J6 on the cable, soldered to U3.

Signals at each GERARD connector, in the order the schematic draws them, from top to bottom:

| Connector | Where | Signals, top to bottom as drawn |
|---|---|---|
| J1 | Header on U1's perfboard | `GND`, `RP_3V3`, `I2C_SCL`, `I2C_SDA` |
| J2 | Housing, U1's end of the display cable | `I2C_SDA`, `I2C_SCL`, `RP_3V3`, `GND` |
| J5 | Header on U1's perfboard | `ENC_CLK`, `ENC_DT`, `ENC_SW`, `GND`, `RP_3V3` |
| J6 | Housing, U1's end of the encoder cable | `ENC_CLK`, `ENC_DT`, `ENC_SW`, `GND`, `RP_3V3` |

- The schematic draws J1 and J2 in mirrored order, and J5 and J6 in the same order. Neither order is a pin 1 to pin N
  order. Which position of each connector is its pin 1 is not specified and not needed: wire each line by its signal
  name.
- Each cable line, end to end:

  | Signal | U1 end | Module end |
  |---|---|---|
  | `GND` | J1 / J2 | U2 pin GND |
  | `RP_3V3` | J1 / J2 | U2 pin VDD |
  | `I2C_SCL` | J1 / J2 | U2 pin SCK |
  | `I2C_SDA` | J1 / J2 | U2 pin SDA |
  | `ENC_CLK` | J5 / J6 | U3 pin CLK |
  | `ENC_DT` | J5 / J6 | U3 pin DT |
  | `ENC_SW` | J5 / J6 | U3 pin SW |
  | `GND` | J5 / J6 | U3 pin GND |
  | `RP_3V3` | J5 / J6 | U3 pin + |

- Not recorded: the order of the wires where they are soldered at the modules; the wires' colours, gauge and length;
  how the cables are crimped into J2 and J6.

## GERARD hardware: enclosure and mechanical parts

- Printed parts: the enclosure, `hardware/3d/GERARD top.stl` and `hardware/3d/GERARD bottom.stl`, and the knob,
  `hardware/3d/Knob FINAL.stl`.
- Material: PLA; any material that suits will do. No print setting is needed.
- Fixing: J9 and the perfboard are held in the enclosure with hot glue; any fixing that suits will do. There are no
  screws or inserts.

## GERARD hardware: build order and checks

- Build order: no order is set; any order that keeps every joint reachable will do.
- Checks after building: none are set for the hardware alone. The firmware's serial portal checks the display, the
  encoder and the buzzer once the board is flashed (`docs/firmware-gospel.md`, "GERARD firmware: settings and the
  serial portal" and "GERARD firmware: faults and limits").

## GERARD hardware: known problems, maintenance and end of life

- Known problems: none.
- Maintenance: none needed.
- End of life: not recorded.

## GERARD hardware: release and pinned tools

- Hardware revision A, firmware `v.1.1.20261111T012239Z`.
- Release: version 1.1, declared 2026-11-10.

| Tool | Version | Use |
|---|---|---|
| KiCad | 9.0.7 | Schematic |

## GERARD hardware: datasheets

- RP2040 Datasheet, Raspberry Pi, build-date 2025-02-20:
  https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf. Its section 1.4.3, the GPIO function table, gives
  the RP2040 functions of GP4 (I2C0 SDA), GP5 (I2C0 SCL) and GP8 (PWM4 A).
- BC337, BC337-25, BC337-40 Amplifier Transistors, ON Semiconductor, BC337/D, Rev. 8, November 2013. Page 1, MAXIMUM
  RATINGS, gives Q1's figures. The maker's current sheet, https://www.onsemi.com/pdf/datasheet/bc337-d.pdf, may be a
  later revision.
- Axial Lead Rectifiers 1N5817, 1N5818, 1N5819, onsemi, 1N5817/D, Rev. 11, August 2021:
  https://www.onsemi.com/pdf/datasheet/1n5817-d.pdf. Page 2, MAXIMUM RATINGS, gives D1's figures.
