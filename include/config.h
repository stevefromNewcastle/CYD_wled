#pragma once

// =============================================================
//  Firmware version — build date/time, shown on the long-press
//  reset-confirm dialog to verify which build is currently flashed.
// =============================================================
#define FIRMWARE_VERSION  __DATE__ " " __TIME__

// =============================================================
//  WLED instance
// =============================================================
// WLED_HOST is only the seed default used the first time the device
// boots (before anything has been saved via the Touch_Panel_AP setup
// portal). After that, the runtime value from settings.h takes over.
// WiFi credentials are not stored here — see wifi_setup.h.
#define WLED_HOST  "192.168.20.98"
#define WLED_PORT  80

// Maximum number of presets to fetch and display
#define WLED_MAX_PRESETS  64

// =============================================================
//  Display (portrait, ILI9341 via TFT_eSPI — pins/driver set in
//  include/User_Setup.h, force-included via platformio.ini)
// =============================================================
#define DISPLAY_WIDTH    240
#define DISPLAY_HEIGHT   320
// 0 = portrait, 180 = portrait upside-down. Override with a
// -DDISPLAY_ROTATION=180 build flag (see platformio.ini) rather than
// editing this file, so the flip is a one-line, per-build toggle.
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 0
#endif
#define DISPLAY_BL_PIN   21   // GPIO21 — TFT_BL, backlight enable (active HIGH)

// Backlight turns off after this many ms with no touch input, and back on
// at the next touch. This is only the seed default used the first time the
// device boots — after that, the runtime value (in seconds, configurable
// via the Touch_Panel_AP setup portal) from settings.h takes over.
#define SCREEN_TIMEOUT_MS  20000

// =============================================================
//  Touch — XPT2046 resistive touch controller, SPI
// =============================================================
#define XPT2046_IRQ   36   // T_IRQ
#define XPT2046_MOSI  32   // T_DIN
#define XPT2046_MISO  39   // T_OUT
#define XPT2046_CLK   25   // T_CLK
#define XPT2046_CS    33   // T_CS

// =============================================================
//  Battery — LiPo cell on the JST connector, sensed via a 2:1
//  resistor divider into GPIO34 (per this board's vendor example).
//  No soft-power-latch circuit here, unlike the Waveshare board
//  LVGL_wled originally targeted — so there's no POWER_HOLD_PIN.
// =============================================================
#define BATTERY_ADC_PIN        34
#define BATTERY_EMPTY_V        3.3f  // 0%
#define BATTERY_FULL_V         4.2f  // 100%
#define BATTERY_PRESENT_MIN_V  3.0f  // below this, treat as "no battery" (USB-only)

// =============================================================
//  Test buttons — two momentary buttons wired to ground, using
//  internal pullups (pressed = pin reads LOW).
// =============================================================
#define BUTTON1_PIN  27
#define BUTTON2_PIN  18
