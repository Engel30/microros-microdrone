#include "uros_interface.h"
#include "drone_config.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "uros";

// ============================================================================
// WiFi station — connessione bloccante con retry
// ============================================================================

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRY      10
#define WIFI_TIMEOUT_MS     30000

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "WiFi retry %d/%d", s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "WiFi connected, IP=" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t uros_wifi_connect(void)
{
    // NVS: WiFi richiede storage non volatile per parametri di calibrazione
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any_id, inst_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &inst_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &inst_got_ip));

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID=\"%s\"...", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(WIFI_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi connect FAILED dopo %d retry", WIFI_MAX_RETRY);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi connect TIMEOUT (%dms)", WIFI_TIMEOUT_MS);
        return ESP_ERR_TIMEOUT;
    }
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
