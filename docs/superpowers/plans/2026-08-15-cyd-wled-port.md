# Port LVGL_wled to the ESP32 CYD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get LVGL_wled's WLED preset/colour remote running on the ESP32 CYD (TFT_eSPI/ILI9341/XPT2046, LVGL v9.2), replacing the Waveshare-specific hardware layer while keeping the WLED/WiFi/settings business logic intact.

**Architecture:** Six-module split carried over from LVGL_wled (`config.h`, `display`, `touch`, `battery`, `ui`, plus the untouched `wled`/`wifi_setup`/`settings`/`logo_mark`). Hardware modules (display, touch, battery) are rewritten for CYD hardware; `ui.cpp` is migrated from LVGL v8 to v9 API (including replacing the colorwheel widget, which v9.5.0 doesn't ship, with RGB sliders); business-logic modules are copied verbatim.

**Tech Stack:** ESP32 Arduino (PlatformIO), LVGL v9.2, TFT_eSPI, XPT2046_Touchscreen, WiFiManager, ArduinoJson.

**Spec:** `docs/superpowers/specs/2026-08-15-cyd-wled-port-design.md`

**No unit tests:** this is hardware-dependent embedded code (display/touch/WiFi) with no practical unit-test harness — LVGL_wled's own specs use the same approach. Each task's "test" step is a `pio run` compile check; hardware behaviour is verified on-device, with two dedicated checkpoints (Task 6 after the hardware layer, Task 11 at the end) rather than after every single task.

---

### Task 1: Add WiFiManager and ArduinoJson to `platformio.ini`

**Files:**
- Modify: `platformio.ini`

- [ ] **Step 1: Add the two new dependencies**

Current `lib_deps` block:
```ini
lib_deps =
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
    bodmer/TFT_eSPI@^2.5.43
    lvgl@^9.2.0
```

Replace with:
```ini
lib_deps =
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
    bodmer/TFT_eSPI@^2.5.43
    lvgl@^9.2.0
    tzapu/WiFiManager@^2.0.17
    bblanchon/ArduinoJson@^6.21.3
```

- [ ] **Step 2: Compile check (existing hello-world main.cpp still builds with the new deps present)**

Run: `cd /Users/stephenholmes/Documents/GitHub/CYD_wled && pio run`
Expected: `[SUCCESS]` — the two new libraries download but aren't referenced by any code yet, so the existing `src/main.cpp` (from the base project) still builds unchanged.

- [ ] **Step 3: Commit**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add platformio.ini
git commit -m "Add WiFiManager and ArduinoJson dependencies for the WLED port"
```

---

### Task 2: Write `include/config.h`

**Files:**
- Create: `include/config.h`

- [ ] **Step 1: Write the file**

```c
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
#define DISPLAY_ROTATION 0    // 0 = portrait
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

// Note: this board has no battery circuit (USB/5V powered) and no
// soft-power-latch circuit, unlike the Waveshare board LVGL_wled
// originally targeted — so there are deliberately no BATTERY_* or
// POWER_HOLD_PIN defines here.
```

- [ ] **Step 2: Commit**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add include/config.h
git commit -m "Add config.h with CYD display/touch pins and WLED settings"
```

(No compile check yet — nothing includes this file until Task 3.)

---

### Task 3: `battery.cpp`/`battery.h` — stub

**Files:**
- Create: `src/battery.h`
- Create: `src/battery.cpp`

- [ ] **Step 1: Write `src/battery.h`**

```c
#pragma once

// This board (ESP32 CYD) has no battery circuit — it's USB/5V powered.
// battery_get_percent() always reports "no battery attached" so the
// header battery icon in ui.cpp stays hidden; no ui.cpp changes needed
// to support this board's lack of battery hardware.

void battery_init();

// Returns battery level as a percentage (0-100), or -1 if no battery is
// detected. Always -1 on this board.
int battery_get_percent();
```

- [ ] **Step 2: Write `src/battery.cpp`**

```c
#include "battery.h"

void battery_init() {
    // No-op — no battery hardware on this board.
}

int battery_get_percent() {
    return -1;
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add src/battery.h src/battery.cpp
git commit -m "Add battery.cpp/h stub — this board has no battery hardware"
```

(No compile check yet — wired together and verified in Task 6.)

---

### Task 4: `display.cpp`/`display.h` — TFT_eSPI + LVGL v9

**Files:**
- Create: `src/display.h`
- Create: `src/display.cpp`

- [ ] **Step 1: Write `src/display.h`**

```c
#pragma once

void display_init();

// Turns the backlight on/off (power-save screen blanking). Content stays
// rendered underneath — only the backlight is toggled.
void display_set_backlight(bool on);
```

- [ ] **Step 2: Write `src/display.cpp`**

```c
#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

// TFT_eSPI's own LVGL v9 porting layer (lv_tft_espi_create) owns the draw
// buffer and flush callback — no manual flush_cb needed here, unlike
// LVGL_wled's LovyanGFX-based display.cpp for the Waveshare board.

#define DRAW_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
static uint32_t draw_buf[DRAW_BUF_SIZE / 4];

void display_init() {
    lv_init();

    lv_display_t *disp = lv_tft_espi_create(DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                             draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);  // portrait

    pinMode(DISPLAY_BL_PIN, OUTPUT);
    digitalWrite(DISPLAY_BL_PIN, HIGH);  // TFT_BACKLIGHT_ON is HIGH on this board

    Serial.printf("[DISPLAY] Ready - %dx%d rotation=%d\n",
                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_ROTATION);
}

void display_set_backlight(bool on) {
    digitalWrite(DISPLAY_BL_PIN, on ? HIGH : LOW);
    Serial.printf("[DISPLAY] Backlight %s\n", on ? "on" : "off");
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add src/display.h src/display.cpp
git commit -m "Add display.cpp/h using TFT_eSPI's LVGL v9 porting layer"
```

(No compile check yet — wired together and verified in Task 6.)

---

### Task 5: `touch.cpp`/`touch.h` — XPT2046

**Files:**
- Create: `src/touch.h`
- Create: `src/touch.cpp`

- [ ] **Step 1: Write `src/touch.h`**

```c
#pragma once
#include <lvgl.h>

void touch_init();
void touch_read(lv_indev_t *indev, lv_indev_data_t *data);
```

- [ ] **Step 2: Write `src/touch.cpp`**

```c
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
```

- [ ] **Step 3: Commit**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add src/touch.h src/touch.cpp
git commit -m "Add touch.cpp/h using XPT2046 SPI polling"
```

---

### Task 6: Hardware smoke test — wire display+touch+battery into a temporary `main.cpp`, compile, flash, verify on-device

This is the first point where display and touch actually run together. Catching a wiring/calibration problem here — before Task 8's much larger `ui.cpp` migration — keeps debugging scoped to just the hardware layer.

**Files:**
- Modify: `src/main.cpp` (temporary content, replaced in Task 9)

- [ ] **Step 1: Replace `src/main.cpp` with a hardware smoke test**

```c
#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "display.h"
#include "touch.h"
#include "battery.h"

static lv_obj_t *coord_label = nullptr;

static void screen_touch_cb(lv_event_t *e) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_label_set_text_fmt(coord_label, "X=%d Y=%d", (int)p.x, (int)p.y);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[BOOT] CYD_wled hardware smoke test");

    display_init();
    touch_init();
    battery_init();

    static lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, touch_read);

    lv_obj_t *scr = lv_screen_active();

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "CYD_wled hardware OK\nTap anywhere");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    coord_label = lv_label_create(scr);
    lv_label_set_text(coord_label, "X=0 Y=0");
    lv_obj_center(coord_label);

    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_PRESSING, NULL);

    Serial.println("[BOOT] Ready");
}

void loop() {
    lv_timer_handler();
    lv_tick_inc(5);  // required even with LV_TICK_CUSTOM/millis() in lv_conf.h --
                      // without it LVGL logs a repeating "lv_tick_inc() is not
                      // called" warning at boot (found during on-device testing;
                      // matches the pattern the base CYD project already used)
    delay(5);
}
```

- [ ] **Step 2: Compile**

Run: `cd /Users/stephenholmes/Documents/GitHub/CYD_wled && pio run`
Expected: `[SUCCESS]`

- [ ] **Step 3: Flash and open the serial monitor**

Run: `cd /Users/stephenholmes/Documents/GitHub/CYD_wled && pio run -t upload && pio device monitor`
Expected serial output: `[BOOT] CYD_wled hardware smoke test` ... `[DISPLAY] Ready - 240x320 rotation=0` ... `[TOUCH] XPT2046 initialized` ... `[BOOT] Ready`

- [ ] **Step 4: On-device visual check — orientation**

Look at the physical screen. Expected: text reads upright in portrait (240 wide, 320 tall — taller than wide). If it's upside-down, change `LV_DISPLAY_ROTATION_0` to `LV_DISPLAY_ROTATION_180` in `src/display.cpp`'s `display_init()`, then re-run Steps 2–3.

- [ ] **Step 5: On-device touch check — all four corners + centre**

Tap each corner and the centre of the screen. Watch the `X=.. Y=..` label update.
Expected: top-left tap reads close to `X=0 Y=0`; top-right close to `X=239 Y=0`; bottom-left close to `X=0 Y=319`; bottom-right close to `X=239 Y=319`; centre close to `X=120 Y=160`.

If any axis is inverted (e.g. top-left reads `X=239`) or swapped (tapping left-right moves the Y reading instead of X), adjust `src/touch.cpp`:
- **X inverted:** change `map(p.x, 200, 3700, 1, DISPLAY_WIDTH)` to `map(p.x, 200, 3700, DISPLAY_WIDTH, 1)`
- **Y inverted:** same swap on the `p.y` map() call
- **Axes swapped:** swap which raw field (`p.x`/`p.y`) feeds `data->point.x` vs `data->point.y`
- **Everything roughly right but consistently off by a margin:** narrow/widen the raw `200, 3700` / `240, 3800` ranges based on the actual raw values printed if you temporarily add `Serial.printf("raw x=%d y=%d\n", p.x, p.y);` inside the `if` block in `touch_read()`

Re-run Steps 2–3 after any change, and repeat this step until all five points are close.

- [ ] **Step 6: Commit the working smoke-test main.cpp (and any calibration fixes from Step 5)**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add src/main.cpp src/display.cpp src/touch.cpp
git commit -m "Hardware smoke test: verify display orientation and touch calibration"
```

---

### Task 7: Copy the unchanged business-logic modules

These modules are hardware-agnostic (WiFi, HTTPClient, ArduinoJson, WiFiManager, Preferences/NVS) and need no LVGL-version or hardware changes.

**`logo_mark` is dropped, not copied.** It's a raw LVGL image descriptor
generated for v8: `logo_mark.cpp` designated-initializes an `lv_img_dsc_t`
using `.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA` and `.header.always_zero` /
`.header.reserved` fields. Checked against the installed v9.5.0 headers —
`LV_IMG_CF_TRUE_COLOR_ALPHA` doesn't exist anywhere in v9 (color formats were
reworked into `lv_color_format_t`), and v9's image header struct has a
different field set entirely (e.g. adds `stride`). The `lv_img_dsc_t` type
*name* still resolves (v9 aliases it to `lv_image_dsc_t`), but the literal
byte data and initializer won't compile against the new struct shape.
Regenerating it would mean re-running LVGL's image-converter tool against a
new color format for a purely decorative status-screen logo — not worth it
for this port. `ui.cpp` (Task 8) skips the logo image entirely; the status
screen keeps its title and message text.

**Files:**
- Create: `src/wled.h`, `src/wled.cpp` (copied from LVGL_wled, unchanged)
- Create: `src/wifi_setup.h`, `src/wifi_setup.cpp` (copied from LVGL_wled, unchanged)
- Create: `src/settings.h`, `src/settings.cpp` (copied from LVGL_wled, NVS namespace renamed)

- [ ] **Step 1: Copy `wled` and `wifi_setup` files verbatim**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
cp /Users/stephenholmes/Documents/GitHub/LVGL_wled/src/wled.h        src/wled.h
cp /Users/stephenholmes/Documents/GitHub/LVGL_wled/src/wled.cpp      src/wled.cpp
cp /Users/stephenholmes/Documents/GitHub/LVGL_wled/src/wifi_setup.h  src/wifi_setup.h
cp /Users/stephenholmes/Documents/GitHub/LVGL_wled/src/wifi_setup.cpp src/wifi_setup.cpp
```

- [ ] **Step 2: Copy `settings.h` verbatim**

```bash
cp /Users/stephenholmes/Documents/GitHub/LVGL_wled/src/settings.h src/settings.h
```

- [ ] **Step 3: Copy `settings.cpp` and rename its NVS namespace**

```bash
cp /Users/stephenholmes/Documents/GitHub/LVGL_wled/src/settings.cpp src/settings.cpp
```

Then edit `src/settings.cpp` — change:
```c
#define SETTINGS_NAMESPACE   "lvglwled"
```
to:
```c
#define SETTINGS_NAMESPACE   "cydwled"
```

(Not functionally required — separate device, separate flash — but avoids the namespace looking like a copy-paste leftover.)

- [ ] **Step 4: Commit**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add src/wled.h src/wled.cpp src/wifi_setup.h src/wifi_setup.cpp \
        src/settings.h src/settings.cpp
git commit -m "Copy WLED/WiFi/settings modules from LVGL_wled unchanged"
```

(No compile check yet — `wled.cpp`/`wifi_setup.cpp` reference `ui_show_status()`, which doesn't exist until Task 8. Verified together in Task 9.)

---

### Task 8: `ui.cpp`/`ui.h` — migrate v8 → v9, replace colorwheel with RGB sliders

**Files:**
- Create: `src/ui.h`
- Create: `src/ui.cpp`

- [ ] **Step 1: Write `src/ui.h`**

```c
#pragma once
#include <lvgl.h>
#include "wled.h"

void ui_show_status(const char *msg);
void ui_show_presets(const WledPreset *presets, int count);
void ui_show_color_picker();
```

(Identical to LVGL_wled's `ui.h` — the public API doesn't change, only the v9 implementation behind it.)

- [ ] **Step 2: Write `src/ui.cpp`**

```c
#include "ui.h"
#include "config.h"
#include "wled.h"
#include "wifi_setup.h"
#include "battery.h"
#include <lvgl.h>
#include <Arduino.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
#define HEADER_H    40
#define BTN_COLS    2
#define BTN_GAP     4
#define BTN_H       60
#define BTN_W       ((DISPLAY_WIDTH - BTN_GAP * (BTN_COLS + 1)) / BTN_COLS)

#define CONTENT_PAD    12
#define SLIDER_H       18
#define SWATCH_H       40
#define SEND_BTN_H     44
#define REFRESH_BAR_H  44

// Colour-picker row Y positions (fixed, not accumulated — easier to verify
// they all fit inside the 320px-tall portrait screen: last element bottom
// edge is 248+44=292, well inside 320).
#define ROW_LABEL_Y_R   52
#define ROW_SLIDER_Y_R  70
#define ROW_LABEL_Y_G   100
#define ROW_SLIDER_Y_G  118
#define ROW_LABEL_Y_B   148
#define ROW_SLIDER_Y_B  166
#define SWATCH_Y        196
#define SEND_BTN_Y      248

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static lv_obj_t *scr_status   = nullptr;
static lv_obj_t *lbl_status   = nullptr;
static lv_obj_t *scr_presets  = nullptr;
static lv_obj_t *scr_color    = nullptr;
static lv_obj_t *color_swatch = nullptr;
static lv_obj_t *lbl_battery  = nullptr;

static lv_obj_t *slider_r = nullptr;
static lv_obj_t *slider_g = nullptr;
static lv_obj_t *slider_b = nullptr;
static lv_obj_t *lbl_r    = nullptr;
static lv_obj_t *lbl_g    = nullptr;
static lv_obj_t *lbl_b    = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static lv_obj_t *make_screen(lv_color_t bg) {
    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, bg, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

// Shared press transition — defined once, used in both screens
static lv_style_transition_dsc_t s_trans;
static bool s_trans_init = false;
static void ensure_trans() {
    if (s_trans_init) return;
    static const lv_style_prop_t props[] = {
        LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, (lv_style_prop_t)0
    };
    lv_style_transition_dsc_init(&s_trans, props, lv_anim_path_ease_in_out, 80, 0, NULL);
    s_trans_init = true;
}

#define BTN_RADIUS  10

// Brand accent border colour, used on every button.
static const lv_color_t BTN_BORDER_COLOR = lv_color_make(0xDF, 0xA8, 0x4A);

// "Soft depth" button treatment shared by every button in the app: a
// vertical gradient + drop shadow so buttons read as raised/tappable
// instead of flat rectangles, and flatten to a solid fill with almost no
// shadow when pressed so they feel like they've been pushed in.
static void style_button(lv_obj_t *btn, lv_color_t top, lv_color_t bottom, lv_color_t pressed) {
    lv_obj_set_style_radius(btn, BTN_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, top, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(btn, bottom, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, BTN_BORDER_COLOR, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(btn, 3, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn, lv_color_black(), LV_PART_MAIN);

    lv_obj_set_style_bg_color(btn, pressed, LV_STATE_PRESSED | LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(btn, pressed, LV_STATE_PRESSED | LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 4, LV_STATE_PRESSED | LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(btn, 1, LV_STATE_PRESSED | LV_PART_MAIN);

    lv_obj_set_style_transition(btn, &s_trans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transition(btn, &s_trans, LV_PART_MAIN | LV_STATE_PRESSED);
}

// ---------------------------------------------------------------------------
// Preset screen callbacks
// ---------------------------------------------------------------------------

static void preset_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[UI] Preset %d tapped\n", id);
    wled_activate_preset(id);
}

// Battery icon: hidden when no battery is detected (always the case on
// this board — see battery.cpp), otherwise refreshed periodically by a
// shared LVGL timer (created once, outlives screen rebuilds — always
// checked against the current lbl_battery pointer).
static lv_timer_t *battery_timer = nullptr;

static const char *battery_symbol_for_percent(int pct) {
    if (pct < 0)   return NULL;  // no battery detected
    if (pct >= 80) return LV_SYMBOL_BATTERY_FULL;
    if (pct >= 55) return LV_SYMBOL_BATTERY_3;
    if (pct >= 30) return LV_SYMBOL_BATTERY_2;
    if (pct >= 10) return LV_SYMBOL_BATTERY_1;
    return LV_SYMBOL_BATTERY_EMPTY;
}

static void battery_timer_cb(lv_timer_t * /*timer*/) {
    if (!lbl_battery) return;
    const char *sym = battery_symbol_for_percent(battery_get_percent());
    if (!sym) {
        lv_obj_add_flag(lbl_battery, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(lbl_battery, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl_battery, sym);
}

// Refresh button: re-fetch the preset list from WLED and rebuild the screen
static WledPreset s_refresh_presets[WLED_MAX_PRESETS];

static void refresh_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    Serial.println("[UI] Refresh presets tapped");
    int count = 0;
    if (wled_fetch_presets(s_refresh_presets, count)) {
        ui_show_presets(s_refresh_presets, count);
    } else {
        ui_show_status("No presets found.\nCreate presets in the\nWLED web UI first.");
    }
}

// Gesture on the presets container: swipe right → colour picker
static void presets_gesture_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_RIGHT) {
        ui_show_color_picker();
    }
}

// ---------------------------------------------------------------------------
// Header long-press → reset WiFi/WLED config
//
// v9's lv_msgbox API is a builder, not a single call with a button-map
// string like v8's — each footer button is created individually and gets
// its own click callback, rather than one VALUE_CHANGED handler reading
// "which button was active".
// ---------------------------------------------------------------------------

// Shared by both footer buttons: hide the backdrop synchronously for
// instant visual feedback and to stop it receiving input, then close for
// real via lv_msgbox_close_async (which — verified in lv_msgbox.c — already
// deletes the auto-created backdrop itself when the msgbox has the
// AUTO_PARENT flag, i.e. was created with a NULL parent, as this one is).
// Actually freeing synchronously here would delete the object from within
// its own event handler, which corrupts LVGL's indev press-tracking and
// leaves the touchscreen unresponsive afterward.
static void dismiss_reset_dialog(lv_obj_t *mbox) {
    lv_obj_t *backdrop = lv_obj_get_parent(mbox);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL);  // force an immediate render so the dialog visibly
                         // disappears before the blocking call below
    lv_msgbox_close_async(mbox);
}

static void reset_cancel_cb(lv_event_t *e) {
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
    dismiss_reset_dialog(mbox);
}

static void reset_confirm_cb(lv_event_t *e) {
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
    dismiss_reset_dialog(mbox);

    Serial.println("[UI] Reset confirmed, opening reconfigure portal");
    if (wifi_setup_reconfigure()) {
        ESP.restart();
    } else if (scr_presets) {
        lv_screen_load(scr_presets);
    }
}

static void header_long_press_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;

    // The finger that triggered this long-press is still down when the
    // dialog appears under it. LVGL re-targets a held press onto whatever
    // object is now underneath it, so without this, releasing that same
    // press instantly "clicks" whatever's there — no deliberate second tap
    // needed. Same idiom as the screen-wake case in main.cpp.
    lv_indev_wait_release(lv_indev_active());

    lv_obj_t *mbox = lv_msgbox_create(NULL);  // NULL parent = auto modal backdrop, self-centers
    lv_obj_set_width(mbox, DISPLAY_WIDTH - 20);
    lv_msgbox_add_title(mbox, "Reset WiFi & WLED config?");
    lv_msgbox_add_text(mbox, "This restarts the device.\n\nFirmware: " FIRMWARE_VERSION);

    lv_obj_t *cancel_btn = lv_msgbox_add_footer_button(mbox, "Cancel");
    lv_obj_t *reset_btn  = lv_msgbox_add_footer_button(mbox, "Reset & Restart");
    lv_obj_add_event_cb(cancel_btn, reset_cancel_cb, LV_EVENT_CLICKED, mbox);
    lv_obj_add_event_cb(reset_btn, reset_confirm_cb, LV_EVENT_CLICKED, mbox);
}

// ---------------------------------------------------------------------------
// Colour picker callbacks — RGB sliders replace v8's lv_colorwheel, which
// LVGL v9.5.0 doesn't ship (confirmed: no widgets/colorwheel directory,
// no reference anywhere in the installed package source).
// ---------------------------------------------------------------------------

static void update_swatch() {
    if (!color_swatch || !slider_r || !slider_g || !slider_b) return;
    uint8_t r = (uint8_t)lv_slider_get_value(slider_r);
    uint8_t g = (uint8_t)lv_slider_get_value(slider_g);
    uint8_t b = (uint8_t)lv_slider_get_value(slider_b);
    lv_obj_set_style_bg_color(color_swatch, lv_color_make(r, g, b), LV_PART_MAIN);
}

static void slider_r_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_label_set_text_fmt(lbl_r, "Red   %d", (int)lv_slider_get_value(slider_r));
    update_swatch();
}
static void slider_g_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_label_set_text_fmt(lbl_g, "Green %d", (int)lv_slider_get_value(slider_g));
    update_swatch();
}
static void slider_b_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_label_set_text_fmt(lbl_b, "Blue  %d", (int)lv_slider_get_value(slider_b));
    update_swatch();
}

// Send button: read all three sliders → POST to WLED
static void color_send_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!slider_r || !slider_g || !slider_b) return;
    uint8_t r = (uint8_t)lv_slider_get_value(slider_r);
    uint8_t g = (uint8_t)lv_slider_get_value(slider_g);
    uint8_t b = (uint8_t)lv_slider_get_value(slider_b);
    Serial.printf("[UI] Send colour rgb(%d,%d,%d)\n", r, g, b);
    wled_set_color(r, g, b);
}

// Back button / swipe left → return to presets
static void color_back_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (scr_presets) lv_screen_load(scr_presets);
}

static void color_gesture_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT) {
        if (scr_presets) lv_screen_load(scr_presets);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ui_show_status(const char *msg) {
    if (!scr_status) {
        scr_status = make_screen(lv_color_black());
        lv_obj_add_flag(scr_status, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(scr_status, header_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);

        // No logo image on this port (see Task 7 in the plan — the v8
        // logo_mark image descriptor doesn't compile against v9's reworked
        // image struct, and regenerating it isn't worth it for a purely
        // decorative element). Title starts near the top instead of below
        // a 96px graphic.
        lv_obj_t *lbl_title = lv_label_create(scr_status);
        lv_label_set_text(lbl_title, "DMX Engine Touch Panel");
        lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl_title, DISPLAY_WIDTH - 20);
        lv_obj_set_style_text_color(lbl_title, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 16);

        lbl_status = lv_label_create(scr_status);
        lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl_status, DISPLAY_WIDTH - 20);
        lv_obj_set_style_text_color(lbl_status, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 16 + 24 + 8);
    }
    lv_label_set_text(lbl_status, msg);
    if (lv_screen_active() != scr_status) lv_screen_load(scr_status);
}

void ui_show_presets(const WledPreset *presets, int count) {
    if (scr_presets) {
        lv_obj_delete(scr_presets);
        scr_presets = nullptr;
        lbl_battery = nullptr;
    }

    ensure_trans();
    scr_presets = make_screen(lv_color_make(0x1C, 0x1C, 0x1E));

    // --- Header ---
    lv_obj_t *header = lv_obj_create(scr_presets);
    lv_obj_set_size(header, DISPLAY_WIDTH, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_make(0x26, 0x27, 0x2A), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Lighting Preset");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(title);

    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(header, header_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);

    // Swipe hint in corner
    lv_obj_t *hint = lv_label_create(header);
    lv_label_set_text(hint, ">");
    lv_obj_set_style_text_color(hint, lv_color_make(0x60, 0x60, 0x60), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_RIGHT_MID, -8, 0);

    // Battery icon — hidden until the first reading confirms a battery is
    // actually attached (never happens on this board, but the mechanism
    // stays identical to LVGL_wled's so ui.cpp doesn't special-case it)
    lbl_battery = lv_label_create(header);
    lv_obj_set_style_text_color(lbl_battery, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(lbl_battery, LV_ALIGN_RIGHT_MID, -24, 0);
    lv_obj_add_flag(lbl_battery, LV_OBJ_FLAG_HIDDEN);
    if (!battery_timer) {
        battery_timer = lv_timer_create(battery_timer_cb, 5000, NULL);
    }
    battery_timer_cb(NULL);  // populate immediately, don't wait for the first tick

    // --- Scrollable preset grid ---
    lv_obj_t *cont = lv_obj_create(scr_presets);
    lv_obj_set_size(cont, DISPLAY_WIDTH, DISPLAY_HEIGHT - HEADER_H - REFRESH_BAR_H - 16);
    lv_obj_set_pos(cont, 0, HEADER_H);
    lv_obj_set_style_bg_color(cont, lv_color_make(0x1C, 0x1C, 0x1E), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, BTN_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(cont, BTN_GAP, LV_PART_MAIN);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);   // vertical scroll only → horizontal = gesture

    // Bubble gestures up to scr_presets
    lv_obj_add_flag(cont, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(scr_presets, presets_gesture_cb, LV_EVENT_GESTURE, NULL);

    if (count == 0) {
        lv_obj_t *empty = lv_label_create(cont);
        lv_label_set_text(empty, "No presets found");
        lv_obj_set_style_text_color(empty, lv_color_make(0x80, 0x80, 0x80), LV_PART_MAIN);
        lv_obj_center(empty);
    }

    for (int i = 0; i < count; i++) {
        lv_obj_t *btn = lv_obj_create(cont);
        lv_obj_set_size(btn, BTN_W, BTN_H);
        style_button(btn,
                     lv_color_make(0x5B, 0x5D, 0x60), lv_color_make(0x44, 0x46, 0x48),
                     lv_color_make(0xA9, 0x76, 0x2A));
        lv_obj_set_style_pad_all(btn, 4, LV_PART_MAIN);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);

        lv_obj_add_event_cb(btn, preset_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)presets[i].id);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, presets[i].name);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, BTN_W - 8);
        lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_center(lbl);
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_GESTURE_BUBBLE);
    }

    // --- Refresh bar ---
    lv_obj_t *refresh_btn = lv_button_create(scr_presets);
    lv_obj_set_size(refresh_btn, DISPLAY_WIDTH - 16, REFRESH_BAR_H);
    lv_obj_align(refresh_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    style_button(refresh_btn,
                 lv_color_make(0x5B, 0x5D, 0x60), lv_color_make(0x44, 0x46, 0x48),
                 lv_color_make(0xA9, 0x76, 0x2A));
    lv_obj_add_event_cb(refresh_btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *refresh_lbl = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_lbl, LV_SYMBOL_REFRESH " Refresh");
    lv_obj_set_style_text_color(refresh_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(refresh_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(refresh_lbl);

    lv_screen_load(scr_presets);
}

void ui_show_color_picker() {
    if (scr_color) {
        lv_obj_delete(scr_color);
        scr_color = nullptr;
        slider_r = slider_g = slider_b = nullptr;
        lbl_r = lbl_g = lbl_b = nullptr;
        color_swatch = nullptr;
    }

    ensure_trans();
    const lv_color_t dark = lv_color_make(0x1C, 0x1C, 0x1E);
    scr_color = make_screen(dark);

    // Bubble gestures on the screen itself for swipe-left-to-back
    lv_obj_add_event_cb(scr_color, color_gesture_cb, LV_EVENT_GESTURE, NULL);

    // --- Header ---
    lv_obj_t *header = lv_obj_create(scr_color);
    lv_obj_set_size(header, DISPLAY_WIDTH, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_make(0x26, 0x27, 0x2A), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(header, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Back button
    lv_obj_t *back_btn = lv_button_create(header);
    lv_obj_set_size(back_btn, 50, HEADER_H - 8);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 4, 0);
    style_button(back_btn,
                 lv_color_make(0x5B, 0x5D, 0x60), lv_color_make(0x44, 0x46, 0x48),
                 lv_color_make(0xA9, 0x76, 0x2A));
    lv_obj_add_event_cb(back_btn, color_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(back_lbl);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Colour");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(title);

    // --- RGB sliders ---
    lbl_r = lv_label_create(scr_color);
    lv_label_set_text(lbl_r, "Red   0");
    lv_obj_set_style_text_color(lbl_r, lv_color_make(0xFF, 0x80, 0x80), LV_PART_MAIN);
    lv_obj_set_pos(lbl_r, CONTENT_PAD, ROW_LABEL_Y_R);

    slider_r = lv_slider_create(scr_color);
    lv_obj_set_size(slider_r, DISPLAY_WIDTH - CONTENT_PAD * 2, SLIDER_H);
    lv_obj_set_pos(slider_r, CONTENT_PAD, ROW_SLIDER_Y_R);
    lv_slider_set_range(slider_r, 0, 255);
    lv_slider_set_value(slider_r, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_r, slider_r_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(slider_r, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lbl_g = lv_label_create(scr_color);
    lv_label_set_text(lbl_g, "Green 0");
    lv_obj_set_style_text_color(lbl_g, lv_color_make(0x80, 0xFF, 0x80), LV_PART_MAIN);
    lv_obj_set_pos(lbl_g, CONTENT_PAD, ROW_LABEL_Y_G);

    slider_g = lv_slider_create(scr_color);
    lv_obj_set_size(slider_g, DISPLAY_WIDTH - CONTENT_PAD * 2, SLIDER_H);
    lv_obj_set_pos(slider_g, CONTENT_PAD, ROW_SLIDER_Y_G);
    lv_slider_set_range(slider_g, 0, 255);
    lv_slider_set_value(slider_g, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_g, slider_g_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(slider_g, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lbl_b = lv_label_create(scr_color);
    lv_label_set_text(lbl_b, "Blue  0");
    lv_obj_set_style_text_color(lbl_b, lv_color_make(0x80, 0x80, 0xFF), LV_PART_MAIN);
    lv_obj_set_pos(lbl_b, CONTENT_PAD, ROW_LABEL_Y_B);

    slider_b = lv_slider_create(scr_color);
    lv_obj_set_size(slider_b, DISPLAY_WIDTH - CONTENT_PAD * 2, SLIDER_H);
    lv_obj_set_pos(slider_b, CONTENT_PAD, ROW_SLIDER_Y_B);
    lv_slider_set_range(slider_b, 0, 255);
    lv_slider_set_value(slider_b, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_b, slider_b_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(slider_b, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // --- Swatch preview ---
    color_swatch = lv_obj_create(scr_color);
    lv_obj_set_size(color_swatch, DISPLAY_WIDTH - CONTENT_PAD * 2, SWATCH_H);
    lv_obj_set_pos(color_swatch, CONTENT_PAD, SWATCH_Y);
    lv_obj_set_style_radius(color_swatch, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(color_swatch, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(color_swatch, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_flag(color_swatch, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // --- Send button ---
    lv_obj_t *send_btn = lv_button_create(scr_color);
    lv_obj_set_size(send_btn, DISPLAY_WIDTH - CONTENT_PAD * 2, SEND_BTN_H);
    lv_obj_set_pos(send_btn, CONTENT_PAD, SEND_BTN_Y);
    style_button(send_btn,
                 lv_color_make(0x1E, 0x8A, 0xFF), lv_color_make(0x00, 0x5A, 0xD1),
                 lv_color_make(0xA9, 0x76, 0x2A));
    lv_obj_add_event_cb(send_btn, color_send_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *send_lbl = lv_label_create(send_btn);
    lv_label_set_text(send_lbl, "Send to WLED");
    lv_obj_set_style_text_color(send_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(send_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(send_lbl);

    lv_screen_load(scr_color);
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add src/ui.h src/ui.cpp
git commit -m "Migrate ui.cpp to LVGL v9: renamed calls, v9 msgbox builder API, RGB sliders replacing colorwheel"
```

(No compile check yet — `main.cpp` doesn't call into `ui.cpp` until Task 9. Verified together there.)

---

### Task 9: Wire up the real `main.cpp`

**Files:**
- Modify: `src/main.cpp` (replaces Task 6's smoke-test content)

- [ ] **Step 1: Replace `src/main.cpp`**

```c
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
```

- [ ] **Step 2: Compile**

Run: `cd /Users/stephenholmes/Documents/GitHub/CYD_wled && pio run`
Expected: `[SUCCESS]`. If there are errors, they'll be in `ui.cpp`, `wled.cpp`, or `wifi_setup.cpp` — cross-check the failing line against the v8→v9 migration table in the spec (`docs/superpowers/specs/2026-08-15-cyd-wled-port-design.md`) and this task's Step 1 code above, which are the two files most likely to have a typo'd v9 symbol.

- [ ] **Step 3: Commit**

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add src/main.cpp
git commit -m "Wire up real main.cpp: WiFi connect, fetch presets, show UI, power-save loop"
```

---

### Task 10: Update `platformio.ini` port settings if needed

**Files:**
- Modify: `platformio.ini` (only if the port differs)

- [ ] **Step 1: Confirm the CYD board's serial port**

Run: `pio device list`
Expected: a CH340-style entry (`VID:PID=1A86:7523`) — likely still `/dev/cu.usbserial-1110`, inherited from the base CYD project when CYD_wled was cloned, but confirm since a different USB port/cable can change the device name on macOS.

- [ ] **Step 2: If the port differs, update it**

`platformio.ini` currently has:
```ini
upload_port = /dev/cu.usbserial-1110
monitor_port = /dev/cu.usbserial-1110
```

Update both to match Step 1's actual port if different, then commit:
```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add platformio.ini
git commit -m "Update serial port for this machine"
```

(Skip the commit if the port already matched — nothing changed.)

---

### Task 11: Full on-device verification

**Files:** none (verification only)

- [ ] **Step 1: Flash and boot**

Run: `cd /Users/stephenholmes/Documents/GitHub/CYD_wled && pio run -t upload && pio device monitor`
Expected: boot log shows `[BOOT] CYD_wled starting` → `[DISPLAY] Ready...` → `[TOUCH] XPT2046 initialized` → `[BOOT] Ready` (or the WiFi/preset-fetch status lines below if WiFi isn't yet configured).

- [ ] **Step 2: First-boot WiFi setup portal**

If this is the first boot (or after an NVS erase), the screen should show "Setup Mode / Join WiFi: Touch_Panel_AP / ...". From a phone, join the `Touch_Panel_AP` open network, follow the captive-portal prompts, enter your real WiFi credentials and your WLED instance's IP/hostname (defaults to the `WLED_HOST` seed value from `config.h` if left blank), save. Expected: device restarts and connects to your WiFi.

- [ ] **Step 3: Preset grid**

Once connected to a reachable WLED instance, expected: the screen shows "Lighting Preset" header with a scrollable grid of preset buttons (or "No presets found" if that WLED instance has none defined yet — create one via the WLED web UI and use the Refresh button to confirm it appears). Tap a preset button; confirm the actual WLED device changes to that preset.

- [ ] **Step 4: Colour picker**

Swipe right from the presets screen (or watch for the ">" hint in the header). Expected: the Colour screen appears with three sliders (Red/Green/Blue) and a swatch. Drag each slider; confirm the swatch updates live and the label above each slider shows the current 0–255 value. Tap "Send to WLED"; confirm the actual light changes to that colour. Swipe left (or tap the back arrow) to return to presets.

- [ ] **Step 5: Reset dialog**

Long-press the header on the presets screen. Expected: a "Reset WiFi & WLED config?" dialog with "Cancel" and "Reset & Restart" buttons. Tap Cancel; confirm the dialog closes cleanly and the presets screen is still fully responsive (tap a preset button afterward to confirm touch input wasn't left in a stuck state). Long-press again, tap "Reset & Restart"; confirm it opens the reconfigure portal (180s timeout) rather than immediately restarting.

- [ ] **Step 6: Screen timeout**

Leave the device idle past `SCREEN_TIMEOUT_MS` (20s by default). Expected: backlight turns off, content still rendered underneath. Tap the screen; expected: backlight comes back on and that waking tap does *not* also register as a click on whatever's underneath it.

- [ ] **Step 7: Final commit if Step 2's WiFi/host defaults were adjusted**

Only if `config.h`'s `WLED_HOST` seed default needs changing for your actual setup (not required — the portal overrides it at runtime, but worth updating so a fresh NVS erase seeds the right value):

```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git add include/config.h
git commit -m "Update WLED_HOST seed default"
git push
```

Otherwise just push whatever's already committed:
```bash
cd /Users/stephenholmes/Documents/GitHub/CYD_wled
git push
```
