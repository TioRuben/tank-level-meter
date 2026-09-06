idf_component_register(SRCS "main.c" "i2c_bus.c" "sensor.c" "ble.c"
                    PRIV_REQUIRES bt esp_driver_gpio esp_driver_i2c nvs_flash
                    INCLUDE_DIRS "")
