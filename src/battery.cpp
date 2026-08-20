#include "battery.h"
#include "config.h"
#include <Arduino.h>

static float read_volts() {
    int mv = analogReadMilliVolts(BATTERY_ADC_PIN);
    return (float)mv * 2.0f / 1000.0f;
}

void battery_init() {
    analogReadResolution(12);
    pinMode(BATTERY_ADC_PIN, INPUT);
    Serial.println("[BATTERY] Initialized");
}

int battery_get_percent() {
    float v = read_volts();
    if (v < BATTERY_PRESENT_MIN_V) return -1;

    float pct = (v - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    return (int)(pct + 0.5f);
}
