#include "wifi_service.h"

#include <stddef.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

static const char *TAG = "wifi_service";

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    EventGroupHandle_t network_event_group = (EventGroupHandle_t)arg;
    (void)event_data;

    if ( event_base == WIFI_EVENT )
    {
        switch ( event_id )
        {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits(network_event_group, WIFI_SERVICE_IPV4_READY);
            ESP_LOGI(TAG, "再接続を試みます...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_STOP:
            xEventGroupClearBits(network_event_group, WIFI_SERVICE_IPV4_READY);
            break;

        default:
            break;
        }
    }
    else if ( event_base == IP_EVENT )
    {
        switch ( event_id )
        {
        case IP_EVENT_STA_GOT_IP:
            // ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            // ESP_LOGI(TAG, "IPアドレス取得: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(network_event_group, WIFI_SERVICE_IPV4_READY);
            break;

        case IP_EVENT_STA_LOST_IP:
            xEventGroupClearBits(network_event_group, WIFI_SERVICE_IPV4_READY);
            break;

        default:
            break;
        }
    }
}

esp_err_t wifi_service_init(EventGroupHandle_t network_event_group)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_APP_WIFI_SSID,
            .password = CONFIG_APP_WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    xEventGroupClearBits(network_event_group, WIFI_SERVICE_IPV4_READY);

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, network_event_group));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, network_event_group));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, wifi_event_handler, network_event_group));

    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}
