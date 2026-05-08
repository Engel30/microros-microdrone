#include "uros_interface.h"
#include "drone_config.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <math.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>
#include <sensor_msgs/msg/imu.h>
#include <sensor_msgs/msg/range.h>
#include <sensor_msgs/msg/battery_state.h>
#include <geometry_msgs/msg/vector3_stamped.h>
#include <std_msgs/msg/float32_multi_array.h>
#include <micro_ros_utilities/string_utilities.h>

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
    // Transition WPA2/WPA3: nessun threshold, lascia che il driver scelga
    // il metodo di auth offerto dall'AP. PMF capable abilitato (richiesto da WPA3-SAE).
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

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

// ============================================================================
// micro-ROS — Step 5: support, node, publisher /drone_1/imu/raw
// ============================================================================

#define RCCHECK(fn) do { rcl_ret_t _rc = (fn); if (_rc != RCL_RET_OK) { \
    ESP_LOGE(TAG, "rcl error %d at line %d", (int)_rc, __LINE__); return ESP_FAIL; } } while (0)
#define RCSOFT(fn) do { rcl_ret_t _rc = (fn); if (_rc != RCL_RET_OK) { \
    ESP_LOGW(TAG, "rcl soft error %d at line %d", (int)_rc, __LINE__); } } while (0)

#define STR_HELPER(x) #x
#define STR(x)        STR_HELPER(x)

// Conversioni unità: drone_types.h ha accel in g, gyro in deg/s.
// sensor_msgs/Imu vuole accel in m/s² e angular_velocity in rad/s.
#define G_TO_MS2     9.80665f
#define DEG_TO_RAD   0.017453292519943295f

static rclc_support_t        s_support;
static rcl_node_t            s_node;
static rcl_allocator_t       s_allocator;
static rcl_publisher_t       s_pub_imu;
static rcl_publisher_t       s_pub_flow;
static rcl_publisher_t       s_pub_range;
static rcl_publisher_t       s_pub_battery;
static rcl_publisher_t       s_pub_motors;
static sensor_msgs__msg__Imu              s_msg_imu;
static geometry_msgs__msg__Vector3Stamped s_msg_flow;
static sensor_msgs__msg__Range            s_msg_range;
static sensor_msgs__msg__BatteryState     s_msg_battery;
static std_msgs__msg__Float32MultiArray   s_msg_motors;
static float                              s_motors_data[4];
static bool                  s_uros_ready = false;

esp_err_t uros_init(const uros_queues_t *queues)
{
    if (!queues) return ESP_ERR_INVALID_ARG;

    s_allocator = rcl_get_default_allocator();

    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    RCCHECK(rcl_init_options_init(&init_options, s_allocator));

    rmw_init_options_t *rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
    RCCHECK(rmw_uros_options_set_udp_address(UROS_AGENT_IP,
                                             STR(UROS_AGENT_PORT),
                                             rmw_options));

    ESP_LOGI(TAG, "uROS agent target = %s:%s", UROS_AGENT_IP, STR(UROS_AGENT_PORT));

    // Ping retry: aspetta che l'agent sia raggiungibile prima di support_init.
    // Senza questo, se l'agent è giù si va in panic e reboot in loop.
    // Loop infinito ma non bloccante: log ogni tentativo, sleep 1s.
    ESP_LOGI(TAG, "Ping agent in attesa che sia online...");
    int ping_retries = 0;
    while (rmw_uros_ping_agent_options(1000, 1, rmw_options) != RMW_RET_OK) {
        if (++ping_retries % 10 == 1) {
            ESP_LOGW(TAG, "Agent non risponde (tentativo %d), continuo a riprovare...",
                     ping_retries);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Agent reachable dopo %d tentativi", ping_retries);

    // support_init: se fallisce comunque, retry fino a successo
    int sup_retries = 0;
    while (rclc_support_init_with_options(&s_support, 0, NULL,
                                          &init_options, &s_allocator) != RCL_RET_OK) {
        ESP_LOGW(TAG, "support_init fallito, retry %d", ++sup_retries);
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (sup_retries >= 30) {
            ESP_LOGE(TAG, "support_init fallito 30 volte, abort");
            return ESP_FAIL;
        }
    }

    RCCHECK(rclc_node_init_default(&s_node, UROS_NODE_NAME, UROS_NAMESPACE, &s_support));

    // Publisher BEST_EFFORT per IMU @ 100Hz
    RCCHECK(rclc_publisher_init_best_effort(
        &s_pub_imu, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
        "imu/raw"));
    RCCHECK(rclc_publisher_init_best_effort(
        &s_pub_flow, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Vector3Stamped),
        "flow"));
    RCCHECK(rclc_publisher_init_best_effort(
        &s_pub_range, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Range),
        "range"));
    RCCHECK(rclc_publisher_init_best_effort(
        &s_pub_battery, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, BatteryState),
        "battery"));
    RCCHECK(rclc_publisher_init_best_effort(
        &s_pub_motors, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
        "motors"));

    // Init messaggio IMU: covarianze a -1 (sconosciute, convenzione ROS2)
    sensor_msgs__msg__Imu__init(&s_msg_imu);
    s_msg_imu.header.frame_id = micro_ros_string_utilities_set(s_msg_imu.header.frame_id, "imu_link");
    s_msg_imu.orientation_covariance[0] = -1.0;
    s_msg_imu.angular_velocity_covariance[0] = -1.0;
    s_msg_imu.linear_acceleration_covariance[0] = -1.0;

    // Init Flow (Vector3Stamped: header + Vector3 con vel_x, vel_y, 0)
    geometry_msgs__msg__Vector3Stamped__init(&s_msg_flow);
    s_msg_flow.header.frame_id = micro_ros_string_utilities_set(s_msg_flow.header.frame_id, "flow_link");

    // Init Range (VL53L1X ToF: 40mm – 4000mm tipici, FoV ~27°)
    sensor_msgs__msg__Range__init(&s_msg_range);
    s_msg_range.header.frame_id = micro_ros_string_utilities_set(s_msg_range.header.frame_id, "range_link");
    s_msg_range.radiation_type = sensor_msgs__msg__Range__INFRARED;
    s_msg_range.field_of_view  = 0.471f;   // ~27° in rad
    s_msg_range.min_range      = 0.04f;
    s_msg_range.max_range      = 4.0f;

    // Init BatteryState (1S LiPo 3.7V nominale)
    sensor_msgs__msg__BatteryState__init(&s_msg_battery);
    s_msg_battery.header.frame_id   = micro_ros_string_utilities_set(s_msg_battery.header.frame_id, "battery_link");
    s_msg_battery.design_capacity   = 0.300f;   // mAh tipico (placeholder)
    s_msg_battery.power_supply_status      = sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_UNKNOWN;
    s_msg_battery.power_supply_health      = sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_UNKNOWN;
    s_msg_battery.power_supply_technology  = sensor_msgs__msg__BatteryState__POWER_SUPPLY_TECHNOLOGY_LIPO;
    s_msg_battery.present           = true;
    s_msg_battery.location          = micro_ros_string_utilities_set(s_msg_battery.location, "");
    s_msg_battery.serial_number     = micro_ros_string_utilities_set(s_msg_battery.serial_number, "");
    // current/charge/percentage = NaN per "unknown" (convenzione BatteryState)
    s_msg_battery.current     = NAN;
    s_msg_battery.charge      = NAN;
    s_msg_battery.capacity    = NAN;
    s_msg_battery.percentage  = NAN;
    s_msg_battery.temperature = NAN;

    // Init Motors (Float32MultiArray a 4 elementi: FL, RL, RR, FR — duty 0-100)
    std_msgs__msg__Float32MultiArray__init(&s_msg_motors);
    s_msg_motors.data.data     = s_motors_data;
    s_msg_motors.data.size     = 4;
    s_msg_motors.data.capacity = 4;
    memset(s_motors_data, 0, sizeof(s_motors_data));

    s_uros_ready = true;
    ESP_LOGI(TAG, "uros_init OK: node=/%s/%s pub=/%s/imu/raw",
             UROS_NAMESPACE, UROS_NODE_NAME, UROS_NAMESPACE);
    return ESP_OK;
}

// Riempie il messaggio Imu dai dati raw del MPU6050 (units conversion + timestamp)
static void fill_imu_msg(const imu_data_t *src)
{
    int64_t ns = src->timestamp_us * 1000LL;
    s_msg_imu.header.stamp.sec = (int32_t)(ns / 1000000000LL);
    s_msg_imu.header.stamp.nanosec = (uint32_t)(ns % 1000000000LL);

    s_msg_imu.linear_acceleration.x = src->accel_x * G_TO_MS2;
    s_msg_imu.linear_acceleration.y = src->accel_y * G_TO_MS2;
    s_msg_imu.linear_acceleration.z = src->accel_z * G_TO_MS2;

    s_msg_imu.angular_velocity.x = src->gyro_x * DEG_TO_RAD;
    s_msg_imu.angular_velocity.y = src->gyro_y * DEG_TO_RAD;
    s_msg_imu.angular_velocity.z = src->gyro_z * DEG_TO_RAD;

    // orientation lasciata a zero (non stimata), covariance[0]=-1 segnala "unknown"
}

// Helpers di fill per gli altri messaggi
static inline void stamp_from_us(builtin_interfaces__msg__Time *t, int64_t us)
{
    int64_t ns = us * 1000LL;
    t->sec = (int32_t)(ns / 1000000000LL);
    t->nanosec = (uint32_t)(ns % 1000000000LL);
}

static void fill_flow_msg(const flow_data_t *src)
{
    stamp_from_us(&s_msg_flow.header.stamp, src->timestamp_us);
    s_msg_flow.vector.x = src->vel_x;
    s_msg_flow.vector.y = src->vel_y;
    s_msg_flow.vector.z = 0.0;
}

static void fill_range_msg(const flow_data_t *src)
{
    stamp_from_us(&s_msg_range.header.stamp, src->timestamp_us);
    s_msg_range.range = (float)src->range_mm * 0.001f;  // mm → m
}

static void fill_battery_msg(float volt)
{
    stamp_from_us(&s_msg_battery.header.stamp, esp_timer_get_time());
    s_msg_battery.voltage = volt;
    // Stima percentuale lineare 3.0V (0%) → 4.2V (100%) — approssimazione 1S LiPo
    float pct = (volt - 3.0f) / (4.2f - 3.0f);
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    s_msg_battery.percentage = pct;
}

static void fill_motors_msg(const motor_cmd_t *src)
{
    s_motors_data[0] = src->motor[0];
    s_motors_data[1] = src->motor[1];
    s_motors_data[2] = src->motor[2];
    s_motors_data[3] = src->motor[3];
}

// Step 6: pubblica IMU @ 100Hz, Flow + Range @ ~20Hz, Battery @ 1Hz, Motors echo @ ~50Hz.
// Strategia: drain di ogni queue ad ogni ciclo del task (10ms),
// publish solo se c'è dato nuovo. La frequenza naturale dei produttori
// determina la freq di publish (no decimazione esplicita per gli altri topic).
void task_microros(void *arg)
{
    uros_queues_t *Q = (uros_queues_t *)arg;
    ESP_LOGI(TAG, "task_microros (Step 6) avviato — task @ %d Hz", FREQ_MICROROS_HZ);

    if (!s_uros_ready) {
        ESP_LOGE(TAG, "uros_init non completato, task terminato");
        vTaskDelete(NULL);
        return;
    }

    imu_data_t imu, imu_last;
    flow_data_t flow, flow_last;
    float volt, volt_last = 0.0f;
    motor_cmd_t mecho, mecho_last;
    bool have_imu = false, have_flow = false, have_batt = false, have_mecho = false;

    uint32_t imu_drain = 0, imu_pub = 0;
    uint32_t flow_pub = 0, batt_pub = 0, motor_pub = 0;
    int64_t last_log_us = esp_timer_get_time();

    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / FREQ_MICROROS_HZ);  // 10ms @ 100Hz

    while (1) {
        // Drena imu_queue, conserva l'ultimo
        while (xQueueReceive(Q->imu_queue, &imu, 0) == pdTRUE) {
            imu_last = imu;
            have_imu = true;
            imu_drain++;
        }
        while (xQueueReceive(Q->flow_queue, &flow, 0) == pdTRUE) {
            flow_last = flow;
            have_flow = true;
        }
        while (xQueueReceive(Q->battery_queue, &volt, 0) == pdTRUE) {
            volt_last = volt;
            have_batt = true;
        }
        while (xQueueReceive(Q->motor_echo_queue, &mecho, 0) == pdTRUE) {
            mecho_last = mecho;
            have_mecho = true;
        }

        if (have_imu) {
            fill_imu_msg(&imu_last);
            RCSOFT(rcl_publish(&s_pub_imu, &s_msg_imu, NULL));
            imu_pub++;
            have_imu = false;
        }
        if (have_flow) {
            fill_flow_msg(&flow_last);
            RCSOFT(rcl_publish(&s_pub_flow, &s_msg_flow, NULL));
            fill_range_msg(&flow_last);
            RCSOFT(rcl_publish(&s_pub_range, &s_msg_range, NULL));
            flow_pub++;
            have_flow = false;
        }
        if (have_batt) {
            fill_battery_msg(volt_last);
            RCSOFT(rcl_publish(&s_pub_battery, &s_msg_battery, NULL));
            batt_pub++;
            have_batt = false;
        }
        if (have_mecho) {
            fill_motors_msg(&mecho_last);
            RCSOFT(rcl_publish(&s_pub_motors, &s_msg_motors, NULL));
            motor_pub++;
            have_mecho = false;
        }

        // Log diagnostico ogni 1s
        int64_t now = esp_timer_get_time();
        if (now - last_log_us >= 1000000) {
            ESP_LOGI(TAG, "pub: imu=%lu (drain=%lu) flow=%lu batt=%lu motors=%lu",
                     (unsigned long)imu_pub, (unsigned long)imu_drain,
                     (unsigned long)flow_pub, (unsigned long)batt_pub,
                     (unsigned long)motor_pub);
            imu_drain = imu_pub = flow_pub = batt_pub = motor_pub = 0;
            last_log_us = now;
        }

        vTaskDelayUntil(&last, period);
    }
}
