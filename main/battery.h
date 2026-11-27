#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
    bool present;
    bool charging;
    int percent;
} battery_status_t;

esp_err_t battery_init(void);

esp_err_t battery_read(battery_status_t *out);
