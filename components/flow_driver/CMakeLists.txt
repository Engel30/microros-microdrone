idf_component_register(
    SRCS "flow_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES common esp_driver_uart esp_timer
)
