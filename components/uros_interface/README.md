# uros_interface

Bridge tra micro-ROS (ROS2) e le FreeRTOS queues interne del drone.

## Stato: Step 6 completato (5 publisher attivi). Step 7 pending (subscriber `cmd_motor_test`).

## API

- `uros_wifi_connect()` — Connessione WiFi station bloccante (timeout 30s, retry 10).
- `uros_init(queues)` — Init micro-ROS: ping retry agent → `rclc_support` → `node` → 5 publisher → init messaggi. Non blocca su agent assente: aspetta indefinitamente.
- `task_microros(arg)` — Task FreeRTOS @ 100Hz, drena le queue produttori e pubblica.

## Topic pubblicati (Step 6)

| Topic | Tipo messaggio | Freq | QoS |
|-------|---------------|------|-----|
| `/drone_1/imu/raw` | `sensor_msgs/msg/Imu` | 100 Hz | BEST_EFFORT |
| `/drone_1/flow` | `geometry_msgs/msg/Vector3Stamped` | ~20 Hz | BEST_EFFORT |
| `/drone_1/range` | `sensor_msgs/msg/Range` | ~20 Hz | BEST_EFFORT |
| `/drone_1/battery` | `sensor_msgs/msg/BatteryState` | 1 Hz | BEST_EFFORT |
| `/drone_1/motors` | `std_msgs/msg/Float32MultiArray` | ~100 Hz (echo) | BEST_EFFORT |

Frame ID: `imu_link`, `flow_link`, `range_link`, `battery_link`. Timestamp via `esp_timer_get_time()` (microsecondi monotonic, NON sincronizzato con clock agent).

## Topic sottoscritti (pendenti)

| Topic | Tipo messaggio | Step |
|-------|---------------|------|
| `/drone_1/cmd_motor_test` | `std_msgs/msg/Float32MultiArray` (data[0..3] FL,RL,RR,FR 0-100%) | 7 |
| `/drone_1/cmd_attitude` | `geometry_msgs/msg/Quaternion` | Fase 1 |

## Configurazione

WiFi e IP agent via `idf.py menuconfig` → "Drone — micro-ROS / WiFi":
- `CONFIG_DRONE_WIFI_SSID`, `CONFIG_DRONE_WIFI_PASSWORD`
- `CONFIG_DRONE_UROS_AGENT_IP`, `CONFIG_DRONE_UROS_AGENT_PORT`

Limiti micro-ROS in `app-colcon.meta` (root progetto):
- `RMW_UXRCE_MAX_PUBLISHERS=8`, `MAX_SUBSCRIPTIONS=4`
- `UCLIENT_UDP_TRANSPORT_MTU=2048`, `STREAM_HISTORY=8`

Modifiche a `app-colcon.meta` richiedono rebuild manuale di `libmicroros.a`:
```bash
rm -f managed_components/micro_ros_espidf_component/libmicroros.a
rm -rf managed_components/micro_ros_espidf_component/micro_ros_src/{build,install}
rm -rf build
idf.py build   # ~5 min
```

## Note di robustezza

- **Ping retry infinito** in `uros_init`: drone aspetta indefinitamente l'agent senza panic/reboot. Log ogni 10 tentativi.
- **WiFi WPA2/WPA3 transition**: `threshold.authmode=WIFI_AUTH_OPEN` + `pmf_cfg.capable=true` per supportare reti SAE.
- **`RCSOFT` macro** logga errori `rcl_publish` ma non aborta — la perdita di un singolo messaggio non interrompe il task.

## Dipendenze

`common`, `esp_wifi`, `esp_netif`, `esp_event`, `esp_timer`, `nvs_flash`, `micro_ros_espidf_component` (managed component).

Vedi `docs/specs/2026-05-07-piano-implementativo-microros-tethered.md` sezione 10bis per le criticità incontrate.
