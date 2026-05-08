#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "drone_config.h"
#include "drone_types.h"
#include "imu_driver.h"
#include "flow_driver.h"
#include "motor_driver.h"
#include "battery_monitor.h"
#include "uros_interface.h"

static const char *TAG = "main";

// Definizione del flag globale di arm. Boot DISARMED per safety: nessun
// comando residuo può far girare i motori prima di un publish esplicito su
// /drone_1/arm. Dichiarazione in components/common/include/drone_types.h.
atomic_bool g_armed = false;

// Forward declaration dei task definiti in main/task_*.c
void task_imu(void *arg);
void task_flow(void *arg);
void task_battery(void *arg);
void task_motors(void *arg);

// ============================================================================
// Console helpers — lettura linea bloccante / con timeout (USB Serial/JTAG)
// ============================================================================

static const char *motor_names[4] = { "FL", "RL", "RR", "FR" };

static int console_readline(char *buf, int maxlen)
{
    int pos = 0;
    while (pos < maxlen - 1) {
        int c = fgetc(stdin);
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (pos > 0) break;
            continue;
        }
        buf[pos++] = (char)c;
    }
    buf[pos] = '\0';
    return pos;
}

// Legge un intero da stdin con timeout. Se l'utente non risponde entro
// timeout_ms, ritorna default_mode. Buffer locale fino a Invio o EOF.
static int read_mode_with_timeout(int timeout_ms, int default_mode)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    char buf[16];
    int pos = 0;
    while (esp_timer_get_time() < deadline) {
        int c = fgetc(stdin);
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                buf[pos] = '\0';
                return atoi(buf);
            }
            continue;
        }
        if (pos < (int)sizeof(buf) - 1) {
            buf[pos++] = (char)c;
        }
    }
    return default_mode;
}

// ============================================================================
// Modalità test motori — menu interattivo via monitor seriale (legacy)
// ============================================================================

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
        if (console_readline(buf, sizeof(buf)) == 0) continue;

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
        if (sel < 1 || sel > 5) {
            printf(">> Invalid selection\n");
            continue;
        }

        printf("Duty %% (0-100): ");
        fflush(stdout);
        if (console_readline(buf, sizeof(buf)) == 0) continue;

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
        printf("   [FL=%.1f%% RL=%.1f%% RR=%.1f%% FR=%.1f%%]\n",
               cmd.motor[0], cmd.motor[1], cmd.motor[2], cmd.motor[3]);
    }
}

// ============================================================================
// Sensor logging mode (legacy CSV)
// ============================================================================

static void sensor_log_mode(void)
{
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
        if (flow_read(&flow) == ESP_OK) have_flow = true;

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
// uROS mode — multi-task FreeRTOS + bridge micro-ROS
// ============================================================================

// Le queue devono sopravvivere fino a fine programma. uros_queues_t è static
// così l'indirizzo passato ai task resta valido dopo che uros_mode ritorna.
static uros_queues_t Q;

static void uros_mode(void)
{
    ESP_LOGI(TAG, "=== uROS MODE ===");

    Q.imu_queue        = xQueueCreate(QUEUE_DEPTH_IMU,   sizeof(imu_data_t));
    Q.flow_queue       = xQueueCreate(QUEUE_DEPTH_FLOW,  sizeof(flow_data_t));
    Q.battery_queue    = xQueueCreate(QUEUE_DEPTH_BATT,  sizeof(float));
    Q.cmd_queue        = xQueueCreate(QUEUE_DEPTH_CMD,   sizeof(motor_cmd_t));
    Q.motor_echo_queue = xQueueCreate(QUEUE_DEPTH_MOTOR, sizeof(motor_cmd_t));
    if (!Q.imu_queue || !Q.flow_queue || !Q.battery_queue ||
        !Q.cmd_queue || !Q.motor_echo_queue) {
        ESP_LOGE(TAG, "Queue create fallita");
        return;
    }

    // Step 4-5: WiFi + uROS init (qui sono stub e tornano OK)
    ESP_ERROR_CHECK(uros_wifi_connect());
    ESP_ERROR_CHECK(uros_init(&Q));

    // Core 1 (flight)
    xTaskCreatePinnedToCore(task_imu,    "imu",    STACK_IMU,    &Q, PRIO_TASK_IMU,    NULL, 1);
    xTaskCreatePinnedToCore(task_flow,   "flow",   STACK_FLOW,   &Q, PRIO_TASK_FLOW,   NULL, 1);
    xTaskCreatePinnedToCore(task_motors, "motors", STACK_MOTORS, &Q, PRIO_TASK_MOTORS, NULL, 1);

    // Core 0 (comms)
    xTaskCreatePinnedToCore(task_battery,  "batt", STACK_BATTERY,  &Q, PRIO_TASK_BATTERY, NULL, 0);
    xTaskCreatePinnedToCore(task_microros, "uros", STACK_MICROROS, &Q, PRIO_TASK_MICROROS, NULL, 0);

    ESP_LOGI(TAG, "Tutti i task lanciati. app_main termina, scheduler tiene attivo il sistema.");
}

// ============================================================================
// app_main — selezione modalità
// ============================================================================

void app_main(void)
{
    ESP_LOGI(TAG, "microros-microdrone starting - Drone ID: %d", DRONE_ID);

    // LED status
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << PIN_LED_STATUS),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(PIN_LED_STATUS, 1);

    // Motor driver init + stop (safety, anche prima di tutto)
    if (motors_init() != ESP_OK) ESP_LOGE(TAG, "motors_init fallita");
    motors_stop();

    // Sensori comuni a tutte le modalità
    if (imu_init() != ESP_OK) ESP_LOGE(TAG, "imu_init fallita");
    if (flow_init() != ESP_OK) ESP_LOGE(TAG, "flow_init fallita");
    if (battery_init() != ESP_OK) ESP_LOGE(TAG, "battery_init fallita");

    // Calibrazione IMU (drone fermo!)
    ESP_LOGI(TAG, "Calibrazione IMU... TIENI FERMO");
    imu_calibrate(500);

    // Attesa monitor USB (lampeggi LED ~3s)
    for (int i = 0; i < 10; i++) {
        gpio_set_level(PIN_LED_STATUS, 0);
        vTaskDelay(pdMS_TO_TICKS(150));
        gpio_set_level(PIN_LED_STATUS, 1);
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    // Menu (timeout 3s → default uROS mode)
    printf("\n====================================\n");
    printf("  MICROROS-MICRODRONE v0.3\n");
    printf("====================================\n");
    printf("[1] uROS mode (multi-task + WiFi) [DEFAULT]\n");
    printf("[2] Sensor logging (CSV)\n");
    printf("[3] Motor test (legacy menu)\n");
    printf("Select mode (3s timeout): ");
    fflush(stdout);

    int mode = read_mode_with_timeout(3000, 1);
    printf("\n");

    switch (mode) {
    case 2:
        ESP_LOGI(TAG, "Entering SENSOR LOGGING mode");
        sensor_log_mode();
        break;
    case 3:
        ESP_LOGI(TAG, "Entering MOTOR TEST mode");
        motor_test_mode();
        break;
    case 1:
    default:
        ESP_LOGI(TAG, "Entering uROS mode");
        uros_mode();
        // app_main ritorna, scheduler tiene vivi i task
        break;
    }
}
