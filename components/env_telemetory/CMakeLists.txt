idf_component_register(
    SRCS "env_telemetory.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES app_config env_measure log wifi_service esp_http_client json mbedtls
)
