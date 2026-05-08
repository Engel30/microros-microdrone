#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "drone_types.h"

// Bundle di queue passato come arg a tutti i task. Le queue sono create in
// main.c (uros_mode) prima di avviare i task. uros_interface scrive su
// cmd_queue (callback subscriber) e legge da imu/flow/battery/motor_echo.
typedef struct {
    QueueHandle_t imu_queue;          // produttore: task_imu
    QueueHandle_t flow_queue;         // produttore: task_flow
    QueueHandle_t battery_queue;      // produttore: task_battery (float volt)
    QueueHandle_t cmd_queue;          // produttore: subscriber callback (uros)
    QueueHandle_t motor_echo_queue;   // produttore: task_motors (echo telemetria)
} uros_queues_t;

// Connessione WiFi station bloccante (timeout 30s, retry interno).
// Step 4: implementazione vera. Per ora stub che logga e ritorna ESP_OK.
esp_err_t uros_wifi_connect(void);

// Init micro-ROS: support, node, publisher, subscriber, executor.
// Step 5+: implementazione vera. Per ora stub.
esp_err_t uros_init(const uros_queues_t *queues);

// Task micro-ROS (Core 0, prio PRIO_TASK_MICROROS, ~50Hz).
// arg = uros_queues_t* (deve sopravvivere fino a fine task).
void task_microros(void *arg);

// ============================================================================
// Console di debug Foxglove — publisher /drone_1/log (rcl_interfaces/Log)
// ============================================================================
// Livelli ROS2 standard. Il Log panel di Foxglove filtra per livello e stampa
// con colori. uros_log() può essere chiamata da qualsiasi task (NON da ISR).
// Drop on full: la queue interna è dimensionata per non saturare la rete; se
// piena, il messaggio viene scartato (non blocca il chiamante).
typedef enum {
    UROS_LOG_DEBUG = 10,
    UROS_LOG_INFO  = 20,
    UROS_LOG_WARN  = 30,
    UROS_LOG_ERROR = 40,
    UROS_LOG_FATAL = 50,
} uros_log_level_t;

// Logga su /drone_1/log. Safe-on-not-ready: prima che uros_init crei la queue
// la chiamata è no-op (nessun crash). Lunghezza messaggio troncata a ~190 byte.
void uros_log(uros_log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
