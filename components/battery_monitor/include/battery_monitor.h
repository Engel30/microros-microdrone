#pragma once

#include "esp_err.h"

esp_err_t battery_init(void);
float battery_read_voltage(void);
