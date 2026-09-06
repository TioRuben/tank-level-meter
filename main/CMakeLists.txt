idf_component_register(SRCS "main.c" "i2c_bus.c" "sensor.c"
                    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c
                    INCLUDE_DIRS "")
