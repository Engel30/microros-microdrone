#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "drone_config.h"
#include "drone_types.h"
#include "imu_driver.h"
#include "flow_driver.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "microros-microdrone starting - Drone ID: %d", DRONE_ID);

    // LED status acceso = firmware in esecuzione
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << PIN_LED_STATUS),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(PIN_LED_STATUS, 1);

    // Inizializza IMU
    esp_err_t ret = imu_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IMU init fallita!");
        while (1) {
            gpio_set_level(PIN_LED_STATUS, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(PIN_LED_STATUS, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Inizializza Optical Flow
    ret = flow_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flow init fallita!");
    }

    // Calibrazione IMU
    ESP_LOGI(TAG, "Calibrazione IMU in 2 secondi... TIENI FERMO!");
    vTaskDelay(pdMS_TO_TICKS(2000));
    imu_calibrate(500);

    // Header CSV per logging (una volta sola)
    printf("timestamp_ms,ax,ay,az,gx,gy,gz,flow_vx,flow_vy,flow_px,flow_py,flow_raw_x,flow_raw_y,range_mm,quality\n");

    // Loop: leggi e stampa dati a 20Hz
    imu_data_t imu;
    flow_data_t flow;
    bool have_imu = false, have_flow = false;

    while (1) {
        have_imu = (imu_read(&imu) == ESP_OK);
        if (flow_read(&flow) == ESP_OK) {
            have_flow = true;
        }

        // Stampa CSV: sempre IMU, flow quando disponibile
        if (have_imu) {
            int64_t t_ms = imu.timestamp_us / 1000;
            printf("%lld,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f",
                   t_ms,
                   imu.accel_x, imu.accel_y, imu.accel_z,
                   imu.gyro_x, imu.gyro_y, imu.gyro_z);

            if (have_flow) {
                printf(",%.4f,%.4f,%.4f,%.4f,%d,%d,%u,%u",
                       flow.vel_x, flow.vel_y,
                       flow.pos_x, flow.pos_y,
                       flow.raw_x, flow.raw_y,
                       flow.range_mm, flow.quality);
                have_flow = false;
            } else {
                printf(",,,,,,,,");
            }
            printf("\n");
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // 20Hz
    }
}
