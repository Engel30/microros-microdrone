#include "uros_interface.h"
#include "esp_log.h"

static const char *TAG = "uros";

esp_err_t uros_init(void)
{
    ESP_LOGI(TAG, "micro-ROS interface init - TODO");
    return ESP_OK;
}

esp_err_t uros_spin(void)
{
    // TODO: spin executor micro-ROS
    return ESP_OK;
}
