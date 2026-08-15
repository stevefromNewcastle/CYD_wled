#include "wled.h"
#include "settings.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// WLED JSON API helpers
// ---------------------------------------------------------------------------

// Parse a presets JSON object ({"1":{"n":"Name",...},...}) into out[].
// Returns count parsed.
static int parse_presets_object(JsonObject root, WledPreset *out, int max_count) {
    int count = 0;
    for (JsonPair kv : root) {
        if (count >= max_count) break;
        const char *key = kv.key().c_str();
        if (key[0] == '~') continue;          // checksum key
        int id = atoi(key);
        if (id <= 0) continue;                // skip reserved id 0
        if (!kv.value().is<JsonObject>()) continue;
        const char *name = kv.value()["n"] | "";
        if (name[0] == '\0') {
            snprintf(out[count].name, sizeof(out[0].name), "Preset %d", id);
        } else {
            strncpy(out[count].name, name, sizeof(out[0].name) - 1);
            out[count].name[sizeof(out[0].name) - 1] = '\0';
        }
        out[count].id = id;
        count++;
    }
    return count;
}

static void sort_presets(WledPreset *out, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (out[j].id < out[i].id) {
                WledPreset tmp = out[i]; out[i] = out[j]; out[j] = tmp;
            }
        }
    }
}

// ---------------------------------------------------------------------------

bool wled_fetch_presets(WledPreset *out, int &count_out) {
    count_out = 0;
    WiFiClient client;

    // --- Try /json/presets (WLED 0.13+) ---
    {
        HTTPClient http;
        http.begin(client, settings_get_wled_host(), WLED_PORT, "/json/presets");
        http.setTimeout(5000);
        int code = http.GET();
        String body = http.getString();
        http.end();

        Serial.printf("[WLED] GET /json/presets -> HTTP %d  len=%d body: %.80s\n", code, body.length(), body.c_str());

        if (code == 200) {
            // WLED embeds each preset's full state (segments, colors,
            // effect/palette params), so document overhead can run well
            // above the raw text length once there are more than a
            // handful of presets. Size the buffer off the actual
            // response instead of guessing a fixed constant that just
            // fails again once more presets are added.
            DynamicJsonDocument doc(body.length() * 3 + 1024);
            DeserializationError err = deserializeJson(doc, body);
            if (err == DeserializationError::Ok) {
                count_out = parse_presets_object(doc.as<JsonObject>(), out, WLED_MAX_PRESETS);
                sort_presets(out, count_out);
                Serial.printf("[WLED] Parsed %d presets from /json/presets\n", count_out);
                return count_out > 0;
            }
            Serial.printf("[WLED] JSON parse error on /json/presets: %s (capacity=%u)\n",
                          err.c_str(), doc.capacity());
        }
    }

    // --- Fallback: /json (all versions) — check for "presets" key ---
    {
        HTTPClient http;
        http.begin(client, settings_get_wled_host(), WLED_PORT, "/json");
        http.setTimeout(5000);
        int code = http.GET();
        String body = http.getString();
        http.end();

        Serial.printf("[WLED] GET /json -> HTTP %d  body: %.80s\n", code, body.c_str());

        if (code != 200) {
            Serial.printf("[WLED] Cannot reach WLED at %s:%d\n", settings_get_wled_host(), WLED_PORT);
            return false;
        }

        DynamicJsonDocument doc(32768);
        if (deserializeJson(doc, body) != DeserializationError::Ok) {
            Serial.println("[WLED] JSON parse error on /json");
            return false;
        }

        // WLED 0.13+ sometimes embeds presets here too
        if (doc.containsKey("presets")) {
            count_out = parse_presets_object(doc["presets"].as<JsonObject>(), out, WLED_MAX_PRESETS);
            sort_presets(out, count_out);
            Serial.printf("[WLED] Parsed %d presets from /json\n", count_out);
            return count_out > 0;
        }

        const char *ver = doc["info"]["ver"] | "unknown";
        Serial.printf("[WLED] Connected to WLED %s\n", ver);
    }

    // --- Last fallback: raw /presets.json file from LittleFS ---
    {
        HTTPClient http;
        http.begin(client, settings_get_wled_host(), WLED_PORT, "/presets.json");
        http.setTimeout(5000);
        int code = http.GET();
        String body = http.getString();
        http.end();

        Serial.printf("[WLED] GET /presets.json -> HTTP %d  len=%d body: %.80s\n", code, body.length(), body.c_str());

        if (code == 200 && body.length() > 2) {
            DynamicJsonDocument doc(body.length() * 3 + 1024);
            DeserializationError err = deserializeJson(doc, body);
            if (err == DeserializationError::Ok) {
                count_out = parse_presets_object(doc.as<JsonObject>(), out, WLED_MAX_PRESETS);
                sort_presets(out, count_out);
                Serial.printf("[WLED] Parsed %d presets from /presets.json\n", count_out);
                return count_out > 0;
            }
            Serial.printf("[WLED] JSON parse error on /presets.json: %s (capacity=%u)\n",
                          err.c_str(), doc.capacity());
        }
    }

    Serial.println("[WLED] No presets found — create some in the WLED web UI first");
    return false;
}

bool wled_activate_preset(int id) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, settings_get_wled_host(), WLED_PORT, "/json/state");
    http.setTimeout(3000);
    http.addHeader("Content-Type", "application/json");

    char body[24];
    snprintf(body, sizeof(body), "{\"ps\":%d}", id);

    int code = http.POST(body);
    http.end();

    Serial.printf("[WLED] activate preset %d -> HTTP %d\n", id, code);
    return code == 200;
}

bool wled_set_color(uint8_t r, uint8_t g, uint8_t b) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, settings_get_wled_host(), WLED_PORT, "/json/state");
    http.setTimeout(3000);
    http.addHeader("Content-Type", "application/json");

    char body[48];
    snprintf(body, sizeof(body), "{\"seg\":[{\"col\":[[%d,%d,%d]]}]}", r, g, b);

    int code = http.POST(body);
    http.end();

    Serial.printf("[WLED] set color rgb(%d,%d,%d) -> HTTP %d\n", r, g, b, code);
    return code == 200;
}
