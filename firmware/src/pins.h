// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD hardware revision A: the single pin file.
// Every microcontroller pin the firmware uses is assigned here and nowhere else.
// Signal names and pads match the Hardware Bible and the schematic (hardware/kicad/gerard.kicad_sch).
// "GPn" is the RP2040-Zero pad printed GPn, which is the RP2040's GPIO n.
#pragma once

#include <stdint.h>

constexpr uint8_t PIN_ENC_CLK = 0;  // ENC_CLK: encoder module (KY-040) pin CLK
constexpr uint8_t PIN_ENC_DT = 1;   // ENC_DT:  encoder module pin DT
constexpr uint8_t PIN_ENC_SW = 2;   // ENC_SW:  encoder module pin SW, the push button
constexpr uint8_t PIN_I2C_SDA = 4;  // I2C_SDA: display data, I2C0 SDA
constexpr uint8_t PIN_I2C_SCL = 5;  // I2C_SCL: display clock, I2C0 SCL
constexpr uint8_t PIN_BUZZER = 8;   // BUZZER:  base of the buzzer's transistor through 1 kOhm; high = sound

// GP16 drives the board's own RGB LED. The firmware leaves it alone.
// Every other pin is unused.
