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
    // Portrait, no LVGL-level rotation (DISPLAY_ROTATION 0) — start with the
    // touch chip's own rotation at 0 so its raw axes match the panel's
    // native orientation. Confirmed/adjusted on-device in Task 6.
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
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
