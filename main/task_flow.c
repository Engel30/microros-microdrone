#include "uros_interface.h"
#include "drone_config.h"
#include "drone_types.h"
#include "flow_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "task_flow";

void task_flow(void *arg)
{
    uros_queues_t *Q = (uros_queues_t *)arg;
    ESP_LOGI(TAG, "task_flow @ %dHz Core %d", FREQ_FLOW_HZ, xPortGetCoreID());

    flow_data_t flow;
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / FREQ_FLOW_HZ);  // 50ms @ 20Hz

    while (1) {
        if (flow_read(&flow) == ESP_OK) {
            xQueueSend(Q->flow_queue, &flow, 0);
        }
        vTaskDelayUntil(&last, period);
    }
}
