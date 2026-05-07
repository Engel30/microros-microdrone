idf_component_register(
    SRCS
        "main.c"
        "task_imu.c"
        "task_flow.c"
        "task_battery.c"
        "task_motors.c"
    INCLUDE_DIRS "."
    REQUIRES
        common
        imu_driver
        flow_driver
        motor_driver
        battery_monitor
        uros_interface
    PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_usb_serial_jtag
)
