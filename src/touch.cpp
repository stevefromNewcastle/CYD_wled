#include "touch.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

static SPIClass touchscreenSPI = SPIClass(VSPI);
static XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

void touch_init() {
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    // Always report raw, unrotated coordinates here regardless of
    // DISPLAY_ROTATION — LVGL's indev_pointer_proc() already calls
    // lv_display_rotate_point() on every pointer read to match whatever
    // lv_display_set_rotation() was set to in display.cpp (see
    // lv_indev.c). Rotating here too caused a double rotation: two 180°
    // mirrors cancel out, so touches landed diagonally opposite from
    // where the screen was actually rotated to.
    touchscreen.setRotation(0);
    Serial.println("[TOUCH] XPT2046 initialized");
}

void touch_read(lv_indev_t * /*indev*/, lv_indev_data_t *data) {
    if (touchscreen.tirqTouched() && touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();
        // Raw ADC ranges are a property of the resistive panel, not the
        // rotation — same starting values as the base CYD project.
        // Confirmed/adjusted on-device in Task 6.
        int16_t x = map(p.x, 200, 3700, 1, DISPLAY_WIDTH);
        int16_t y = map(p.y, 240, 3800, 1, DISPLAY_HEIGHT);

        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PRESSED;

        static uint32_t last_log = 0;
        uint32_t now = millis();
        if (now - last_log >= 200) {  // throttled, a touch spans many reads
            last_log = now;
            Serial.printf("[TOUCH] raw=(%d,%d) -> logical=(%d,%d)\n", p.x, p.y, x, y);
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
