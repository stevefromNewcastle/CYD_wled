#pragma once

// Runtime-configurable settings, persisted in NVS (ESP32 Preferences).
//
// Currently just the WLED host. WiFi credentials are NOT stored here —
// WiFiManager/the ESP32 core persist those internally. See wifi_setup.h.

// Max length (including null terminator) for a stored WLED host value.
// Used by wifi_setup.cpp to size its matching WiFiManagerParameter field.
#define SETTINGS_WLED_HOST_MAX_LEN 64

// Returns the saved WLED host, or the WLED_HOST default from config.h if
// nothing has been saved yet. Returned pointer is valid for the life of
// the program (backed by a static buffer).
const char *settings_get_wled_host();

// Persists a new WLED host value.
void settings_set_wled_host(const char *host);

// Max length (including null terminator) for the screen-timeout portal
// field's text value — generous for any sane number of seconds.
#define SETTINGS_SCREEN_TIMEOUT_PARAM_LEN 8

// Returns the saved screen power-save timeout in seconds, or the
// SCREEN_TIMEOUT_MS default from config.h (converted to seconds) if
// nothing has been saved yet.
int settings_get_screen_timeout_sec();

// Persists a new screen timeout, in seconds. Values <= 0 are ignored
// (keeps the previous value) rather than disabling the timeout entirely.
void settings_set_screen_timeout_sec(int sec);
