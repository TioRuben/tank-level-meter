#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "sensor.h"

static const char *TAG = "sensor_probe";

void app_main(void)
{
    i2c_master_dev_handle_t sensor_handle;
    esp_err_t err = sensor_i2c_init(&sensor_handle);
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
}
