#pragma once

#include "esp_err.h"
#include "drone_types.h"

esp_err_t imu_init(void);
esp_err_t imu_read(imu_data_t *data);
esp_err_t imu_calibrate(int num_samples);
