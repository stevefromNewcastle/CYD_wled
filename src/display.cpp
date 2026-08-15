#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

// TFT_eSPI's own LVGL v9 porting layer (lv_tft_espi_create) owns the draw
// buffer and flush callback — no manual flush_cb needed here, unlike
// LVGL_wled's LovyanGFX-based display.cpp for the Waveshare board.

#define DRAW_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
static uint32_t draw_buf[DRAW_BUF_SIZE / 4];

void display_init() {
    lv_init();

    lv_display_t *disp = lv_tft_espi_create(DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                             draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);  // portrait

    pinMode(DISPLAY_BL_PIN, OUTPUT);
    digitalWrite(DISPLAY_BL_PIN, HIGH);  // TFT_BACKLIGHT_ON is HIGH on this board

    Serial.printf("[DISPLAY] Ready - %dx%d rotation=%d\n",
                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_ROTATION);
}

void display_set_backlight(bool on) {
    digitalWrite(DISPLAY_BL_PIN, on ? HIGH : LOW);
    Serial.printf("[DISPLAY] Backlight %s\n", on ? "on" : "off");
}
