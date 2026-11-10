// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the serial portal, over the board's USB port.
#pragma once

#include <stdint.h>

namespace console {

void begin(bool watchdogReset);
void update(uint32_t now);

}  // namespace console
