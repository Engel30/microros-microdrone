#pragma once

#include "esp_err.h"
#include "drone_types.h"

esp_err_t motors_init(void);
esp_err_t motors_set(const motor_cmd_t *cmd);
esp_err_t motors_stop(void);
