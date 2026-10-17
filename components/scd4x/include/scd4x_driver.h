#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t scd4x_driver_init(void);
esp_err_t scd4x_driver_deinit(void);

esp_err_t scd4x_driver_start_periodic_measurement(void);
esp_err_t scd4x_driver_stop_periodic_measurement(void);

// ESP_OK: measurement acquired; ESP_ERR_NOT_FINISHED: no data ready yet.
// All output pointers must be non-NULL. Use outputs only on ESP_OK.
// Units: CO2 in ppm, temperature in millidegrees Celsius,
// humidity in thousandths of percent relative humidity.
esp_err_t scd4x_driver_try_read_measurement(uint16_t *co2_ppm, int32_t *temperature, int32_t *humidity);
