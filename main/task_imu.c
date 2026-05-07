#include "uros_interface.h"
#include "drone_config.h"
#include "drone_types.h"
#include "imu_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "task_imu";

void task_imu(void *arg)
{
    uros_queues_t *Q = (uros_queues_t *)arg;
    ESP_LOGI(TAG, "task_imu @ %dHz Core %d", FREQ_IMU_HZ, xPortGetCoreID());

    imu_data_t imu;
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / FREQ_IMU_HZ);  // 1ms @ 1kHz

    while (1) {
        if (imu_read(&imu) == ESP_OK) {
            // Drop se piena (queue depth 5, consumatori sono task lenti)
            xQueueSend(Q->imu_queue, &imu, 0);
        }
        vTaskDelayUntil(&last, period);
    }
}
