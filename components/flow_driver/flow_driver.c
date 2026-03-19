#include "flow_driver.h"
#include "esp_log.h"

static const char *TAG = "flow";

esp_err_t flow_init(void)
{
    ESP_LOGI(TAG, "Flow init - TODO");
    return ESP_OK;
}

esp_err_t flow_read(flow_data_t *data)
{
    // TODO: parser CXOF via UART
    return ESP_OK;
}
