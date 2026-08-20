#pragma once

void display_init();

// Turns the backlight on/off (power-save screen blanking). Content stays
// rendered underneath — only the backlight is toggled.
void display_set_backlight(bool on);
