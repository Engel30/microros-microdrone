idf_component_register(
    SRCS "imu_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES common driver mpu6050 i2cdev esp_idf_lib_helpers esp_timer
)
