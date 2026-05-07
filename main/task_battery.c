#include "uros_interface.h"
#include "drone_config.h"
#include "battery_monitor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "task_batt";

void task_battery(void *arg)
{
    uros_queues_t *Q = (uros_queues_t *)arg;
    ESP_LOGI(TAG, "task_battery @ %dHz Core %d", FREQ_BATTERY_HZ, xPortGetCoreID());

    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / FREQ_BATTERY_HZ);  // 1s @ 1Hz

    while (1) {
        float volt = battery_read_voltage();
        xQueueSend(Q->battery_queue, &volt, 0);
        vTaskDelayUntil(&last, period);
    }
}
