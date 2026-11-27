#include "display_reset.h"

#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RESET_PINS (IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2)
#define RESET_LOW_MS 20
#define PANEL_SETTLE_MS 120

static const char *TAG = "display_reset";

/* BSP_LCD_RST is not connected: the panel's hardware reset sits on pins 0 to 2 of the TCA9554 expander, which the board
 * support component never pulses, so a chip reset without power loss leaves the screen dark. */
esp_err_t display_reset(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C bus init failed");
    esp_io_expander_handle_t expander = bsp_io_expander_init();
    ESP_RETURN_ON_FALSE(expander != NULL, ESP_FAIL, TAG, "IO expander init failed");

    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(expander, RESET_PINS, IO_EXPANDER_OUTPUT), TAG, "Setting pin direction failed");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(expander, RESET_PINS, 0), TAG, "Pulling reset low failed");
    vTaskDelay(pdMS_TO_TICKS(RESET_LOW_MS));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(expander, RESET_PINS, 1), TAG, "Releasing reset failed");
    vTaskDelay(pdMS_TO_TICKS(PANEL_SETTLE_MS));
    return ESP_OK;
}
