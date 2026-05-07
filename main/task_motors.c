#include "uros_interface.h"
#include "drone_config.h"
#include "drone_types.h"
#include "motor_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "task_motors";

// Watchdog: se non arriva un nuovo motor_cmd_t entro MOTOR_CMD_TIMEOUT_MS,
// i motori vengono forzati a duty 0. last_cmd_us = 0 indica che NESSUN
// comando è mai arrivato dal boot, condizione di sicurezza fail-safe.
void task_motors(void *arg)
{
    uros_queues_t *Q = (uros_queues_t *)arg;
    ESP_LOGI(TAG, "task_motors @ %dHz Core %d (watchdog %dms)",
             FREQ_MOTORS_HZ, xPortGetCoreID(), MOTOR_CMD_TIMEOUT_MS);

    motor_cmd_t cmd = { .motor = {0}, .timestamp_us = 0 };
    int64_t last_cmd_us = 0;

    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / FREQ_MOTORS_HZ);  // 1ms @ 1kHz

    while (1) {
        motor_cmd_t new_cmd;
        if (xQueueReceive(Q->cmd_queue, &new_cmd, 0) == pdTRUE) {
            cmd = new_cmd;
            last_cmd_us = esp_timer_get_time();
        }

        int64_t now = esp_timer_get_time();
        bool watchdog_expired = (last_cmd_us == 0) ||
                                ((now - last_cmd_us) > (int64_t)MOTOR_CMD_TIMEOUT_MS * 1000);
        if (watchdog_expired) {
            memset(cmd.motor, 0, sizeof(cmd.motor));
        }

        motors_set(&cmd);
        xQueueOverwrite(Q->motor_echo_queue, &cmd);

        vTaskDelayUntil(&last, period);
    }
}
