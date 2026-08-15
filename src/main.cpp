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
    lv_tick_inc(5);
    delay(5);
}
