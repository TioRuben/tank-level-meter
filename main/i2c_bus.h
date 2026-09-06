#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t sensor_i2c_init(i2c_master_dev_handle_t *sensor_handle);