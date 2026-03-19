#include "imu_driver.h"
#include "esp_log.h"

static const char *TAG = "imu";

esp_err_t imu_init(void)
{
    ESP_LOGI(TAG, "IMU init - TODO");
    return ESP_OK;
}

esp_err_t imu_read(imu_data_t *data)
{
    // TODO: leggere registri MPU6050 via I2C
    return ESP_OK;
}

esp_err_t imu_calibrate(int num_samples)
{
    // TODO: calibrazione offset
    return ESP_OK;
}
