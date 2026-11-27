#include "esp_log.h"
#include "nvs_flash.h"

#include "ble_midi.h"
#include "boot_button.h"
#include "commands.h"
#include "looper.h"
#include "ui.h"

static const char *TAG = "pocket_looper";

/* Once ui_init has returned, only callbacks that run on the LVGL task touch s_looper, so it needs no lock. */
static looper_t s_looper;

static void send_actions(const looper_result_t *result)
{
    for (int i = 0; i < result->action_count; i++) {
        commands_send_action(result->actions[i]);
    }
}

static looper_outcome_t on_gesture(looper_button_t button, looper_gesture_t gesture)
{
    if (!ble_midi_can_send()) {
        return LOOPER_DENIED;
    }
    looper_result_t result = looper_press(&s_looper, button, gesture);
    send_actions(&result);
    ui_set_looper(&s_looper);
    return result.outcome;
}

static void on_confirm(void)
{
    looper_result_t result = ble_midi_can_send() ? looper_confirm(&s_looper) : looper_cancel(&s_looper);
    send_actions(&result);
    ui_set_looper(&s_looper);
}

static void on_cancel(void)
{
    looper_cancel(&s_looper);
    ui_set_looper(&s_looper);
}

static void on_confirm_timeout(void)
{
    looper_confirm_timed_out(&s_looper);
    ui_set_looper(&s_looper);
}

static looper_outcome_t on_note(int index)
{
    if (!ble_midi_can_send()) {
        return LOOPER_DENIED;
    }
    commands_send_note(index);
    return LOOPER_APPLIED;
}

static void on_reset(void)
{
    looper_reset(&s_looper);
    ui_set_looper(&s_looper);
}

static void on_link_changed(bool connected)
{
    ESP_LOGI(TAG, "Link %s", connected ? "connected" : "advertising");
    ui_set_can_send(ble_midi_can_send());
}

static void on_subscription_changed(bool subscribed)
{
    ui_set_can_send(ble_midi_can_send());
}

static void on_boot_press(void)
{
    ui_toggle_setup();
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    static const ui_callbacks_t ui_callbacks = {
        .on_gesture = on_gesture,
        .on_confirm = on_confirm,
        .on_cancel = on_cancel,
        .on_confirm_timeout = on_confirm_timeout,
        .on_note = on_note,
        .on_reset = on_reset,
    };
    static const ble_midi_callbacks_t ble_callbacks = {
        .on_link_changed = on_link_changed,
        .on_subscription_changed = on_subscription_changed,
    };

    looper_reset(&s_looper);
    ESP_ERROR_CHECK(commands_init());
    ESP_ERROR_CHECK(ui_init(&ui_callbacks));
    ESP_ERROR_CHECK(ble_midi_init(&ble_callbacks));
    ESP_ERROR_CHECK(boot_button_init(on_boot_press));
}
