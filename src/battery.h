#pragma once

// This board (ESP32 CYD) has no battery circuit — it's USB/5V powered.
// battery_get_percent() always reports "no battery attached" so the
// header battery icon in ui.cpp stays hidden; no ui.cpp changes needed
// to support this board's lack of battery hardware.

void battery_init();

// Returns battery level as a percentage (0-100), or -1 if no battery is
// detected. Always -1 on this board.
int battery_get_percent();
