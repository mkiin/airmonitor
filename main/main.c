#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "taskmng.h"

void app_main(void)
{
#if CONFIG_APP_API_ENVIRONMENT_PRODUCTION
    esp_log_level_set("*", ESP_LOG_NONE);
#endif

    esp_err_t ret = nvs_flash_init();
    if ( ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND )
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(taskmng_init_all());
    ESP_ERROR_CHECK(taskmng_create_all());
}
