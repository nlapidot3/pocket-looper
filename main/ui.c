#include "ui.h"

#include "bsp/esp-bsp.h"
#include "display_reset.h"
#include "ui_internal.h"

#define LONG_PRESS_MS 600
#define BRIGHTNESS_PERCENT 80

static ui_callbacks_t s_callbacks;
static lv_obj_t *s_main_screen;
static lv_obj_t *s_setup_screen;

esp_err_t ui_init(const ui_callbacks_t *callbacks)
{
    s_callbacks = *callbacks;

    esp_err_t err = display_reset();
    if (err != ESP_OK) {
        return err;
    }
    if (bsp_display_start() == NULL) {
        return ESP_FAIL;
    }
    err = bsp_display_brightness_set(BRIGHTNESS_PERCENT);
    if (err != ESP_OK) {
        return err;
    }
    if (!bsp_display_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_indev_set_long_press_time(bsp_display_get_input_dev(), LONG_PRESS_MS);
    ui_pad_styles_init();
    s_main_screen = lv_screen_active();
    ui_main_create(s_main_screen, &s_callbacks);
    s_setup_screen = lv_obj_create(NULL);
    ui_setup_create(s_setup_screen, &s_callbacks);
    bsp_display_unlock();
    return ESP_OK;
}

void ui_set_looper(const looper_t *looper)
{
    if (bsp_display_lock(0)) {
        ui_main_set_looper(looper);
        bsp_display_unlock();
    }
}

void ui_set_can_send(bool can_send)
{
    if (bsp_display_lock(0)) {
        ui_main_set_can_send(can_send);
        bsp_display_unlock();
    }
}

void ui_set_battery(const battery_status_t *battery)
{
    if (bsp_display_lock(0)) {
        ui_main_set_battery(battery);
        bsp_display_unlock();
    }
}

void ui_toggle_setup(void)
{
    if (bsp_display_lock(0)) {
        lv_screen_load(lv_screen_active() == s_main_screen ? s_setup_screen : s_main_screen);
        bsp_display_unlock();
    }
}
