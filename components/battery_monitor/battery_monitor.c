#include "battery_monitor.h"
#include "esp_log.h"

static const char *TAG = "battery";

esp_err_t battery_init(void)
{
    ESP_LOGI(TAG, "Battery monitor init - TODO");
    return ESP_OK;
}

float battery_read_voltage(void)
{
    // TODO: leggere ADC e convertire con BATTERY_DIVIDER_RATIO
    return 0.0f;
}
