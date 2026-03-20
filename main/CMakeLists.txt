idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES common imu_driver flow_driver motor_driver
    PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_usb_serial_jtag
)
