#include "taskmng.h"

#include <stddef.h>

#include "env_measure.h"
#include "env_telemetory.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "wifi_service.h"

typedef struct
{
    const char *name;
    uint32_t    stack_size_bytes;
    UBaseType_t priority;
    esp_err_t (*init)(void);
    TaskFunction_t run;
    void          *arg;
    TaskHandle_t   handle;
} taskmng_entry_t;

static env_measure_args_t    s_env_measure_args;
static env_telemetory_args_t s_env_telemetory_args;
static QueueHandle_t         s_measurement_queue;
static EventGroupHandle_t    s_network_event_group;
static esp_timer_handle_t    s_env_measure_timer;

static taskmng_entry_t s_task_entries[] = {
    {
     .name             = "env_measure",
     .stack_size_bytes = 4096,
     .priority         = 5,
     .init             = env_measure_init,
     .run              = env_measure_run,
     .arg              = &s_env_measure_args,
     .handle           = NULL,
     },
    {
     .name             = "env_telemetory",
     .stack_size_bytes = 4096,
     .priority         = 5,
     .init             = env_telemetory_init,
     .run              = env_telemetory_run,
     .arg              = &s_env_telemetory_args,
     .handle           = NULL,
     }
};

static void taskmng_env_measure_timer_callback(void *arg)
{
    taskmng_entry_t *entry = (taskmng_entry_t *)arg;
    if ( entry->handle != NULL )
    {
        xTaskNotify(entry->handle, ENV_MEASURE_TIMER_BIT, eSetBits);
    }
}

esp_err_t taskmng_init_all(void)
{
    if ( s_measurement_queue != NULL || s_network_event_group != NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    s_measurement_queue = xQueueCreate(1, sizeof(env_measurement_t));
    if ( s_measurement_queue == NULL )
    {
        return ESP_ERR_NO_MEM;
    }

    s_network_event_group = xEventGroupCreate();
    if ( s_network_event_group == NULL )
    {
        vQueueDelete(s_measurement_queue);
        s_measurement_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_env_measure_args.measurement_queue = s_measurement_queue;

    s_env_telemetory_args.measurement_queue   = s_measurement_queue;
    s_env_telemetory_args.network_event_group = s_network_event_group;

    for ( size_t i = 0; i < sizeof(s_task_entries) / sizeof(s_task_entries[ 0 ]); ++i )
    {
        if ( s_task_entries[ i ].init == NULL )
        {
            continue;
        }

        esp_err_t err = s_task_entries[ i ].init();
        if ( err != ESP_OK )
        {
            return err;
        }
    }

    return wifi_service_init(s_network_event_group);
}

esp_err_t taskmng_create_all(void)
{
    if ( s_env_measure_timer != NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_timer_create_args_t timer_args = {
        .callback        = taskmng_env_measure_timer_callback,
        .arg             = &s_task_entries[ 0 ],
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "env_measure",
    };
    esp_err_t err = esp_timer_create(&timer_args, &s_env_measure_timer);
    if ( err != ESP_OK )
    {
        return err;
    }
    s_env_measure_args.deadline_timer = s_env_measure_timer;

    for ( size_t i = 0; i < sizeof(s_task_entries) / sizeof(s_task_entries[ 0 ]); ++i )
    {
        if ( s_task_entries[ i ].run == NULL )
        {
            return ESP_ERR_INVALID_STATE;
        }

        taskmng_entry_t *entry = &s_task_entries[ i ];
        if ( entry->handle != NULL )
        {
            return ESP_ERR_INVALID_STATE;
        }

        BaseType_t result = xTaskCreate(entry->run, entry->name, entry->stack_size_bytes, entry->arg, entry->priority, &entry->handle);
        if ( result != pdPASS )
        {
            entry->handle = NULL;
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}
