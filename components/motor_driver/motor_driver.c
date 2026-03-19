#include "motor_driver.h"
#include "esp_log.h"

static const char *TAG = "motors";

esp_err_t motors_init(void)
{
    ESP_LOGI(TAG, "Motors init - TODO");
    return ESP_OK;
}

esp_err_t motors_set(const motor_cmd_t *cmd)
{
    // TODO: impostare duty PWM via LEDC
    return ESP_OK;
}

esp_err_t motors_stop(void)
{
    // TODO: duty 0 su tutti i motori
    return ESP_OK;
}
