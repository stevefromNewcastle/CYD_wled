# Port LVGL_wled to the ESP32 CYD — Design

Date: 2026-08-15

## Problem

`LVGL_wled` (in the sibling `LVGL_wled` repo) is a working WLED preset/colour
remote built for the Waveshare ESP32-S3-Touch-LCD-2.8: LovyanGFX driving an
ST7789 panel, a custom I2C driver for a CST-series capacitive touch chip,
LVGL v8.3.11, and battery/power-latch handling for that board's LiPo circuit.

`CYD_wled` targets different hardware entirely — the ESP32-2432S028R "Cheap
Yellow Display": plain ESP32, ILI9341 panel via TFT_eSPI, resistive XPT2046
touch via SPI, LVGL v9.2 (via TFT_eSPI's `lv_tft_espi_create()` porting
layer, already validated in the base CYD project), no battery, no
power-latch circuit.

This ports LVGL_wled's WLED remote functionality onto that hardware.

## Decisions made during brainstorming

- **Orientation: portrait (240×320)**, matching LVGL_wled's layout as-is.
  Landscape would require re-laying out every screen; not worth it for a
  first port.
- **Battery UI: stubbed, not removed.** `battery.cpp/h` keeps the same API;
  `battery_get_percent()` always returns `-1` ("no battery present"). The
  header icon logic in `ui.cpp` already hides itself on `-1` — zero changes
  needed there.
- **LVGL version: port to v9** (not pin to v8.3.11), to stay on the same
  LVGL major version as the already-built-and-flashed base CYD project.
  This is the higher-risk option of the two considered — it means
  translating ui.cpp's ~480 lines of v8 API calls — but keeps one LVGL
  version across both CYD projects going forward.
- **Colour picker: RGB sliders, not a colour wheel.** LVGL v9.5.0 does not
  ship the colorwheel widget at all (confirmed by inspecting the installed
  package — no `widgets/colorwheel` directory, no reference to it anywhere
  in the source tree). Three `lv_slider`s (R/G/B, 0–255) with a live swatch
  replace it. `wled_set_color(r, g, b)` already takes exactly this shape.

## Architecture

Same six-module split as LVGL_wled:

| Module | Fate |
|---|---|
| `config.h` | Rewritten for CYD pins; drops `POWER_HOLD_PIN` and battery ADC pins |
| `display.cpp/h` | Rewritten: TFT_eSPI + `lv_tft_espi_create()` |
| `touch.cpp/h` | Rewritten: XPT2046 SPI polling |
| `battery.cpp/h` | Stubbed — always reports "no battery" |
| `ui.cpp/h` | Migrated v8→v9 (see below) |
| `wled.cpp/h`, `wifi_setup.cpp/h`, `settings.cpp/h`, `logo_mark.cpp/h` | Unchanged |
| `main.cpp` | Adapted setup/loop, power-latch code removed |

## `config.h`

```
FIRMWARE_VERSION                unchanged (__DATE__ " " __TIME__)

WLED_HOST / WLED_PORT           unchanged (192.168.20.98:80, seed default only)
WLED_MAX_PRESETS                unchanged (64)

DISPLAY_WIDTH / HEIGHT          240 / 320 (unchanged — CYD panel is also 240×320 native)
DISPLAY_ROTATION                0 (portrait)
DISPLAY_BL_PIN                  21   (CYD's TFT_BL, was 5)
SCREEN_TIMEOUT_MS                unchanged (20000)

Touch (XPT2046, SPI — replaces the I2C CST block entirely):
  XPT2046_IRQ    36
  XPT2046_MOSI   32
  XPT2046_MISO   39
  XPT2046_CLK    25
  XPT2046_CS     33

POWER_HOLD_PIN                  removed — CYD is USB/5V powered, no latch circuit
BATTERY_* pins/constants         removed — no ADC divider on this board
```

## `display.cpp` / `display.h`

Same public API as LVGL_wled (`display_init()`, `display_set_backlight(bool)`),
minus the exported `display_flush()` — not needed, since
`lv_tft_espi_create()` (TFT_eSPI's own LVGL v9 porting layer, already proven
in the base CYD project's `main.cpp`) owns the flush callback internally.
This makes `display.cpp` *shorter* than LVGL_wled's LovyanGFX version, not
longer:

```c
void display_init() {
    lv_init();
    static uint32_t draw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT / 10 * 2 / 4];
    lv_display_t *disp = lv_tft_espi_create(DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                             draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);  // portrait

    pinMode(DISPLAY_BL_PIN, OUTPUT);
    digitalWrite(DISPLAY_BL_PIN, HIGH);  // TFT_BACKLIGHT_ON is HIGH on this board
}

void display_set_backlight(bool on) {
    digitalWrite(DISPLAY_BL_PIN, on ? HIGH : LOW);
}
```

No PWM/dimming — LVGL_wled's `setBrightness(255/0)` was already just on/off
in practice (`display_set_backlight` is only ever called with a plain
bool), so a digital pin matches existing behaviour exactly.

**Open item:** `LV_DISPLAY_ROTATION_0` is the starting assumption for
portrait; TFT_eSPI's own `tft.setRotation()` equivalent inside the porting
layer may need a 180°-flipped value depending on which way the panel is
upright in this orientation. Verified on-device during bring-up, not
something to guess from source alone.

## `touch.cpp` / `touch.h`

Same public API as LVGL_wled (`touch_init()`, `touch_read()`), signature
updated to v9's indev callback shape
(`void touch_read(lv_indev_t *indev, lv_indev_data_t *data)` — matches
the base CYD project's already-working `touchscreen_read()`).

Body follows the base CYD project's XPT2046 polling approach (`tirqTouched()`
+ `touched()` + `getPoint()` + `map()`), not LVGL_wled's I2C register
parsing.

**Open item:** the base project's `map()` ranges (`200–3700` / `240–3800`)
and `touchscreen.setRotation(2)` were tuned for *landscape*
(`LV_DISPLAY_ROTATION_270`). Portrait needs its own calibration — same raw
ADC ranges likely still apply (they're a property of the resistive panel,
not the rotation), but the axis mapping and `setRotation()` value need
re-deriving and will likely need a small on-device nudge after first flash,
same as any resistive-touch bring-up.

## `battery.cpp` / `battery.h`

```c
void battery_init() { /* no-op — no battery hardware on this board */ }
int  battery_get_percent() { return -1; }
```

Header comment updated to say why, so it doesn't read as an unfinished
stub. `ui.cpp`'s battery-icon code is untouched — it already hides the icon
on `-1`.

## `ui.cpp` — v8 → v9 migration

Renames (mechanical, behaviour-preserving):

| v8 | v9 |
|---|---|
| `lv_scr_act()` | `lv_screen_active()` |
| `lv_scr_load(x)` | `lv_screen_load(x)` |
| `lv_btn_create(p)` | `lv_button_create(p)` |
| `lv_obj_del(x)` | `lv_obj_delete(x)` |
| `lv_indev_get_act()` | `lv_indev_active()` |
| `lv_task_handler()` (called from `main.cpp`) | `lv_timer_handler()` |
| `lv_color_to32(c)` → `.full`/`.ch.red` etc. | `lv_color_to_32(c, LV_OPA_COVER)` → flat `.red`/`.green`/`.blue` fields (v9's `lv_color32_t` has no union) |

Everything else already confirmed present and unchanged in the installed
v9.5.0 package: `lv_style_transition_dsc_init`, `LV_OBJ_FLAG_GESTURE_BUBBLE`,
`lv_indev_get_gesture_dir`, `lv_indev_wait_release`,
`lv_display_get_inactive_time` (was `lv_disp_get_inactive_time`, takes
`lv_display_t*` now), `LV_SYMBOL_BATTERY_*`, flex layout, sliders, msgbox
and image widgets (all already enabled in this project's `lv_conf.h`).

**Message box — real rework, not a rename.** v9's msgbox is a builder API:

```c
// v8 (LVGL_wled today):
lv_obj_t *mbox = lv_msgbox_create(NULL, "Reset WiFi & WLED config?",
                                   "This restarts the device...",
                                   reset_confirm_btns, false);
// ...then one LV_EVENT_VALUE_CHANGED handler reads
// lv_msgbox_get_active_btn_text() to see which button fired.

// v9 (CYD_wled):
lv_obj_t *mbox = lv_msgbox_create(NULL);
lv_msgbox_add_title(mbox, "Reset WiFi & WLED config?");
lv_msgbox_add_text(mbox, "This restarts the device.\n\nFirmware: " FIRMWARE_VERSION);
lv_obj_t *cancel_btn = lv_msgbox_add_footer_button(mbox, "Cancel");
lv_obj_t *reset_btn  = lv_msgbox_add_footer_button(mbox, "Reset & Restart");
lv_obj_add_event_cb(cancel_btn, cancel_cb, LV_EVENT_CLICKED, mbox);
lv_obj_add_event_cb(reset_btn,  reset_cb,  LV_EVENT_CLICKED, mbox);
```

Two separate per-button click callbacks replace the old single
"active-button-text" handler. The backdrop-hide-then-`lv_msgbox_close_async`
dance (needed in v8 so a held touch doesn't re-target onto whatever's
underneath) carries over conceptually — re-verified against v9's actual
close/backdrop behaviour while implementing, since the internals changed
even though `lv_msgbox_close_async()` still exists.

**Colour picker — widget swap, not just an API change.** Replaces
`lv_colorwheel_create()` / `lv_colorwheel_get_rgb()` with three
`lv_slider`s:

```c
lv_obj_t *r_slider = lv_slider_create(scr_color);
lv_slider_set_range(r_slider, 0, 255);
// ...same for g_slider, b_slider
// on each LV_EVENT_VALUE_CHANGED: read all three, update swatch bg colour
// "Send to WLED" button reads all three slider values → wled_set_color(r,g,b)
```

Same screen structure otherwise (header with back button + swipe-left,
swatch preview, send button) — only the input control changes.

## `wled.cpp/h`, `wifi_setup.cpp/h`, `settings.cpp/h`, `logo_mark.cpp/h`

Copied unchanged. No LVGL or hardware-specific code in any of them — WiFi,
HTTPClient, ArduinoJson, WiFiManager, Preferences (NVS) are all
board-agnostic ESP32/Arduino APIs.

One change: `settings.cpp`'s NVS namespace (`"lvglwled"`) is renamed to
`"cydwled"`. Not functionally required (separate device, separate flash),
but avoids the namespace looking like a copy-paste leftover.

## `main.cpp`

Same shape as LVGL_wled's `setup()`/`loop()`, minus the power-latch block
(`pinMode(POWER_HOLD_PIN, ...)` / `digitalWrite(POWER_HOLD_PIN, HIGH)` at
the very top of `setup()` — deleted, not replaced, since this board has
no such circuit) and using v9-style indev registration
(`lv_indev_create()` / `lv_indev_set_read_cb()`, as already proven in the
base CYD project) instead of v8's `lv_indev_drv_t` dance.

## `platformio.ini`

Add to existing `lib_deps`:
```
tzapu/WiFiManager@^2.0.17
bblanchon/ArduinoJson@^6.21.3
```
(TFT_eSPI, XPT2046_Touchscreen, lvgl stay as already configured in
CYD_wled.)

## Testing approach

Hardware-dependent (display, touch, WiFi, a real WLED instance) — not
unit-testable. Verification plan:

1. `pio run` compiles clean on this toolchain (same ESP32 core/TFT_eSPI/lvgl
   versions already validated for the base CYD project).
2. Flash and confirm: boot screen renders in portrait; WiFi setup portal
   ("Touch_Panel_AP") reachable on first boot / after NVS erase.
3. Point at a real WLED instance: presets fetch and render as a scrollable
   grid; tapping a preset activates it on the actual device.
4. Swipe to colour picker: drag each RGB slider, confirm swatch updates
   live; "Send to WLED" changes the actual light colour.
5. Long-press header → confirm dialog → both buttons (Cancel, Reset &
   Restart) behave correctly; confirm the backdrop doesn't get stuck.
6. Idle past `SCREEN_TIMEOUT_MS` → backlight goes off; touch wakes it
   without the waking tap also registering as a click.
7. Touch calibration pass: tap all four corners and the centre, adjust the
   portrait mapping constants in `touch.cpp` if off.
