#pragma once

#include "lvgl.h"
#include "ui.h"

#define UI_COLOR_BACKGROUND 0x000000
#define UI_COLOR_TEXT 0xFFFFFF

typedef enum {
    UI_FILL_NONE,
    UI_FILL_RED,
    UI_FILL_ORANGE,
} ui_fill_t;

typedef struct {
    lv_obj_t *obj;
    lv_timer_t *flash_timer;
} ui_pad_t;

void ui_pad_styles_init(void);

void ui_pad_create(ui_pad_t *pad, lv_obj_t *parent);

void ui_pad_set_fill(ui_pad_t *pad, ui_fill_t fill);

void ui_pad_flash(ui_pad_t *pad, looper_outcome_t outcome, looper_gesture_t gesture);

void ui_main_create(lv_obj_t *screen, const ui_callbacks_t *callbacks);

void ui_main_set_looper(const looper_t *looper);

void ui_main_set_can_send(bool can_send);

void ui_main_set_battery(const battery_status_t *battery);

void ui_setup_create(lv_obj_t *screen, const ui_callbacks_t *callbacks);
