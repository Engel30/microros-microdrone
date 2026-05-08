idf_component_register(
    SRCS "uros_interface.c"
    INCLUDE_DIRS "include"
    REQUIRES common esp_wifi esp_netif esp_event nvs_flash esp_timer esp_driver_tsens micro_ros_espidf_component
)
