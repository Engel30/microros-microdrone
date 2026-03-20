idf_component_register(
    SRCS "motor_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES common esp_driver_ledc
)
