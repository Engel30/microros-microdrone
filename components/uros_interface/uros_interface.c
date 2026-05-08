#include "uros_interface.h"
#include "drone_config.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "driver/temperature_sensor.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>
#include <sensor_msgs/msg/imu.h>
#include <sensor_msgs/msg/range.h>
#include <sensor_msgs/msg/battery_state.h>
#include <sensor_msgs/msg/temperature.h>
#include <geometry_msgs/msg/vector3_stamped.h>
#include <std_msgs/msg/float32_multi_array.h>
#include <std_msgs/msg/bool.h>
#include <rcl_interfaces/msg/log.h>
#include <micro_ros_utilities/string_utilities.h>

static const char *TAG = "uros";

// ============================================================================
// WiFi station — connessione bloccante con retry
// ============================================================================

#define WIFI_CONNECTED_BIT  BIT0

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

// Retry policy: infinito. La connessione WiFi può fallire per cause transitorie
// (AP che kicka durante DHCP, RSSI basso, congestione). Abortire dopo N tentativi
// genera un reboot loop sterile. Meglio loggare e continuare a riprovare —
// stessa filosofia del ping agent in uros_init. Reset del contatore su
// STA_CONNECTED (non solo su GOT_IP) per non consumare retry quando l'associazione
// va a buon fine ma il DHCP fallisce.
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi associato, in attesa IP via DHCP...");
        s_retry_num = 0;
        uros_log(UROS_LOG_INFO, "WiFi associato, attesa DHCP");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        s_retry_num++;
        if (s_retry_num <= 30 || (s_retry_num % 10) == 0) {
            ESP_LOGW(TAG, "WiFi retry %d reason=%d (fix AP/credenziali se persiste)",
                     s_retry_num, d ? d->reason : -1);
            uros_log(UROS_LOG_WARN, "WiFi disconnect retry=%d reason=%d",
                     s_retry_num, d ? d->reason : -1);
        }
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "WiFi connected, IP=" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        uros_log(UROS_LOG_INFO, "WiFi connected IP=" IPSTR, IP2STR(&event->ip_info.ip));
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
    // WPA2-PSK puro o WPA2/WPA3 transition: PMF capable ma non required.
    // pmf=required va attivato SOLO se l'AP è WPA3 (altrimenti il driver
    // rigetta l'AP con "AP not PMF Capable when STA requires, reject profile",
    // reason 210). Threshold OPEN: il driver sceglie il metodo offerto dall'AP.
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    // ALL_CHANNEL_SCAN + sort by RSSI: utile con router dual-band/extender che
    // annunciano la stessa SSID su BSSID multipli. Il default FAST_SCAN prende
    // il primo trovato, che può essere un BSSID 5GHz o con RSSI scarso.
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.rssi = -127;

    // Pulisci eventuale stato WiFi cached in NVS dalla sessione precedente
    // (BSSID memorizzato di un AP non più presente, credenziali stale).
    // Va fatto PRIMA di set_config per non cancellare le credenziali nuove.
    esp_wifi_restore();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    // Power save OFF: alcuni router consumer rifiutano associazioni STA in
    // power-save mode, oppure hanno timeout corti che kickano la STA tra TX.
    // Costo: ~50mW in più, accettabile per drone tethered USB-power.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID=\"%s\" (retry infinito)...", WIFI_SSID);

    // Blocca indefinitamente fino a GOT_IP. Niente abort: il sistema NON deve
    // andare in reboot loop se l'AP è temporaneamente giù o lento al DHCP.
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);
    return ESP_OK;
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
static rcl_publisher_t       s_pub_armed;
static rcl_publisher_t       s_pub_temp;
static rcl_publisher_t       s_pub_log;
static rcl_subscription_t    s_sub_cmd_motor;
static rcl_subscription_t    s_sub_arm;
static rclc_executor_t       s_executor;
static sensor_msgs__msg__Imu              s_msg_imu;
static geometry_msgs__msg__Vector3Stamped s_msg_flow;
static sensor_msgs__msg__Range            s_msg_range;
static sensor_msgs__msg__BatteryState     s_msg_battery;
static std_msgs__msg__Float32MultiArray   s_msg_motors;
static float                              s_motors_data[4];
// Buffer pre-allocato per il messaggio in arrivo. micro-ROS deserializza
// in-place, quindi data.data e data.capacity devono puntare a memoria valida.
static std_msgs__msg__Float32MultiArray   s_msg_cmd_motor;
static float                              s_cmd_motor_data[8];
static std_msgs__msg__Bool                s_msg_arm_in;
static std_msgs__msg__Bool                s_msg_armed_out;
static sensor_msgs__msg__Temperature      s_msg_temp;
static temperature_sensor_handle_t        s_temp_sensor = NULL;
// Flag "publish armed pending": settato dalla callback /arm dopo aver
// aggiornato g_armed; consumato in task_microros che pubblica /armed.
// Pubblicare dalla callback rischierebbe di bloccare l'executor → meglio
// segnalare e pubblicare dal main loop del task.
static volatile bool         s_armed_pub_pending = false;
static QueueHandle_t                      s_cmd_queue_ref = NULL;
static bool                  s_uros_ready = false;

// ----- Log /drone_1/log: queue + publisher + buffer pre-allocato ------------
// Un item porta livello + timestamp + stringa pre-formattata (vsnprintf nel
// chiamante). La queue assorbe burst di log e disaccoppia chi logga dal task
// micro-ROS che pubblica realmente. Dimensione queue * sizeof(item) tenuta
// piccola (16 * 208 ≈ 3.3 KB) per non sprecare RAM.
#define UROS_LOG_MSG_LEN   192
#define UROS_LOG_QUEUE_LEN 16
typedef struct {
    uint8_t  level;
    int64_t  ts_us;
    char     msg[UROS_LOG_MSG_LEN];
} log_item_t;
static QueueHandle_t              s_log_queue = NULL;
static rcl_interfaces__msg__Log   s_msg_log;
static char                       s_log_msg_buf[UROS_LOG_MSG_LEN];
static char                       s_log_name_buf[16] = "drone";
static char                       s_log_empty_buf[1] = {0};

// API pubblica: emette un log su /drone_1/log. Non-blocking, drop on full.
// Sicura prima di uros_init (no-op se la queue non esiste). NON chiamare
// da ISR (xQueueSend richiede contesto task).
void uros_log(uros_log_level_t level, const char *fmt, ...)
{
    if (!s_log_queue) return;
    log_item_t it;
    it.level = (uint8_t)level;
    it.ts_us = esp_timer_get_time();
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(it.msg, sizeof(it.msg), fmt, ap);
    va_end(ap);
    (void)xQueueSend(s_log_queue, &it, 0);
}

// Callback subscriber /drone_1/cmd_motor_test
// Validazione: data.size deve essere == 4, altrimenti scarta + log warning.
// Clamp 0-100 per ciascun duty. Scrive su cmd_queue con xQueueOverwrite
// (depth=1: sempre l'ultimo comando vince). task_motors aggiornerà il proprio
// timestamp watchdog quando legge dalla queue.
static void cmd_motor_test_cb(const void *msgin)
{
    const std_msgs__msg__Float32MultiArray *m = (const std_msgs__msg__Float32MultiArray *)msgin;

    if (m == NULL || m->data.size != 4) {
        unsigned got = (unsigned)(m ? m->data.size : 0);
        ESP_LOGW(TAG, "cmd_motor_test: size=%u atteso 4 — scartato", got);
        uros_log(UROS_LOG_WARN, "cmd_motor_test size=%u (atteso 4) scartato", got);
        return;
    }

    motor_cmd_t cmd = { .timestamp_us = esp_timer_get_time() };
    for (int i = 0; i < 4; i++) {
        float v = m->data.data[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 100.0f) v = 100.0f;
        cmd.motor[i] = v;
    }

    if (s_cmd_queue_ref) {
        xQueueOverwrite(s_cmd_queue_ref, &cmd);
    }
}

// Callback subscriber /drone_1/arm — std_msgs/Bool RELIABLE.
// Aggiorna il flag atomico g_armed (letto da task_motors). Pubblica /armed
// solo on-change: il main loop di task_microros consuma s_armed_pub_pending.
// Stato sticky: cambia solo su nuovo messaggio, niente auto-disarm.
static void arm_cb(const void *msgin)
{
    const std_msgs__msg__Bool *m = (const std_msgs__msg__Bool *)msgin;
    if (m == NULL) return;

    bool req = m->data;
    bool prev = atomic_exchange(&g_armed, req);
    if (prev != req) {
        ESP_LOGW(TAG, "ARM state: %s -> %s",
                 prev ? "ARMED" : "DISARMED",
                 req  ? "ARMED" : "DISARMED");
        uros_log(UROS_LOG_WARN, "ARM %s -> %s",
                 prev ? "ARMED" : "DISARMED",
                 req  ? "ARMED" : "DISARMED");
        s_armed_pub_pending = true;
    }
}

esp_err_t uros_init(const uros_queues_t *queues)
{
    if (!queues) return ESP_ERR_INVALID_ARG;
    s_cmd_queue_ref = queues->cmd_queue;

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

    // Publisher RELIABLE per stato armed (Bool, on-change, low rate).
    RCCHECK(rclc_publisher_init_default(
        &s_pub_armed, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
        "armed"));

    // Publisher BEST_EFFORT per temperatura on-die ESP32-S3 @ 1Hz.
    RCCHECK(rclc_publisher_init_best_effort(
        &s_pub_temp, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Temperature),
        "temp"));

    // Publisher BEST_EFFORT per console di debug Foxglove (Log panel).
    // BEST_EFFORT: i log non sono safety-critical e non devono saturare il
    // canale RELIABLE. Foxglove riconosce rcl_interfaces/Log nativamente.
    RCCHECK(rclc_publisher_init_best_effort(
        &s_pub_log, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(rcl_interfaces, msg, Log),
        "log"));

    // Init Log msg con buffer statici. Niente alloc per publish: assegniamo
    // s_log_msg_buf a msg.data una volta sola e aggiorniamo size/contenuto
    // ad ogni pubblicazione. file/function vuoti (saving banda + non utili
    // dato che logghiamo da task FreeRTOS, non da file C ROS2).
    rcl_interfaces__msg__Log__init(&s_msg_log);
    s_msg_log.name.data     = s_log_name_buf;
    s_msg_log.name.capacity = sizeof(s_log_name_buf);
    s_msg_log.name.size     = strlen(s_log_name_buf);
    s_msg_log.msg.data      = s_log_msg_buf;
    s_msg_log.msg.capacity  = sizeof(s_log_msg_buf);
    s_msg_log.msg.size      = 0;
    s_msg_log.file.data      = s_log_empty_buf;
    s_msg_log.file.capacity  = sizeof(s_log_empty_buf);
    s_msg_log.file.size      = 0;
    s_msg_log.function.data      = s_log_empty_buf;
    s_msg_log.function.capacity  = sizeof(s_log_empty_buf);
    s_msg_log.function.size      = 0;
    s_msg_log.line = 0;

    // Queue log creata DOPO il publisher: uros_log() è no-op finché s_log_queue
    // è NULL, quindi log "early" (es. da event handler WiFi pre-init) sono
    // semplicemente scartati senza crash.
    s_log_queue = xQueueCreate(UROS_LOG_QUEUE_LEN, sizeof(log_item_t));

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

    // Subscriber RELIABLE per cmd_motor_test (Float32MultiArray, attesi 4 valori)
    RCCHECK(rclc_subscription_init_default(
        &s_sub_cmd_motor, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
        "cmd_motor_test"));

    // Buffer di deserializzazione: capacity > 4 per gestire publish "lunghi"
    // (la callback li scarterà comunque), evita errori out-of-buffer.
    std_msgs__msg__Float32MultiArray__init(&s_msg_cmd_motor);
    s_msg_cmd_motor.data.data     = s_cmd_motor_data;
    s_msg_cmd_motor.data.size     = 0;
    s_msg_cmd_motor.data.capacity = sizeof(s_cmd_motor_data) / sizeof(s_cmd_motor_data[0]);

    // Init Temperature msg (sensor_msgs/Temperature: temp °C + variance)
    sensor_msgs__msg__Temperature__init(&s_msg_temp);
    s_msg_temp.header.frame_id = micro_ros_string_utilities_set(s_msg_temp.header.frame_id, "esp32_die");
    s_msg_temp.variance = 4.0;  // ±2°C accuracy nominale → varianza 2² = 4

    // Sensore temperatura on-die ESP32-S3. Range 10-50°C tipico per il drone
    // tethered (l'IDF richiede di scegliere un range; più stretto = più accurato).
    temperature_sensor_config_t tcfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    if (temperature_sensor_install(&tcfg, &s_temp_sensor) == ESP_OK) {
        temperature_sensor_enable(s_temp_sensor);
    } else {
        ESP_LOGW(TAG, "temperature_sensor_install fallito — /temp non sarà accurato");
        s_temp_sensor = NULL;
    }

    // Subscriber RELIABLE per /drone_1/arm (Bool). Sticky lato firmware:
    // g_armed cambia solo alla ricezione di un nuovo messaggio.
    RCCHECK(rclc_subscription_init_default(
        &s_sub_arm, &s_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
        "arm"));
    std_msgs__msg__Bool__init(&s_msg_arm_in);
    std_msgs__msg__Bool__init(&s_msg_armed_out);

    // Executor con 2 handle (cmd_motor_test + arm). I publisher non passano
    // dall'executor: pubblichiamo direttamente da task_microros.
    RCCHECK(rclc_executor_init(&s_executor, &s_support.context, 2, &s_allocator));
    RCCHECK(rclc_executor_add_subscription(
        &s_executor, &s_sub_cmd_motor, &s_msg_cmd_motor,
        &cmd_motor_test_cb, ON_NEW_DATA));
    RCCHECK(rclc_executor_add_subscription(
        &s_executor, &s_sub_arm, &s_msg_arm_in,
        &arm_cb, ON_NEW_DATA));

    // Publish stato armed iniziale (DISARMED al boot). Aiuta i client che si
    // collegano subito dopo l'init a sapere lo stato senza dover toccare arm.
    s_msg_armed_out.data = atomic_load(&g_armed);
    RCSOFT(rcl_publish(&s_pub_armed, &s_msg_armed_out, NULL));

    s_uros_ready = true;
    ESP_LOGI(TAG, "uros_init OK: node=/%s/%s pub=/%s/imu/raw",
             UROS_NAMESPACE, UROS_NODE_NAME, UROS_NAMESPACE);

    // Logga il motivo del reset precedente su /log: utile per diagnosticare
    // brownout dovuti a buck-boost / supply insufficiente dai motori senza
    // doversi attaccare al monitor seriale. Pubblicato come WARN se anomalo,
    // INFO se reset normale (POWERON / SW).
    esp_reset_reason_t rr = esp_reset_reason();
    const char *rrstr = "UNKNOWN";
    uros_log_level_t lvl = UROS_LOG_INFO;
    switch (rr) {
        case ESP_RST_POWERON:  rrstr = "POWERON";    break;
        case ESP_RST_EXT:      rrstr = "EXT_PIN";    lvl = UROS_LOG_WARN; break;
        case ESP_RST_SW:       rrstr = "SW";         break;
        case ESP_RST_PANIC:    rrstr = "PANIC";      lvl = UROS_LOG_ERROR; break;
        case ESP_RST_INT_WDT:  rrstr = "INT_WDT";    lvl = UROS_LOG_ERROR; break;
        case ESP_RST_TASK_WDT: rrstr = "TASK_WDT";   lvl = UROS_LOG_ERROR; break;
        case ESP_RST_WDT:      rrstr = "OTHER_WDT";  lvl = UROS_LOG_ERROR; break;
        case ESP_RST_DEEPSLEEP:rrstr = "DEEPSLEEP";  break;
        case ESP_RST_BROWNOUT: rrstr = "BROWNOUT";   lvl = UROS_LOG_ERROR; break;
        case ESP_RST_SDIO:     rrstr = "SDIO";       lvl = UROS_LOG_WARN; break;
        default: break;
    }
    uros_log(lvl, "boot reset_reason=%s (%d)", rrstr, (int)rr);
    uros_log(UROS_LOG_INFO, "uROS ready: node=/%s/%s, agent=%s:%s",
             UROS_NAMESPACE, UROS_NODE_NAME, UROS_AGENT_IP, STR(UROS_AGENT_PORT));
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
    uint32_t flow_pub = 0, batt_pub = 0, motor_pub = 0, temp_pub = 0;
    int64_t last_log_us = esp_timer_get_time();
    int64_t last_temp_us = 0;

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

        // Drain log queue: max 5 messaggi per ciclo (10ms) → cap effettivo 500/s.
        // BEST_EFFORT, msg pre-allocato (niente malloc), file/function vuoti.
        if (s_log_queue) {
            log_item_t lit;
            int log_pulled = 0;
            while (log_pulled < 5 && xQueueReceive(s_log_queue, &lit, 0) == pdTRUE) {
                stamp_from_us(&s_msg_log.stamp, lit.ts_us);
                s_msg_log.level = lit.level;
                size_t n = strnlen(lit.msg, sizeof(lit.msg));
                if (n >= s_msg_log.msg.capacity) n = s_msg_log.msg.capacity - 1;
                memcpy(s_msg_log.msg.data, lit.msg, n);
                s_msg_log.msg.data[n] = '\0';
                s_msg_log.msg.size = n;
                RCSOFT(rcl_publish(&s_pub_log, &s_msg_log, NULL));
                log_pulled++;
            }
        }

        // Spin executor per eseguire le callback dei subscriber (cmd_motor_test, arm).
        // Timeout corto (5ms) per non sforare il periodo del task (10ms @ 100Hz).
        rclc_executor_spin_some(&s_executor, RCL_MS_TO_NS(5));

        // Publish temperatura on-die ESP32-S3 @ ~1Hz (gated da timestamp).
        int64_t now_us = esp_timer_get_time();
        if (s_temp_sensor && (now_us - last_temp_us) >= 1000000) {
            float tc = 0.0f;
            if (temperature_sensor_get_celsius(s_temp_sensor, &tc) == ESP_OK) {
                stamp_from_us(&s_msg_temp.header.stamp, now_us);
                s_msg_temp.temperature = tc;
                RCSOFT(rcl_publish(&s_pub_temp, &s_msg_temp, NULL));
                temp_pub++;
            }
            last_temp_us = now_us;
        }

        // Publish /armed on-change. La callback arm_cb setta s_armed_pub_pending
        // dopo aver aggiornato g_armed; qui facciamo il publish vero (fuori dal
        // contesto della callback per non bloccare l'executor).
        if (s_armed_pub_pending) {
            s_armed_pub_pending = false;
            s_msg_armed_out.data = atomic_load(&g_armed);
            RCSOFT(rcl_publish(&s_pub_armed, &s_msg_armed_out, NULL));
        }

        // Log diagnostico ogni 1s
        int64_t now = esp_timer_get_time();
        if (now - last_log_us >= 1000000) {
            ESP_LOGI(TAG, "pub: imu=%lu (drain=%lu) flow=%lu batt=%lu motors=%lu temp=%lu",
                     (unsigned long)imu_pub, (unsigned long)imu_drain,
                     (unsigned long)flow_pub, (unsigned long)batt_pub,
                     (unsigned long)motor_pub, (unsigned long)temp_pub);
            imu_drain = imu_pub = flow_pub = batt_pub = motor_pub = temp_pub = 0;
            last_log_us = now;
        }

        vTaskDelayUntil(&last, period);
    }
}
