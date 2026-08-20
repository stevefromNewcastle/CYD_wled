#include "buttons.h"
#include "config.h"
#include "ui.h"
#include <Arduino.h>
#include <esp_timer.h>

#define BUTTON_DEBOUNCE_MS     30
#define BUTTON1_LONG_PRESS_MS  600    // held this long -> refresh instead of short-press action
#define BUTTON2_HOLD_MS        2000   // held this long -> restart
#define HOLD_CHECK_PERIOD_US   100000 // 100ms

struct ButtonState {
    uint8_t pin;
    const char *name;
    volatile bool irq_flag;
    bool stable_pressed;
    uint32_t last_irq_ms;
};

static ButtonState g_buttons[2] = {
    { BUTTON1_PIN, "1", false, false, 0 },
    { BUTTON2_PIN, "2", false, false, 0 },
};

static void IRAM_ATTR isr_button1() { g_buttons[0].irq_flag = true; }
static void IRAM_ATTR isr_button2() { g_buttons[1].irq_flag = true; }

// Button 1: short press toggles between the presets and colour screens
// (see ui_toggle_color_presets()); holding it past BUTTON1_LONG_PRESS_MS
// instead refreshes the presets, and suppresses the short-press action on
// release.
static uint32_t g_btn1_press_start_ms = 0;  // 0 = not currently pressed
static bool g_btn1_long_fired = false;

// Button 2's hold-to-restart runs on its own periodic esp_timer instead of
// off buttons_update()/loop(). loop() is fully blocked for as long as the
// WiFi setup/reconfigure portal is showing (wifi_setup.cpp's
// wm.autoConnect()/startConfigPortal() calls don't return until the portal
// closes), so a restart escape hatch that only ran from loop() would be
// unusable on that screen. esp_timer's callback runs in its own FreeRTOS
// task, which keeps getting scheduled as long as something (WiFiManager's
// internal loop, in this case) still yields — which it does.
static esp_timer_handle_t g_hold_timer = nullptr;
static uint32_t g_btn2_press_start_ms = 0;  // 0 = not currently pressed
static bool g_btn2_restart_fired = false;

static void hold_check_cb(void * /*arg*/) {
    bool pressed = digitalRead(BUTTON2_PIN) == LOW;
    if (!pressed) {
        g_btn2_press_start_ms = 0;
        g_btn2_restart_fired = false;
        return;
    }

    uint32_t now = millis();
    if (g_btn2_press_start_ms == 0) g_btn2_press_start_ms = now;
    if (!g_btn2_restart_fired && now - g_btn2_press_start_ms >= BUTTON2_HOLD_MS) {
        g_btn2_restart_fired = true;
        Serial.println("[BUTTON] 2 held 2s, restarting");
        delay(50);  // let the Serial line flush before reboot
        ESP.restart();
    }
}

void buttons_init() {
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON1_PIN), isr_button1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), isr_button2, CHANGE);

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &hold_check_cb;
    timer_args.name = "btn2_hold";
    esp_timer_create(&timer_args, &g_hold_timer);
    esp_timer_start_periodic(g_hold_timer, HOLD_CHECK_PERIOD_US);

    Serial.println("[BUTTONS] Initialized (GPIO27, GPIO18, internal pullups)");
}

void buttons_update() {
    uint32_t now = millis();

    for (ButtonState &btn : g_buttons) {
        if (!btn.irq_flag) continue;
        btn.irq_flag = false;
        btn.last_irq_ms = now;
    }

    for (int i = 0; i < 2; i++) {
        ButtonState &btn = g_buttons[i];
        if (btn.last_irq_ms == 0 || now - btn.last_irq_ms < BUTTON_DEBOUNCE_MS) continue;

        bool pressed = digitalRead(btn.pin) == LOW;
        if (pressed != btn.stable_pressed) {
            btn.stable_pressed = pressed;
            Serial.printf("[BUTTON] %s %s\n", btn.name, pressed ? "pressed" : "released");

            if (i == 0) {
                if (pressed) {
                    g_btn1_press_start_ms = now;
                    g_btn1_long_fired = false;
                } else {
                    if (!g_btn1_long_fired) ui_toggle_color_presets();
                    g_btn1_press_start_ms = 0;
                }
            }
        }
        btn.last_irq_ms = 0;
    }

    // Button 1 held past the threshold -> refresh instead of the
    // short-press toggle (fires once, while still held).
    if (g_buttons[0].stable_pressed && !g_btn1_long_fired &&
        g_btn1_press_start_ms != 0 && now - g_btn1_press_start_ms >= BUTTON1_LONG_PRESS_MS) {
        g_btn1_long_fired = true;
        Serial.println("[BUTTON] 1 held, refreshing presets");
        ui_refresh_presets();
    }
}
