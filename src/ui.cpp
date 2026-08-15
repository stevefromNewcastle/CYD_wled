#include "ui.h"
#include "config.h"
#include "wled.h"
#include "wifi_setup.h"
#include "battery.h"
#include "logo_touch.h"
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

        // Logo + title are kept compact (small top margin, tight gaps) so
        // the longest status message ("Setup Mode" + WiFi instructions,
        // 8 lines) still fits below them on a 320px-tall screen.
        lv_obj_t *logo = lv_image_create(scr_status);
        lv_image_set_src(logo, &logo_touch);
        lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 8);

        lv_obj_t *lbl_title = lv_label_create(scr_status);
        lv_label_set_text(lbl_title, "DMX Engine Touch Panel");
        lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl_title, DISPLAY_WIDTH - 20);
        lv_obj_set_style_text_color(lbl_title, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 8 + 96 + 8);

        lbl_status = lv_label_create(scr_status);
        lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl_status, DISPLAY_WIDTH - 20);
        lv_obj_set_style_text_color(lbl_status, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 8 + 96 + 8 + 24 + 8);
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
