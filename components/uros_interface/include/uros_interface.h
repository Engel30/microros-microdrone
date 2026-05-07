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
