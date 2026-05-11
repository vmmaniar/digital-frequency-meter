idf_component_register(
    SRCS "app_main.c" "freq_counter.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)
