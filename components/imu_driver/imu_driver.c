#include "imu_driver.h"
#include "drone_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mpu6050.h"
#include "i2cdev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "imu";

static mpu6050_dev_t mpu_dev;

// Offset calibrazione nel chip frame (sottratti prima della rotazione body)
static float offset_ax, offset_ay, offset_az;
static float offset_gx, offset_gy, offset_gz;

/**
 * Lettura interna nel chip frame (senza offset e senza rotazione).
 * Usata dalla calibrazione.
 */
static esp_err_t imu_read_raw_chip(float *ax, float *ay, float *az,
                                    float *gx, float *gy, float *gz)
{
    mpu6050_acceleration_t accel;
    mpu6050_rotation_t gyro;

    esp_err_t ret = mpu6050_get_motion(&mpu_dev, &accel, &gyro);
    if (ret != ESP_OK) return ret;

    *ax = accel.x;
    *ay = accel.y;
    *az = accel.z;
    *gx = gyro.x;
    *gy = gyro.y;
    *gz = gyro.z;

    return ESP_OK;
}

esp_err_t imu_init(void)
{
    esp_err_t ret;

    ret = i2cdev_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore init i2cdev: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mpu6050_init_desc(&mpu_dev, MPU6050_ADDR, 0, PIN_I2C_SDA, PIN_I2C_SCL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore init I2C desc: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mpu6050_init(&mpu_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore init MPU6050: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mpu6050_set_clock_source(&mpu_dev, MPU6050_CLOCK_PLL_X);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore config clock: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mpu6050_set_full_scale_accel_range(&mpu_dev, MPU6050_ACCEL_RANGE_4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore config accel range: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mpu6050_set_full_scale_gyro_range(&mpu_dev, MPU6050_GYRO_RANGE_500);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore config gyro range: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mpu6050_set_dlpf_mode(&mpu_dev, MPU6050_DLPF_3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore config DLPF: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mpu6050_set_rate(&mpu_dev, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore config sample rate: %s", esp_err_to_name(ret));
        return ret;
    }

    offset_ax = offset_ay = offset_az = 0.0f;
    offset_gx = offset_gy = offset_gz = 0.0f;

    ESP_LOGI(TAG, "MPU6050 inizializzato: ±4g, ±500°/s, DLPF 42Hz, 1kHz");
    return ESP_OK;
}

esp_err_t imu_read(imu_data_t *data)
{
    float raw_ax, raw_ay, raw_az;
    float raw_gx, raw_gy, raw_gz;

    esp_err_t ret = imu_read_raw_chip(&raw_ax, &raw_ay, &raw_az,
                                       &raw_gx, &raw_gy, &raw_gz);
    if (ret != ESP_OK) return ret;

    // Sottrai offset nel chip frame
    raw_ax -= offset_ax;
    raw_ay -= offset_ay;
    raw_az -= offset_az;
    raw_gx -= offset_gx;
    raw_gy -= offset_gy;
    raw_gz -= offset_gz;

    // Ruota nel body frame (X=avanti, Y=destra, Z=giù)
    data->accel_x = IMU_SIGN_AX_FROM_RAW(raw_ax, raw_ay);
    data->accel_y = IMU_SIGN_AY_FROM_RAW(raw_ax, raw_ay);
    data->accel_z = IMU_SIGN_AZ(raw_az);

    data->gyro_x = IMU_SIGN_GX_FROM_RAW(raw_gx, raw_gy);
    data->gyro_y = IMU_SIGN_GY_FROM_RAW(raw_gx, raw_gy);
    data->gyro_z = IMU_SIGN_GZ(raw_gz);

    data->timestamp_us = esp_timer_get_time();

    return ESP_OK;
}

esp_err_t imu_calibrate(int num_samples)
{
    ESP_LOGI(TAG, "Calibrazione IMU: %d campioni...", num_samples);

    float sum_ax = 0, sum_ay = 0, sum_az = 0;
    float sum_gx = 0, sum_gy = 0, sum_gz = 0;

    // Azzera offset per leggere valori chip raw
    offset_ax = offset_ay = offset_az = 0.0f;
    offset_gx = offset_gy = offset_gz = 0.0f;

    float ax, ay, az, gx, gy, gz;
    for (int i = 0; i < num_samples; i++) {
        esp_err_t ret = imu_read_raw_chip(&ax, &ay, &az, &gx, &gy, &gz);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Errore lettura calibrazione: %s", esp_err_to_name(ret));
            return ret;
        }
        sum_ax += ax;
        sum_ay += ay;
        sum_az += az;
        sum_gx += gx;
        sum_gy += gy;
        sum_gz += gz;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    offset_ax = sum_ax / num_samples;
    offset_ay = sum_ay / num_samples;
    // Il chip MPU6050 a riposo misura +1g su Z (verso alto).
    // L'offset deve togliere tutto tranne quella componente di gravità.
    offset_az = sum_az / num_samples - 1.0f;
    offset_gx = sum_gx / num_samples;
    offset_gy = sum_gy / num_samples;
    offset_gz = sum_gz / num_samples;

    ESP_LOGI(TAG, "Calibrazione OK (chip frame).");
    ESP_LOGI(TAG, "  Offset accel: [%.3f, %.3f, %.3f] g", offset_ax, offset_ay, offset_az);
    ESP_LOGI(TAG, "  Offset gyro:  [%.2f, %.2f, %.2f] deg/s", offset_gx, offset_gy, offset_gz);

    return ESP_OK;
}
