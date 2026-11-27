#pragma once

#include "esp_err.h"
#include "looper.h"

#define COMMANDS_NOTE_COUNT 12

esp_err_t commands_init(void);

void commands_send_action(looper_action_t action);

void commands_send_note(int index);
