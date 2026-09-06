#include "i2c_bus.h"

#include "driver/gpio.h"

#define SENSOR_I2C_PORT I2C_NUM_0
#define SENSOR_SDA_GPIO GPIO_NUM_21
#define SENSOR_SCL_GPIO GPIO_NUM_22
#define SENSOR_I2C_ADDRESS 0x40
#define SENSOR_I2C_FREQUENCY_HZ 100000

esp_err_t sensor_i2c_init(i2c_master_dev_handle_t *sensor_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = SENSOR_I2C_PORT,
        .sda_io_num = SENSOR_SDA_GPIO,
        .scl_io_num = SENSOR_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    i2c_master_bus_handle_t bus_handle;
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SENSOR_I2C_ADDRESS,
        .scl_speed_hz = SENSOR_I2C_FREQUENCY_HZ,
    };
    err = i2c_master_bus_add_device(bus_handle, &device_config, sensor_handle);
    if (err != ESP_OK)
    {
        i2c_del_master_bus(bus_handle);
    }

    return err;
}