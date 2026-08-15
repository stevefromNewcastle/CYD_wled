#include "wifi_setup.h"
#include "settings.h"
#include "ui.h"
#include <Arduino.h>
#include <WiFiManager.h>

// Set synchronously by save_config_cb(), which WiFiManager invokes on the
// same call stack from within autoConnect()/startConfigPortal() — not from
// an ISR or another thread, so no volatile/atomic qualifier is needed.
static bool s_config_saved = false;

// Fires when the portal form is submitted (WiFi creds and/or params).
static void save_config_cb() {
    s_config_saved = true;
    Serial.println("[WIFI_SETUP] Config saved");
}

// Fires the instant the setup AP goes up, before the portal starts
// serving — flips the LVGL screen to a static "Setup Mode" message.
static void ap_callback(WiFiManager * /*wm*/) {
    Serial.println("[WIFI_SETUP] AP mode active: Touch_Panel_AP");
    ui_show_status(
        "Setup Mode\n\n"
        "Join WiFi:\n"
        "Touch_Panel_AP\n\n"
        "Then follow the\n"
        "prompts in your\n"
        "browser"
    );
    lv_refr_now(NULL);  // force an immediate render — nothing pumps
                         // lv_task_handler() while the portal blocks below
}

// Persists both custom portal fields after a successful save. Screen
// timeout is parsed as an integer; settings_set_screen_timeout_sec()
// itself ignores non-positive/garbage input and keeps the prior value.
static void apply_saved_params(WiFiManagerParameter &wled_host_param,
                                WiFiManagerParameter &timeout_param) {
    settings_set_wled_host(wled_host_param.getValue());
    Serial.printf("[WIFI_SETUP] WLED host saved: %s\n", wled_host_param.getValue());

    settings_set_screen_timeout_sec(atoi(timeout_param.getValue()));
    Serial.printf("[WIFI_SETUP] Screen timeout saved: %d s\n", settings_get_screen_timeout_sec());
}

bool wifi_setup_connect() {
    s_config_saved = false;
    Serial.println("[WIFI_SETUP] Connecting with saved credentials (portal fallback if needed)...");

    char timeout_buf[SETTINGS_SCREEN_TIMEOUT_PARAM_LEN];
    snprintf(timeout_buf, sizeof(timeout_buf), "%d", settings_get_screen_timeout_sec());

    WiFiManager wm;
    WiFiManagerParameter wled_host_param(
        "wled_host", "WLED IP or hostname",
        settings_get_wled_host(), SETTINGS_WLED_HOST_MAX_LEN);
    WiFiManagerParameter timeout_param(
        "screen_timeout", "Screen timeout (seconds)",
        timeout_buf, SETTINGS_SCREEN_TIMEOUT_PARAM_LEN);
    wm.addParameter(&wled_host_param);
    wm.addParameter(&timeout_param);
    wm.setSaveConfigCallback(save_config_cb);
    wm.setAPCallback(ap_callback);
    // Without this, when STA is already connected (see
    // wifi_setup_reconfigure() below) the AP+STA captive-portal
    // sign-in target can end up advertising the STA's own IP instead
    // of an address on the AP's isolated subnet, which phones joining
    // the AP can't reach. Harmless here too (STA isn't connected yet
    // on this path), and keeps both entry points consistent.
    wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    wm.setConfigPortalTimeout(0);  // wait indefinitely

    bool connected = wm.autoConnect("Touch_Panel_AP");
    Serial.printf("[WIFI_SETUP] autoConnect returned %s\n", connected ? "true" : "false");

    if (s_config_saved) {
        apply_saved_params(wled_host_param, timeout_param);
    }
    return true;
}

bool wifi_setup_reconfigure() {
    s_config_saved = false;
    Serial.println("[WIFI_SETUP] Opening reconfigure portal (180s timeout)...");

    char timeout_buf[SETTINGS_SCREEN_TIMEOUT_PARAM_LEN];
    snprintf(timeout_buf, sizeof(timeout_buf), "%d", settings_get_screen_timeout_sec());

    WiFiManager wm;
    WiFiManagerParameter wled_host_param(
        "wled_host", "WLED IP or hostname",
        settings_get_wled_host(), SETTINGS_WLED_HOST_MAX_LEN);
    WiFiManagerParameter timeout_param(
        "screen_timeout", "Screen timeout (seconds)",
        timeout_buf, SETTINGS_SCREEN_TIMEOUT_PARAM_LEN);
    wm.addParameter(&wled_host_param);
    wm.addParameter(&timeout_param);
    wm.setSaveConfigCallback(save_config_cb);
    wm.setAPCallback(ap_callback);
    wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    wm.setConfigPortalTimeout(180);

    // WiFiManager's captivePortal() redirect handler
    // (WiFiManager.cpp's captivePortal(), ~line 2428) determines the
    // address to redirect requests to via server->client().localIP(),
    // which is known-buggy on this ESP32 WebServer stack and returns
    // "0.0.0.0". Its fallback for that case picks WiFi.softAPIP() (the
    // AP's own address, correct) only when STA is disconnected —
    // otherwise it picks WiFi.localIP() (the STA's address) and every
    // request, including one aimed straight at the AP's own IP, gets
    // 302-redirected to the STA's address instead, which clients on
    // the AP subnet can never reach. This reconfigure path only ever
    // runs while STA is already connected, so without this disconnect
    // that fallback would pick the wrong branch on every single call.
    // Safe to drop here: once the portal opens we're re-doing WiFi
    // setup anyway, and a save reboots the device.
    WiFi.disconnect();

    wm.startConfigPortal("Touch_Panel_AP");
    Serial.printf("[WIFI_SETUP] Reconfigure portal closed: %s\n",
                  s_config_saved ? "saved" : "timed out / not saved");

    if (s_config_saved) {
        apply_saved_params(wled_host_param, timeout_param);
    }
    return s_config_saved;
}
