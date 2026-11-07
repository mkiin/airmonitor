#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t scd4x_driver_init(void);
esp_err_t scd4x_driver_deinit(void);

esp_err_t scd4x_driver_start_periodic_measurement(void);
esp_err_t scd4x_driver_stop_periodic_measurement(void);

// Commands return after I2C transmission. Callers enforce the 500 ms stop
// delay and 1 ms command-to-response delay.
esp_err_t scd4x_driver_request_data_ready(void);
esp_err_t scd4x_driver_read_data_ready(bool *ready);
esp_err_t scd4x_driver_request_measurement(void);

// Units: CO2 in ppm, temperature in millidegrees Celsius, humidity in
// thousandths of percent relative humidity. Outputs change only on ESP_OK.
esp_err_t scd4x_driver_read_measurement(uint16_t *co2_ppm, int32_t *temperature, int32_t *humidity);
