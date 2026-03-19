#include "sensor_fusion.h"
#include "esp_log.h"

static const char *TAG = "fusion";

esp_err_t fusion_init(void)
{
    ESP_LOGI(TAG, "Sensor fusion init - TODO (Fase 2+)");
    return ESP_OK;
}

esp_err_t fusion_update(const imu_data_t *imu, const flow_data_t *flow, state_t *state)
{
    // TODO: fusione sensori (Fase 2+)
    return ESP_OK;
}
