#include "env_measure.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/task.h"
#include "scd4x_driver.h"

#define ENV_MEASURE_POLL_INTERVAL_MS 5000U

static const char *TAG = "env_measure";

static bool env_measure_try_read(env_measurement_t *sample)
{
    esp_err_t err = scd4x_driver_try_read_measurement(&sample->co2_ppm, &sample->temperature, &sample->humidity);

    if ( err == ESP_ERR_NOT_FINISHED )
    {
        return false;
    }

    if ( err != ESP_OK )
    {
        ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
        return false;
    }

    if ( sample->co2_ppm == 0 )
    {
        ESP_LOGW(TAG, "Ignoring invalid CO2 sample (0 ppm)");
        return false;
    }

    return true;
}

static void env_measure_publish(QueueHandle_t measurement_queue, const env_measurement_t *sample)
{
    if ( xQueueOverwrite(measurement_queue, sample) != pdPASS )
    {
        ESP_LOGW(TAG, "sample not queued");
        return;
    }
    ESP_LOGI(TAG, "CO2=%u ppm, T=%.2f C, RH=%.2f %%", (unsigned)sample->co2_ppm, sample->temperature / 1000.0, sample->humidity / 1000.0);
}

esp_err_t env_measure_init(void)
{
    esp_err_t err = scd4x_driver_init();
    if ( err != ESP_OK )
    {
        return err;
    }

    // Also handles an ESP32-only reboot while the sensor is still measuring.
    err = scd4x_driver_stop_periodic_measurement();
    if ( err != ESP_OK )
    {
        return err;
    }

    err = scd4x_driver_start_periodic_measurement();
    if ( err != ESP_OK )
    {
        return err;
    }

    ESP_LOGI(TAG, "Periodic measurement started; updates every ~5 s");
    return ESP_OK;
}

void env_measure_run(void *arg)
{
    env_measure_args_t *args = (env_measure_args_t *)arg;

    TickType_t last_wake_tick = xTaskGetTickCount();

    for ( ;; )
    {
        xTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(ENV_MEASURE_POLL_INTERVAL_MS));
        env_measurement_t sample;

        if ( !env_measure_try_read(&sample) )
        {
            continue;
        }
        env_measure_publish(args->measurement_queue, &sample);
    }
}
