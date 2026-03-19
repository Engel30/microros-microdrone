#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "drone_config.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "microros-microdrone starting - Drone ID: %d", DRONE_ID);
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());

    // TODO Fase 0A: creare queue e lanciare task
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
