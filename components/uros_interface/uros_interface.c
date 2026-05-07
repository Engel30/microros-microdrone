#include "uros_interface.h"
#include "drone_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "uros";

esp_err_t uros_wifi_connect(void)
{
    // Step 4: implementare WiFi station + attesa GOT_IP
    ESP_LOGW(TAG, "uros_wifi_connect: STUB (Step 4 da implementare)");
    return ESP_OK;
}

esp_err_t uros_init(const uros_queues_t *queues)
{
    // Step 5+: implementare support/node/pub/sub/executor
    ESP_LOGW(TAG, "uros_init: STUB (Step 5 da implementare)");
    if (!queues) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

// Step 3 placeholder: drena tutte le queue produttrici e logga il count
// ogni N messaggi. Serve a verificare che task_imu/flow/battery girino
// e che le queue non si saturino. Nessun traffico micro-ROS reale.
void task_microros(void *arg)
{
    uros_queues_t *Q = (uros_queues_t *)arg;
    ESP_LOGI(TAG, "task_microros (placeholder Step 3) avviato");

    uint32_t imu_count = 0, flow_count = 0, batt_count = 0, motor_count = 0;
    imu_data_t imu;
    flow_data_t flow;
    float volt;
    motor_cmd_t mecho;

    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / FREQ_MICROROS_HZ);  // 20ms @ 50Hz
    uint32_t tick = 0;

    while (1) {
        while (xQueueReceive(Q->imu_queue, &imu, 0) == pdTRUE) {
            imu_count++;
        }
        while (xQueueReceive(Q->flow_queue, &flow, 0) == pdTRUE) {
            flow_count++;
        }
        while (xQueueReceive(Q->battery_queue, &volt, 0) == pdTRUE) {
            batt_count++;
        }
        while (xQueueReceive(Q->motor_echo_queue, &mecho, 0) == pdTRUE) {
            motor_count++;
        }

        // Log ogni secondo (~50 tick a 50Hz)
        if (++tick >= FREQ_MICROROS_HZ) {
            ESP_LOGI(TAG, "rcv imu=%lu flow=%lu batt=%lu motor=%lu",
                     (unsigned long)imu_count, (unsigned long)flow_count,
                     (unsigned long)batt_count, (unsigned long)motor_count);
            tick = 0;
        }

        vTaskDelayUntil(&last, period);
    }
}
