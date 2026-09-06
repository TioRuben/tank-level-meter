#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ble.h"
#include "i2c_bus.h"
#include "sensor.h"

static const char *TAG = "sensor_probe";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        err = nvs_flash_erase();
        if (err == ESP_OK)
        {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(err));
        return;
    }

    i2c_master_dev_handle_t sensor_handle;
    err = sensor_i2c_init(&sensor_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C initialization failed: %s", esp_err_to_name(err));
        return;
    }

    err = sensor_init(sensor_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Sensor initialization failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Starting sensor sampling at I2C address 0x40");
    sensor_task_start(sensor_handle);

    err = app_ble_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE initialization failed: %s", esp_err_to_name(err));
    }
}
