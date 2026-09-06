#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#define SENSOR_RAW_LENGTH 4

typedef struct
{
    uint8_t level;
    uint8_t raw[SENSOR_RAW_LENGTH];
    esp_err_t last_error;
    uint32_t consecutive_failures;
    uint32_t sample_count;
    bool has_valid_sample;
} sensor_snapshot_t;

esp_err_t sensor_init(i2c_master_dev_handle_t sensor_handle);
esp_err_t sensor_read(i2c_master_dev_handle_t sensor_handle,
                      sensor_snapshot_t *snapshot);
void sensor_task_start(i2c_master_dev_handle_t sensor_handle);
esp_err_t sensor_get_latest(sensor_snapshot_t *snapshot);