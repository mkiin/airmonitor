#include "env_measure.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/task.h"
#include "scd4x_driver.h"

#define SCD4X_STOP_DELAY_US     500000ULL
#define SCD4X_RESPONSE_DELAY_US 1000ULL
#define SCD4X_POLL_INTERVAL_US  5000000ULL

static const char *TAG = "env_measure";

typedef enum
{
    ENV_STOP,
    ENV_WAIT_STOP,
    ENV_WAIT_SAMPLE,
    ENV_WAIT_READY_REPLY,
    ENV_WAIT_MEAS_REPLY,
} env_measure_state_t;

static void env_measure_publish(QueueHandle_t measurement_queue, const env_measurement_t *sample)
{
    if ( xQueueOverwrite(measurement_queue, sample) != pdPASS )
    {
        ESP_LOGW(TAG, "sample not queued");
        return;
    }
    ESP_LOGI(TAG, "CO2=%u ppm, T=%.2f C, RH=%.2f %%", (unsigned)sample->co2_ppm, sample->temperature / 1000.0, sample->humidity / 1000.0);
}

static void env_measure_wait_for_deadline(esp_timer_handle_t timer, uint64_t delay_us)
{
    ESP_ERROR_CHECK(esp_timer_start_once(timer, delay_us));

    uint32_t bits;
    do
    {
        bits = 0;
        xTaskNotifyWait(0, ENV_MEASURE_TIMER_BIT, &bits, portMAX_DELAY);
    } while ( (bits & ENV_MEASURE_TIMER_BIT) == 0 );
}

esp_err_t env_measure_init(void)
{
    return scd4x_driver_init();
}

void env_measure_run(void *arg)
{
    const env_measure_args_t *args  = (const env_measure_args_t *)arg;
    env_measure_state_t       state = ENV_STOP;

    for ( ;; )
    {
        esp_err_t err;
        uint64_t  delay_us = SCD4X_POLL_INTERVAL_US;

        switch ( state )
        {
        case ENV_STOP:
            err = scd4x_driver_stop_periodic_measurement();
            if ( err == ESP_OK )
            {
                state    = ENV_WAIT_STOP;
                delay_us = SCD4X_STOP_DELAY_US;
            }
            else
            {
                ESP_LOGW(TAG, "stop failed: %s", esp_err_to_name(err));
            }
            break;

        case ENV_WAIT_STOP:
            err = scd4x_driver_start_periodic_measurement();
            if ( err == ESP_OK )
            {
                ESP_LOGI(TAG, "Periodic measurement started; checking every ~5 s");
                state = ENV_WAIT_SAMPLE;
            }
            else
            {
                ESP_LOGW(TAG, "start failed: %s", esp_err_to_name(err));
                state = ENV_STOP;
            }
            break;

        case ENV_WAIT_SAMPLE:
            err = scd4x_driver_request_data_ready();
            if ( err == ESP_OK )
            {
                state    = ENV_WAIT_READY_REPLY;
                delay_us = SCD4X_RESPONSE_DELAY_US;
            }
            else
            {
                ESP_LOGW(TAG, "ready request failed: %s", esp_err_to_name(err));
            }
            break;

        case ENV_WAIT_READY_REPLY: {
            bool ready = false;
            err        = scd4x_driver_read_data_ready(&ready);
            if ( err != ESP_OK )
            {
                ESP_LOGW(TAG, "ready response failed: %s", esp_err_to_name(err));
                state = ENV_WAIT_SAMPLE;
            }
            else if ( !ready )
            {
                state = ENV_WAIT_SAMPLE;
            }
            else
            {
                err = scd4x_driver_request_measurement();
                if ( err == ESP_OK )
                {
                    state    = ENV_WAIT_MEAS_REPLY;
                    delay_us = SCD4X_RESPONSE_DELAY_US;
                }
                else
                {
                    ESP_LOGW(TAG, "measurement request failed: %s", esp_err_to_name(err));
                    state = ENV_WAIT_SAMPLE;
                }
            }
            break;
        }

        case ENV_WAIT_MEAS_REPLY: {
            env_measurement_t sample;
            err = scd4x_driver_read_measurement(&sample.co2_ppm, &sample.temperature, &sample.humidity);
            if ( err != ESP_OK )
            {
                ESP_LOGW(TAG, "measurement response failed: %s", esp_err_to_name(err));
            }
            else if ( sample.co2_ppm == 0 )
            {
                ESP_LOGW(TAG, "Ignoring invalid CO2 sample (0 ppm)");
            }
            else
            {
                env_measure_publish(args->measurement_queue, &sample);
            }
            state = ENV_WAIT_SAMPLE;
            break;
        }
        }

        env_measure_wait_for_deadline(args->deadline_timer, delay_us);
    }
}
