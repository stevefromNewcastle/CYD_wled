#pragma once

// Connect using previously-saved WiFi credentials. If none are saved, or
// they fail to connect, opens the "Touch_Panel_AP" captive portal
// (open/no password, blocking, no timeout — waits indefinitely for setup
// to complete since the device has no useful fallback without WiFi).
// Always returns true once WiFi is connected.
bool wifi_setup_connect();

// Re-opens the "Touch_Panel_AP" captive portal on demand, even though
// WiFi is already connected — used by the long-press "reconfigure" flow
// on the Presets screen. Blocking, 180s timeout.
// Returns true if the user saved new settings (caller should restart the
// device), false if the portal timed out or was closed without saving
// (caller should just return to the current screen — nothing changed).
bool wifi_setup_reconfigure();
