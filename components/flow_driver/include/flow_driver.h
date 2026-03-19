#pragma once

#include "esp_err.h"
#include "drone_types.h"

esp_err_t flow_init(void);
esp_err_t flow_read(flow_data_t *data);
