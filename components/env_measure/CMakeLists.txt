idf_component_register(
    SRCS "env_measure.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES scd4x log
)
