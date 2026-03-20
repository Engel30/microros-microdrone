#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "drone_config.h"
#include "drone_types.h"
#include "imu_driver.h"
#include "flow_driver.h"
#include "motor_driver.h"

static const char *TAG = "main";

// ============================================================================
// Modalità test motori — menu interattivo via monitor seriale
// ============================================================================

static const char *motor_names[4] = { "FL", "RL", "RR", "FR" };

// Flag: driver USB Serial/JTAG installato
static bool usb_serial_installed = false;

static void console_init(void)
{
    if (usb_serial_installed) return;
    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = 256,
        .tx_buffer_size = 256,
    };
    esp_err_t ret = usb_serial_jtag_driver_install(&cfg);
    if (ret == ESP_OK) {
        usb_serial_installed = true;
        ESP_LOGI(TAG, "USB Serial/JTAG driver installato per input console");
    } else {
        ESP_LOGE(TAG, "USB Serial/JTAG install fallito: %s", esp_err_to_name(ret));
    }
}

// Legge una linea dalla console USB Serial/JTAG.
// Ritorna lunghezza stringa, 0 se timeout.
static int console_readline(char *buf, int maxlen, TickType_t timeout)
{
    int pos = 0;
    TickType_t start = xTaskGetTickCount();
    while (pos < maxlen - 1) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= timeout) break;
        TickType_t remaining = timeout - elapsed;

        char c;
        int len = usb_serial_jtag_read_bytes((uint8_t *)&c, 1, remaining);
        if (len <= 0) break;  // timeout
        if (c == '\n' || c == '\r') {
            if (pos > 0) break;
            continue;
        }
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return pos;
}

static void motor_test_mode(void)
{
    ESP_LOGI(TAG, "=== MOTOR TEST MODE ===");

    motor_cmd_t cmd = { .motor = {0, 0, 0, 0}, .timestamp_us = 0 };

    while (1) {
        printf("\n--- Motor Test ---\n");
        printf("[1] FL  [2] RL  [3] RR  [4] FR\n");
        printf("[5] All motors  [0] Stop all  [9] Exit test\n");
        printf("Select: ");
        fflush(stdout);

        char buf[16];
        if (console_readline(buf, sizeof(buf), pdMS_TO_TICKS(30000)) == 0) {
            continue;
        }

        int sel = atoi(buf);

        if (sel == 0) {
            motors_stop();
            memset(cmd.motor, 0, sizeof(cmd.motor));
            printf(">> All motors STOPPED\n");
            continue;
        }

        if (sel == 9) {
            motors_stop();
            printf(">> Exiting motor test\n");
            break;
        }

        if (sel < 1 || (sel > 5)) {
            printf(">> Invalid selection\n");
            continue;
        }

        printf("Duty %% (0-100): ");
        fflush(stdout);

        if (console_readline(buf, sizeof(buf), pdMS_TO_TICKS(15000)) == 0) {
            continue;
        }

        float duty = atof(buf);
        if (duty < 0.0f) duty = 0.0f;
        if (duty > 100.0f) duty = 100.0f;

        if (sel >= 1 && sel <= 4) {
            int idx = sel - 1;
            cmd.motor[idx] = duty;
            printf(">> Motor %s = %.1f%%\n", motor_names[idx], duty);
        } else if (sel == 5) {
            for (int i = 0; i < 4; i++) cmd.motor[i] = duty;
            printf(">> All motors = %.1f%%\n", duty);
        }

        motors_set(&cmd);

        // Mostra stato corrente
        printf("   [FL=%.1f%% RL=%.1f%% RR=%.1f%% FR=%.1f%%]\n",
               cmd.motor[0], cmd.motor[1], cmd.motor[2], cmd.motor[3]);
    }

}

// ============================================================================
// Sensor logging mode (originale)
// ============================================================================

static void sensor_log_mode(void)
{
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

    // Header CSV per logging
    printf("timestamp_ms,ax,ay,az,gx,gy,gz,"
           "flow_vx,flow_vy,flow_px,flow_py,flow_raw_x,flow_raw_y,range_mm,quality,"
           "comp_vx,comp_vy,comp_px,comp_py\n");

    imu_data_t imu;
    flow_data_t flow;
    bool have_imu = false, have_flow = false;
    float comp_px = 0.0f, comp_py = 0.0f;
    int64_t prev_flow_ts = 0;

    while (1) {
        have_imu = (imu_read(&imu) == ESP_OK);
        if (flow_read(&flow) == ESP_OK) {
            have_flow = true;
        }

        if (have_imu) {
            int64_t t_ms = imu.timestamp_us / 1000;
            printf("%lld,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f",
                   t_ms,
                   imu.accel_x, imu.accel_y, imu.accel_z,
                   imu.gyro_x, imu.gyro_y, imu.gyro_z);

            if (have_flow) {
                float dt_flow = (prev_flow_ts > 0)
                    ? (flow.timestamp_us - prev_flow_ts) / 1e6f
                    : 0.05f;
                if (dt_flow < 0.001f) dt_flow = 0.001f;
                if (dt_flow > 0.2f) dt_flow = 0.2f;
                prev_flow_ts = flow.timestamp_us;

                float altitude_m = flow.range_mm / 1000.0f;
                float gx_rad = imu.gyro_x * (float)(M_PI / 180.0);
                float gy_rad = imu.gyro_y * (float)(M_PI / 180.0);
                float rot_dx = gy_rad * dt_flow * altitude_m;
                float rot_dy = -gx_rad * dt_flow * altitude_m;
                float flow_dx = (float)FLOW_SIGN_X(flow.raw_x) * FLOW_SCALE_RAD * altitude_m;
                float flow_dy = (float)FLOW_SIGN_Y(flow.raw_y) * FLOW_SCALE_RAD * altitude_m;
                float comp_dx = flow_dx - rot_dx;
                float comp_dy = flow_dy - rot_dy;
                float comp_vx = comp_dx / dt_flow;
                float comp_vy = comp_dy / dt_flow;
                comp_px += comp_dx;
                comp_py += comp_dy;

                printf(",%.4f,%.4f,%.4f,%.4f,%d,%d,%u,%u",
                       flow.vel_x, flow.vel_y,
                       flow.pos_x, flow.pos_y,
                       flow.raw_x, flow.raw_y,
                       flow.range_mm, flow.quality);
                printf(",%.4f,%.4f,%.4f,%.4f",
                       comp_vx, comp_vy, comp_px, comp_py);

                have_flow = false;
            } else {
                printf(",,,,,,,,,,,,");
            }
            printf("\n");
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================================
// app_main — selezione modalità
// ============================================================================

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

    // Inizializza motori (sempre, anche per safety stop)
    esp_err_t ret = motors_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Motors init fallita!");
    }
    motors_stop();  // Safety: assicura duty 0 all'avvio

    // Inizializza input console via USB Serial/JTAG
    console_init();

    // Menu selezione modalità — LED lampeggia per segnalare attesa input
    printf("\n====================================\n");
    printf("  MICROROS-MICRODRONE v0.2\n");
    printf("====================================\n");
    printf("[1] Sensor logging (IMU + Flow)\n");
    printf("[2] Motor test\n");
    printf("Select mode (default=1 in 10s): ");
    fflush(stdout);

    // Lampeggio LED durante attesa
    for (int i = 0; i < 5; i++) {
        gpio_set_level(PIN_LED_STATUS, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(PIN_LED_STATUS, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    char buf[8];
    int mode = 1;  // default: sensor logging
    if (console_readline(buf, sizeof(buf), pdMS_TO_TICKS(10000)) > 0) {
        mode = atoi(buf);
    }

    printf("\n");

    switch (mode) {
    case 2:
        ESP_LOGI(TAG, "Entering MOTOR TEST mode");
        motor_test_mode();
        // Dopo exit dal test, cade nel sensor mode
        ESP_LOGI(TAG, "Motor test finished, starting sensor logging...");
        sensor_log_mode();
        break;
    case 1:
    default:
        ESP_LOGI(TAG, "Entering SENSOR LOGGING mode");
        sensor_log_mode();
        break;
    }
}
