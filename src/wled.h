#pragma once
#include <Arduino.h>
#include "config.h"

struct WledPreset {
    int  id;
    char name[40];
};

// Fetch all presets from WLED. Returns true on success.
// Fills 'out' array (max WLED_MAX_PRESETS) and sets count_out.
bool wled_fetch_presets(WledPreset *out, int &count_out);

// Activate a preset by ID. Returns true if WLED acknowledged.
bool wled_activate_preset(int id);

// Set the primary segment colour. Returns true if WLED acknowledged.
bool wled_set_color(uint8_t r, uint8_t g, uint8_t b);
