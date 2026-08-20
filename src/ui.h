#pragma once
#include <lvgl.h>
#include "wled.h"

void ui_show_status(const char *msg);
void ui_show_presets(const WledPreset *presets, int count);
void ui_show_color_picker();

// Re-fetches the preset list from WLED and rebuilds the presets screen.
// Same action the on-screen Refresh button used to trigger — now wired
// to a long press of physical Button 1 (see buttons.cpp).
void ui_refresh_presets();

// Returns to the presets screen from the colour picker.
void ui_back_to_presets();

// Physical Button 1 short-press action: from the colour screen goes back
// to presets, from anywhere else goes to the colour screen.
void ui_toggle_color_presets();
