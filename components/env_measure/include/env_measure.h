#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"

#define ENV_MEASURE_TIMER_BIT (1UL << 0)

typedef struct
{
    uint16_t co2_ppm;
    int32_t  temperature;  // Millidegrees Celsius.
    int32_t  humidity;     // Thousandths of percent relative humidity.
} env_measurement_t;

typedef struct
{
    QueueHandle_t      measurement_queue;
    esp_timer_handle_t deadline_timer;  // Owned by taskmng.
} env_measure_args_t;

esp_err_t env_measure_init(void);
void      env_measure_run(void *arg);
