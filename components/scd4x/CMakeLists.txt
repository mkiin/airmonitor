idf_component_register(
    SRCS
        "scd4x_driver.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_i2c
)
