#include "boot_button.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT_GPIO GPIO_NUM_0
#define POLL_MS 10
#define DEBOUNCE_SAMPLES 4

static boot_button_cb_t s_on_press;

static void poll_task(void *arg)
{
    bool pressed = false;
    int disagreeing_samples = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        bool low = gpio_get_level(BOOT_GPIO) == 0;
        if (low == pressed) {
            disagreeing_samples = 0;
            continue;
        }
        if (++disagreeing_samples < DEBOUNCE_SAMPLES) {
            continue;
        }
        disagreeing_samples = 0;
        pressed = low;
        if (pressed) {
            s_on_press();
        }
    }
}

esp_err_t boot_button_init(boot_button_cb_t on_press)
{
    s_on_press = on_press;
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    return xTaskCreate(poll_task, "boot_button", 4096, NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
