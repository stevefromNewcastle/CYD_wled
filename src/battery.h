#pragma once

// Reads the LiPo cell on GPIO34 (see BATTERY_* in config.h). The header
// battery icon in ui.cpp already handles both states — hidden when no
// battery is attached, showing a level icon otherwise — so no ui.cpp
// changes are needed here.

void battery_init();

// Returns battery level as a percentage (0-100), or -1 if no battery is
// detected (voltage below BATTERY_PRESENT_MIN_V — e.g. running on USB
// with no battery attached).
int battery_get_percent();
