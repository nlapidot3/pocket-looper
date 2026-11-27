#include "ui_internal.h"

#define FLASH_MS 180
#define PAD_RADIUS 20

#define COLOR_PAD 0x1F1F1F
#define COLOR_PAD_PRESSED 0x3A3A3A
#define COLOR_TAP_FLASH 0x00C853
#define COLOR_LONG_PRESS_FLASH 0xFF6D00
#define COLOR_DENIED_FLASH 0x7F1D1D
#define COLOR_FILL_RED 0xD32F2F
#define COLOR_FILL_ORANGE 0xF57C00

#define STATE_TAP_FLASH LV_STATE_USER_1
#define STATE_LONG_PRESS_FLASH LV_STATE_USER_2
#define STATE_DENIED_FLASH LV_STATE_USER_3

static lv_style_t s_pad_style;
static lv_style_t s_fill_red_style;
static lv_style_t s_fill_orange_style;
static lv_style_t s_pressed_style;
static lv_style_t s_tap_flash_style;
static lv_style_t s_long_press_flash_style;
static lv_style_t s_denied_flash_style;

static void init_color_style(lv_style_t *style, uint32_t color)
{
    lv_style_init(style);
    lv_style_set_bg_color(style, lv_color_hex(color));
}

static void on_flash_done(lv_timer_t *timer)
{
    ui_pad_t *pad = lv_timer_get_user_data(timer);
    lv_obj_remove_state(pad->obj, STATE_TAP_FLASH | STATE_LONG_PRESS_FLASH | STATE_DENIED_FLASH);
    lv_timer_pause(timer);
}

void ui_pad_styles_init(void)
{
    lv_style_init(&s_pad_style);
    lv_style_set_bg_opa(&s_pad_style, LV_OPA_COVER);
    lv_style_set_bg_color(&s_pad_style, lv_color_hex(COLOR_PAD));
    lv_style_set_radius(&s_pad_style, PAD_RADIUS);

    init_color_style(&s_fill_red_style, COLOR_FILL_RED);
    init_color_style(&s_fill_orange_style, COLOR_FILL_ORANGE);
    init_color_style(&s_pressed_style, COLOR_PAD_PRESSED);
    init_color_style(&s_tap_flash_style, COLOR_TAP_FLASH);
    init_color_style(&s_long_press_flash_style, COLOR_LONG_PRESS_FLASH);
    init_color_style(&s_denied_flash_style, COLOR_DENIED_FLASH);
}

/* Styles added later take precedence, so the order here is the priority from the idle colour up to the flashes. */
void ui_pad_create(ui_pad_t *pad, lv_obj_t *parent)
{
    pad->obj = lv_button_create(parent);
    lv_obj_remove_style_all(pad->obj);
    lv_obj_add_style(pad->obj, &s_pad_style, LV_STATE_DEFAULT);
    lv_obj_add_style(pad->obj, &s_fill_red_style, LV_STATE_DEFAULT);
    lv_obj_add_style(pad->obj, &s_fill_orange_style, LV_STATE_DEFAULT);
    lv_obj_add_style(pad->obj, &s_pressed_style, LV_STATE_PRESSED);
    lv_obj_add_style(pad->obj, &s_tap_flash_style, STATE_TAP_FLASH);
    lv_obj_add_style(pad->obj, &s_long_press_flash_style, STATE_LONG_PRESS_FLASH);
    lv_obj_add_style(pad->obj, &s_denied_flash_style, STATE_DENIED_FLASH);
    ui_pad_set_fill(pad, UI_FILL_NONE);

    pad->flash_timer = lv_timer_create(on_flash_done, FLASH_MS, pad);
    lv_timer_pause(pad->flash_timer);
}

void ui_pad_set_fill(ui_pad_t *pad, ui_fill_t fill)
{
    lv_obj_set_style_enabled(pad->obj, &s_fill_red_style, LV_STATE_DEFAULT, fill == UI_FILL_RED);
    lv_obj_set_style_enabled(pad->obj, &s_fill_orange_style, LV_STATE_DEFAULT, fill == UI_FILL_ORANGE);
}

void ui_pad_flash(ui_pad_t *pad, looper_outcome_t outcome, looper_gesture_t gesture)
{
    if (outcome == LOOPER_IGNORED) {
        return;
    }
    lv_state_t flash = STATE_DENIED_FLASH;
    if (outcome == LOOPER_APPLIED) {
        flash = gesture == LOOPER_LONG_PRESS ? STATE_LONG_PRESS_FLASH : STATE_TAP_FLASH;
    }
    lv_obj_remove_state(pad->obj, STATE_TAP_FLASH | STATE_LONG_PRESS_FLASH | STATE_DENIED_FLASH);
    lv_obj_add_state(pad->obj, flash);
    lv_timer_reset(pad->flash_timer);
    lv_timer_resume(pad->flash_timer);
}
