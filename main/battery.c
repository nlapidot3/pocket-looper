#include "battery.h"

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

#define AXP2101_ADDRESS 0x34
#define REG_STATUS1 0x00
#define REG_STATUS2 0x01
#define REG_GAUGE_CONTROL 0x18
#define REG_PERCENT 0xA4

#define STATUS1_BATTERY_PRESENT (1 << 3)
#define STATUS2_CHARGE_SHIFT 5
#define STATUS2_CHARGING 0x01
#define GAUGE_ENABLE (1 << 3)

#define I2C_TIMEOUT_MS 100

static const char *TAG = "battery";

static i2c_master_dev_handle_t s_device;

static esp_err_t read_register(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(s_device, &reg, 1, value, 1, I2C_TIMEOUT_MS);
}

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(s_device, data, sizeof(data), I2C_TIMEOUT_MS);
}

esp_err_t battery_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_FAIL, TAG, "I2C bus unavailable");

    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDRESS,
        .scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &config, &s_device), TAG, "Adding the power chip failed");

    uint8_t gauge;
    ESP_RETURN_ON_ERROR(read_register(REG_GAUGE_CONTROL, &gauge), TAG, "Reading the gauge control failed");
    if (!(gauge & GAUGE_ENABLE)) {
        ESP_RETURN_ON_ERROR(write_register(REG_GAUGE_CONTROL, gauge | GAUGE_ENABLE), TAG, "Enabling the gauge failed");
    }

    uint8_t status1, status2, percent;
    ESP_RETURN_ON_ERROR(read_register(REG_STATUS1, &status1), TAG, "Reading status 1 failed");
    ESP_RETURN_ON_ERROR(read_register(REG_STATUS2, &status2), TAG, "Reading status 2 failed");
    ESP_RETURN_ON_ERROR(read_register(REG_PERCENT, &percent), TAG, "Reading the percentage failed");
    ESP_LOGI(TAG, "Raw registers: 0x00=0x%02x 0x01=0x%02x 0x18=0x%02x (before) 0xA4=0x%02x", status1, status2, gauge,
             percent);
    return ESP_OK;
}

esp_err_t battery_read(battery_status_t *out)
{
    uint8_t status1;
    ESP_RETURN_ON_ERROR(read_register(REG_STATUS1, &status1), TAG, "Reading status 1 failed");
    battery_status_t status = {.present = status1 & STATUS1_BATTERY_PRESENT};
    if (status.present) {
        uint8_t status2, percent;
        ESP_RETURN_ON_ERROR(read_register(REG_STATUS2, &status2), TAG, "Reading status 2 failed");
        ESP_RETURN_ON_ERROR(read_register(REG_PERCENT, &percent), TAG, "Reading the percentage failed");
        status.charging = (status2 >> STATUS2_CHARGE_SHIFT) == STATUS2_CHARGING;
        status.percent = percent > 100 ? 100 : percent;
    }
    *out = status;
    return ESP_OK;
}
