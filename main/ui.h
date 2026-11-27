#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "looper.h"

typedef struct {
    looper_outcome_t (*on_gesture)(looper_button_t button, looper_gesture_t gesture);
    void (*on_confirm)(void);
    void (*on_cancel)(void);
    void (*on_confirm_timeout)(void);
    looper_outcome_t (*on_note)(int index);
    void (*on_reset)(void);
} ui_callbacks_t;

esp_err_t ui_init(const ui_callbacks_t *callbacks);

void ui_set_looper(const looper_t *looper);

void ui_set_can_send(bool can_send);

void ui_toggle_setup(void);
