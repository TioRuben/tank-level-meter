#include "sensor.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#define SENSOR_REGISTER_LEVEL 0x00
#define SENSOR_REGISTER_HANDSHAKE 0x01
#define SENSOR_REGISTER_SAMPLE 0x02
#define SENSOR_CALIBRATION_READ_COMMAND 0xCA
#define SENSOR_CALIBRATION_WRITE_COMMAND 0x8C
#define SENSOR_CALIBRATION_READY_RESPONSE 0xCD
#define SENSOR_CALIBRATION_CHANNEL_OFFSET 0x30
#define SENSOR_CALIBRATION_POLL_ATTEMPTS 20
#define SENSOR_CALIBRATION_POLL_DELAY_US 500
#define SENSOR_READ_TIMEOUT_MS 1000
#define SENSOR_SAMPLE_PERIOD_MS 1000
#define SENSOR_POWER_ON_DELAY_MS 600
#define SENSOR_TASK_STACK_SIZE 3072
#define SENSOR_TASK_PRIORITY 5

static const char *TAG = "sensor";
static SemaphoreHandle_t snapshot_mutex;
static SemaphoreHandle_t operation_mutex;
static sensor_snapshot_t latest_snapshot;
static i2c_master_dev_handle_t sensor_handle;
static sensor_calibration_state_t calibration_state = SENSOR_CALIBRATION_IDLE;

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

static esp_err_t sensor_write_register(uint8_t register_address,
                                       const uint8_t *data,
                                       size_t data_length)
{
    if (data_length > 2)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t transaction[3] = {register_address, 0, 0};
    memcpy(&transaction[1], data, data_length);
    return i2c_master_transmit(sensor_handle,
                               transaction,
                               data_length + 1,
                               SENSOR_READ_TIMEOUT_MS);
}

static esp_err_t sensor_read_registers(uint8_t register_address,
                                       uint8_t *data,
                                       size_t data_length)
{
    return i2c_master_transmit_receive(sensor_handle,
                                       &register_address,
                                       sizeof(register_address),
                                       data,
                                       data_length,
                                       SENSOR_READ_TIMEOUT_MS);
}

static esp_err_t sensor_restore_level_pointer(void)
{
    return sensor_write_register(SENSOR_REGISTER_LEVEL, NULL, 0);
}

static esp_err_t sensor_poll_register(uint8_t expected_value)
{
    uint8_t response[3];
    for (int attempt = 0; attempt < SENSOR_CALIBRATION_POLL_ATTEMPTS; ++attempt)
    {
        esp_err_t err = sensor_read_registers(SENSOR_REGISTER_HANDSHAKE,
                                              response,
                                              sizeof(response));
        if (err != ESP_OK)
        {
            return err;
        }
        if (response[0] == expected_value)
        {
            return ESP_OK;
        }
        esp_rom_delay_us(SENSOR_CALIBRATION_POLL_DELAY_US);
    }

    return ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t sensor_read_channel(uint8_t channel, uint16_t *sample)
{
    const uint8_t command = SENSOR_CALIBRATION_READ_COMMAND;
    const uint8_t channel_command = SENSOR_CALIBRATION_CHANNEL_OFFSET + channel;
    uint8_t response[3];
    esp_err_t err = sensor_write_register(SENSOR_REGISTER_HANDSHAKE,
                                          &command,
                                          sizeof(command));
    if (err != ESP_OK)
    {
        return err;
    }

    err = sensor_poll_register(SENSOR_CALIBRATION_READY_RESPONSE);
    if (err != ESP_OK)
    {
        return err;
    }

    err = sensor_write_register(SENSOR_REGISTER_HANDSHAKE,
                                &channel_command,
                                sizeof(channel_command));
    if (err != ESP_OK)
    {
        return err;
    }

    err = sensor_poll_register(channel);
    if (err != ESP_OK)
    {
        return err;
    }

    err = sensor_read_registers(SENSOR_REGISTER_HANDSHAKE,
                                response,
                                sizeof(response));
    if (err != ESP_OK)
    {
        return err;
    }

    *sample = ((uint16_t)response[1] << 8) | response[2];
    return ESP_OK;
}

static esp_err_t sensor_write_threshold(uint8_t channel, uint16_t sample)
{
    const uint8_t command = SENSOR_CALIBRATION_WRITE_COMMAND;
    const uint8_t channel_command = SENSOR_CALIBRATION_CHANNEL_OFFSET + channel;
    const uint8_t sample_data[2] = {
        (uint8_t)(sample >> 8),
        (uint8_t)sample,
    };
    esp_err_t err = sensor_write_register(SENSOR_REGISTER_HANDSHAKE,
                                          &command,
                                          sizeof(command));
    if (err != ESP_OK)
    {
        return err;
    }

    err = sensor_poll_register(SENSOR_CALIBRATION_READY_RESPONSE);
    if (err != ESP_OK)
    {
        return err;
    }

    err = sensor_write_register(SENSOR_REGISTER_SAMPLE,
                                sample_data,
                                sizeof(sample_data));
    if (err != ESP_OK)
    {
        return err;
    }

    err = sensor_write_register(SENSOR_REGISTER_HANDSHAKE,
                                &channel_command,
                                sizeof(channel_command));
    if (err != ESP_OK)
    {
        return err;
    }

    return sensor_poll_register(channel);
}

static esp_err_t sensor_calibrate_current_step(void)
{
    uint16_t sample;
    esp_err_t err;

    switch (calibration_state)
    {
    case SENSOR_CALIBRATION_WAIT_EMPTY:
        ESP_LOGI(TAG, "Calibration empty step: reading channel 1");
        err = sensor_read_channel(1, &sample);
        if (err == ESP_OK)
        {
            err = sensor_write_threshold(1, sample);
        }
        if (err == ESP_OK)
        {
            calibration_state = SENSOR_CALIBRATION_WAIT_ZERO;
        }
        return err;

    case SENSOR_CALIBRATION_WAIT_ZERO:
        ESP_LOGI(TAG, "Calibration zero step: reading channel 2");
        err = sensor_read_channel(2, &sample);
        if (err == ESP_OK)
        {
            err = sensor_write_threshold(2, sample);
        }
        if (err == ESP_OK)
        {
            calibration_state = SENSOR_CALIBRATION_WAIT_FULL;
        }
        return err;

    case SENSOR_CALIBRATION_WAIT_FULL:
        ESP_LOGI(TAG, "Calibration full step: reading channels 1 and 2");
        err = sensor_read_channel(1, &sample);
        if (err == ESP_OK)
        {
            err = sensor_write_threshold(3, sample);
        }
        if (err == ESP_OK)
        {
            err = sensor_read_channel(2, &sample);
        }
        if (err == ESP_OK)
        {
            err = sensor_write_threshold(4, sample);
        }
        if (err == ESP_OK)
        {
            calibration_state = SENSOR_CALIBRATION_COMPLETE;
        }
        return err;

    default:
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t sensor_init(i2c_master_dev_handle_t device_handle)
{
    if (device_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    snapshot_mutex = xSemaphoreCreateMutex();
    operation_mutex = xSemaphoreCreateMutex();
    if (snapshot_mutex == NULL || operation_mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    sensor_handle = device_handle;
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

    if (xSemaphoreTake(operation_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t raw[SENSOR_RAW_LENGTH];
    esp_err_t err = sensor_read_raw(sensor_handle, raw);
    if (err != ESP_OK)
    {
        snapshot->last_error = err;
        snapshot->consecutive_failures++;
        xSemaphoreGive(operation_mutex);
        return err;
    }

    memcpy(snapshot->raw, raw, sizeof(snapshot->raw));
    snapshot->level = raw[0];
    snapshot->last_error = ESP_OK;
    snapshot->consecutive_failures = 0;
    snapshot->sample_count++;
    snapshot->has_valid_sample = true;
    xSemaphoreGive(operation_mutex);
    return ESP_OK;
}

esp_err_t sensor_calibration_start(void)
{
    if (operation_mutex == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(operation_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    if (calibration_state == SENSOR_CALIBRATION_WAIT_EMPTY ||
        calibration_state == SENSOR_CALIBRATION_WAIT_ZERO ||
        calibration_state == SENSOR_CALIBRATION_WAIT_FULL)
    {
        xSemaphoreGive(operation_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    calibration_state = SENSOR_CALIBRATION_WAIT_EMPTY;
    ESP_LOGI(TAG, "Calibration started; waiting for empty container");
    xSemaphoreGive(operation_mutex);
    return ESP_OK;
}

esp_err_t sensor_calibration_commit(void)
{
    if (operation_mutex == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(operation_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = sensor_calibrate_current_step();
    if (err != ESP_OK)
    {
        calibration_state = SENSOR_CALIBRATION_FAILED;
        ESP_LOGE(TAG, "Calibration step failed: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Calibration state is now %d", calibration_state);
    }

    esp_err_t restore_err = sensor_restore_level_pointer();
    if (err == ESP_OK && restore_err != ESP_OK)
    {
        calibration_state = SENSOR_CALIBRATION_FAILED;
        err = restore_err;
        ESP_LOGE(TAG, "Could not restore level register pointer: %s",
                 esp_err_to_name(err));
    }

    xSemaphoreGive(operation_mutex);
    return err;
}

esp_err_t sensor_calibration_cancel(void)
{
    if (operation_mutex == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(operation_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    calibration_state = SENSOR_CALIBRATION_CANCELLED;
    esp_err_t err = sensor_restore_level_pointer();
    ESP_LOGI(TAG, "Calibration cancelled");
    xSemaphoreGive(operation_mutex);
    return err;
}

sensor_calibration_state_t sensor_calibration_get_state(void)
{
    sensor_calibration_state_t state = SENSOR_CALIBRATION_FAILED;
    if (operation_mutex == NULL)
    {
        return state;
    }
    if (xSemaphoreTake(operation_mutex, portMAX_DELAY) == pdTRUE)
    {
        state = calibration_state;
        xSemaphoreGive(operation_mutex);
    }
    return state;
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