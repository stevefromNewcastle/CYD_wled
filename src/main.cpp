#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "display.h"
#include "touch.h"
#include "ui.h"
#include "wled.h"
#include "wifi_setup.h"
#include "battery.h"
#include "settings.h"

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

static WledPreset g_presets[WLED_MAX_PRESETS];
static int        g_preset_count = 0;
static lv_indev_t *g_touch_indev = nullptr;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[BOOT] CYD_wled starting");

    // Display + LVGL
    display_init();

    // Touch input device
    touch_init();

    // Battery voltage sensing (always reports "no battery" on this board)
    battery_init();

    g_touch_indev = lv_indev_create();
    lv_indev_set_type(g_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_touch_indev, touch_read);

    // WiFi — connects with saved credentials, or opens the Touch_Panel_AP
    // captive portal (see wifi_setup.h) if none are saved or they fail.
    ui_show_status("Connecting to WiFi...");
    lv_timer_handler();

    wifi_setup_connect();

    // Fetch WLED presets
    ui_show_status("Fetching WLED presets...");
    lv_timer_handler();

    if (!wled_fetch_presets(g_presets, g_preset_count)) {
        ui_show_status("No presets found.\nCreate presets in the\nWLED web UI first.");
        return;
    }

    // Show preset buttons
    ui_show_presets(g_presets, g_preset_count);

    Serial.println("[BOOT] Ready");
}

void loop() {
    lv_timer_handler();
    lv_tick_inc(5);  // required even with LV_TICK_CUSTOM/millis() in lv_conf.h --
                      // see Task 6, discovered during on-device testing

    // Power-save: backlight off after the configured idle timeout, back on
    // at the next touch. lv_display_get_inactive_time() tracks input
    // activity automatically, so a touch always wakes it even while
    // blanked. Timeout is runtime-configurable via the Touch_Panel_AP
    // setup portal.
    static bool screen_on = true;
    uint32_t timeout_ms = (uint32_t)settings_get_screen_timeout_sec() * 1000;
    bool should_be_on = lv_display_get_inactive_time(NULL) < timeout_ms;
    if (should_be_on && !screen_on) {
        // The touch that's waking the screen shouldn't also click whatever
        // it landed on — ignore this press until it's released.
        if (g_touch_indev) lv_indev_wait_release(g_touch_indev);
    }
    if (should_be_on != screen_on) {
        display_set_backlight(should_be_on);
        screen_on = should_be_on;
    }

    delay(5);
}
