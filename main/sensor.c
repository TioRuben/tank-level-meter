#include "sensor.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"

#define SENSOR_REGISTER_LEVEL 0x00
#define SENSOR_READ_TIMEOUT_MS 1000
#define SENSOR_SAMPLE_PERIOD_MS 1000
#define SENSOR_POWER_ON_DELAY_MS 600
#define SENSOR_TASK_STACK_SIZE 3072
#define SENSOR_TASK_PRIORITY 5

static const char *TAG = "sensor";
static SemaphoreHandle_t snapshot_mutex;
static sensor_snapshot_t latest_snapshot;

static esp_err_t sensor_read_raw(i2c_master_dev_handle_t sensor_handle,
                                 uint8_t raw[SENSOR_RAW_LENGTH])
{
    const uint8_t register_address = SENSOR_REGISTER_LEVEL;
    return i2c_master_transmit_receive(sensor_handle,
                                       &register_address,
                                       sizeof(register_address),
                                       raw,
                                       SENSOR_RAW_LENGTH,
                                       SENSOR_READ_TIMEOUT_MS);
}

esp_err_t sensor_init(i2c_master_dev_handle_t sensor_handle)
{
    if (sensor_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    snapshot_mutex = xSemaphoreCreateMutex();
    if (snapshot_mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    memset(&latest_snapshot, 0, sizeof(latest_snapshot));
    latest_snapshot.last_error = ESP_ERR_INVALID_STATE;
    vTaskDelay(pdMS_TO_TICKS(SENSOR_POWER_ON_DELAY_MS));
    return ESP_OK;
}

esp_err_t sensor_read(i2c_master_dev_handle_t sensor_handle,
                      sensor_snapshot_t *snapshot)
{
    if (sensor_handle == NULL || snapshot == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[SENSOR_RAW_LENGTH];
    esp_err_t err = sensor_read_raw(sensor_handle, raw);
    if (err != ESP_OK)
    {
        snapshot->last_error = err;
        snapshot->consecutive_failures++;
        return err;
    }

    memcpy(snapshot->raw, raw, sizeof(snapshot->raw));
    snapshot->level = raw[0];
    snapshot->last_error = ESP_OK;
    snapshot->consecutive_failures = 0;
    snapshot->sample_count++;
    snapshot->has_valid_sample = true;
    return ESP_OK;
}

esp_err_t sensor_get_latest(sensor_snapshot_t *snapshot)
{
    if (snapshot == NULL || snapshot_mutex == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(snapshot_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    *snapshot = latest_snapshot;
    xSemaphoreGive(snapshot_mutex);
    return ESP_OK;
}

static void sensor_task(void *arg)
{
    i2c_master_dev_handle_t sensor_handle = arg;
    sensor_snapshot_t snapshot;

    while (true)
    {
        esp_err_t err = sensor_get_latest(&snapshot);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not get sensor snapshot: %s", esp_err_to_name(err));
        }

        err = sensor_read(sensor_handle, &snapshot);
        if (xSemaphoreTake(snapshot_mutex, portMAX_DELAY) == pdTRUE)
        {
            latest_snapshot = snapshot;
            xSemaphoreGive(snapshot_mutex);
        }

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG,
                     "Read succeeded: level=0x%02X, raw=[0x%02X 0x%02X 0x%02X 0x%02X]",
                     snapshot.level,
                     snapshot.raw[0],
                     snapshot.raw[1],
                     snapshot.raw[2],
                     snapshot.raw[3]);
        }
        else
        {
            ESP_LOGW(TAG,
                     "Read failed: %s (consecutive failures: %lu)",
                     esp_err_to_name(err),
                     (unsigned long)snapshot.consecutive_failures);
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_SAMPLE_PERIOD_MS));
    }
}

void sensor_task_start(i2c_master_dev_handle_t sensor_handle)
{
    xTaskCreate(sensor_task,
                "sensor_task",
                SENSOR_TASK_STACK_SIZE,
                sensor_handle,
                SENSOR_TASK_PRIORITY,
                NULL);
}