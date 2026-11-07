idf_component_register(
    SRCS "taskmng.c"
INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES esp_timer
    PRIV_REQUIRES env_measure env_telemetory wifi_service
)
