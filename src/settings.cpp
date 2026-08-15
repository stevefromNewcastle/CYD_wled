#include "settings.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>

#define SETTINGS_NAMESPACE   "cydwled"
#define SETTINGS_KEY_HOST    "wled_host"
#define SETTINGS_KEY_TIMEOUT "scr_timeout"

static char g_wled_host[SETTINGS_WLED_HOST_MAX_LEN];
static int  g_screen_timeout_sec = 0;
static bool g_loaded = false;

static void copy_into_buf(const char *src) {
    if (!src) src = "";
    strncpy(g_wled_host, src, sizeof(g_wled_host) - 1);
    g_wled_host[sizeof(g_wled_host) - 1] = '\0';
}

static void load_if_needed() {
    if (g_loaded) return;
    Preferences prefs;
    prefs.begin(SETTINGS_NAMESPACE, true);  // read-only
    String host = prefs.getString(SETTINGS_KEY_HOST, WLED_HOST);
    g_screen_timeout_sec = prefs.getInt(SETTINGS_KEY_TIMEOUT, SCREEN_TIMEOUT_MS / 1000);
    prefs.end();
    copy_into_buf(host.c_str());
    g_loaded = true;
}

const char *settings_get_wled_host() {
    load_if_needed();
    return g_wled_host;
}

void settings_set_wled_host(const char *host) {
    Preferences prefs;
    prefs.begin(SETTINGS_NAMESPACE, false);
    prefs.putString(SETTINGS_KEY_HOST, host);
    prefs.end();
    copy_into_buf(host);
    g_loaded = true;
}

int settings_get_screen_timeout_sec() {
    load_if_needed();
    return g_screen_timeout_sec;
}

void settings_set_screen_timeout_sec(int sec) {
    if (sec <= 0) return;  // ignore invalid input, keep the previous value
    Preferences prefs;
    prefs.begin(SETTINGS_NAMESPACE, false);
    prefs.putInt(SETTINGS_KEY_TIMEOUT, sec);
    prefs.end();
    g_screen_timeout_sec = sec;
    g_loaded = true;
}
