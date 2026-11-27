#pragma once

#include "esp_err.h"

typedef void (*boot_button_cb_t)(void);

esp_err_t boot_button_init(boot_button_cb_t on_press);
