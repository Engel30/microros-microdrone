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
//
// Gate arm software: g_armed (atomic_bool, vedi drone_types.h) è scritto
// dalla callback subscriber /drone_1/arm. Quando disarmato, i motori sono
// forzati a 0 indipendentemente da cmd_queue. Sulla transizione disarm→arm
// si resetta last_cmd_us=0 per costringere il watchdog a richiedere un cmd
// fresco prima di far girare i motori (evita che cmd stantii pre-disarm
// riprendano automaticamente).
void task_motors(void *arg)
{
    uros_queues_t *Q = (uros_queues_t *)arg;
    ESP_LOGI(TAG, "task_motors @ %dHz Core %d (watchdog %dms, boot DISARMED)",
             FREQ_MOTORS_HZ, xPortGetCoreID(), MOTOR_CMD_TIMEOUT_MS);

    motor_cmd_t cmd = { .motor = {0}, .timestamp_us = 0 };
    int64_t last_cmd_us = 0;
    bool prev_armed = false;
    bool prev_watchdog_expired = true;  // boot: nessun cmd → expired

    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / FREQ_MOTORS_HZ);  // 1ms @ 1kHz

    while (1) {
        bool armed = atomic_load(&g_armed);

        // Rising edge disarm → arm: invalida ultimo timestamp cmd così il
        // watchdog tiene i motori a 0 finché non arriva un nuovo cmd_motor_test.
        if (armed && !prev_armed) {
            last_cmd_us = 0;
            memset(cmd.motor, 0, sizeof(cmd.motor));
        }
        prev_armed = armed;

        motor_cmd_t new_cmd;
        if (xQueueReceive(Q->cmd_queue, &new_cmd, 0) == pdTRUE) {
            cmd = new_cmd;
            last_cmd_us = esp_timer_get_time();
        }

        int64_t now = esp_timer_get_time();
        bool watchdog_expired = (last_cmd_us == 0) ||
                                ((now - last_cmd_us) > (int64_t)MOTOR_CMD_TIMEOUT_MS * 1000);
        if (watchdog_expired || !armed) {
            memset(cmd.motor, 0, sizeof(cmd.motor));
        }

        // Log su /drone_1/log la transizione del watchdog: edge running→expired
        // ti dice quando smetti di pubblicare cmd_motor_test (es. agent giù,
        // reset, freeze del topic). Edge expired→running quando i cmd ritornano.
        // Solo se armed: in disarmed lo zero è atteso e non interessante.
        if (armed && watchdog_expired && !prev_watchdog_expired) {
            uros_log(UROS_LOG_WARN,
                     "motors watchdog: cmd timeout >%dms, motori azzerati",
                     MOTOR_CMD_TIMEOUT_MS);
        } else if (armed && !watchdog_expired && prev_watchdog_expired) {
            uros_log(UROS_LOG_INFO, "motors: cmd flow ripreso");
        }
        prev_watchdog_expired = watchdog_expired;

        motors_set(&cmd);
        xQueueOverwrite(Q->motor_echo_queue, &cmd);

        vTaskDelayUntil(&last, period);
    }
}
