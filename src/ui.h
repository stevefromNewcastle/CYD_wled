#pragma once
#include <lvgl.h>
#include "wled.h"

void ui_show_status(const char *msg);
void ui_show_presets(const WledPreset *presets, int count);
void ui_show_color_picker();
