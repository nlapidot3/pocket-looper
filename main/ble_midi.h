#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    void (*on_link_changed)(bool connected);
    void (*on_subscription_changed)(bool subscribed);
    void (*on_receive)(const uint8_t *data, size_t length);
} ble_midi_callbacks_t;

esp_err_t ble_midi_init(const ble_midi_callbacks_t *callbacks);

bool ble_midi_is_connected(void);

bool ble_midi_can_send(void);

esp_err_t ble_midi_send(uint8_t status, uint8_t data1, uint8_t data2);

esp_err_t ble_midi_send_two_bytes(uint8_t status, uint8_t data1);
