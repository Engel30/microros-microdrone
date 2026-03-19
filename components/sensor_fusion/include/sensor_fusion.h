#pragma once

#include "esp_err.h"
#include "drone_types.h"

esp_err_t fusion_init(void);
esp_err_t fusion_update(const imu_data_t *imu, const flow_data_t *flow, state_t *state);
