# uros_interface

Bridge tra micro-ROS (ROS2) e le FreeRTOS queues interne del drone.

## Stato: Step 7+ completato (8 publisher + 2 subscriber + executor 2 handle). Watchdog motori 500ms + arm gate in `task_motors`. Temperatura on-die ESP32-S3 via `esp_driver_tsens`. Console di debug `/drone_1/log` (rcl_interfaces/Log) per Foxglove Log panel.

## API

- `uros_wifi_connect()` — Connessione WiFi station bloccante (timeout 30s, retry 10).
- `uros_init(queues)` — Init micro-ROS: ping retry agent → `rclc_support` → `node` → 5 publisher → init messaggi. Non blocca su agent assente: aspetta indefinitamente.
- `task_microros(arg)` — Task FreeRTOS @ 100Hz, drena le queue produttori e pubblica.
- `uros_log(level, fmt, ...)` — Logga su `/drone_1/log`. Livelli `UROS_LOG_DEBUG/INFO/WARN/ERROR/FATAL` (= valori ROS2 standard 10/20/30/40/50). Non-blocking, drop on full, no-op se uros non ancora inizializzato. **Non chiamare da ISR.**

## Topic pubblicati (Step 6)

| Topic | Tipo messaggio | Freq | QoS |
|-------|---------------|------|-----|
| `/drone_1/imu/raw` | `sensor_msgs/msg/Imu` | 100 Hz | BEST_EFFORT |
| `/drone_1/flow` | `geometry_msgs/msg/Vector3Stamped` | ~20 Hz | BEST_EFFORT |
| `/drone_1/range` | `sensor_msgs/msg/Range` | ~20 Hz | BEST_EFFORT |
| `/drone_1/battery` | `sensor_msgs/msg/BatteryState` | 1 Hz | BEST_EFFORT |
| `/drone_1/motors` | `std_msgs/msg/Float32MultiArray` | ~100 Hz (echo) | BEST_EFFORT |
| `/drone_1/armed` | `std_msgs/msg/Bool` | on-change + 1 al boot | RELIABLE |
| `/drone_1/temp` | `sensor_msgs/msg/Temperature` | 1 Hz | BEST_EFFORT |
| `/drone_1/log` | `rcl_interfaces/msg/Log` | event-driven, max ~500/s | BEST_EFFORT |

Frame ID: `imu_link`, `flow_link`, `range_link`, `battery_link`. Timestamp via `esp_timer_get_time()` (microsecondi monotonic, NON sincronizzato con clock agent).

## Topic sottoscritti

| Topic | Tipo messaggio | QoS | Stato |
|-------|---------------|-----|-------|
| `/drone_1/cmd_motor_test` | `std_msgs/msg/Float32MultiArray` (data[0..3] FL,RL,RR,FR 0-100%) | RELIABLE | Step 7 ✅ |
| `/drone_1/arm` | `std_msgs/msg/Bool` (sticky software arm) | RELIABLE | ✅ implementato |
| `/drone_1/cmd_attitude` | `geometry_msgs/msg/Quaternion` | — | Fase 1 |

Callback `cmd_motor_test`: scarta messaggi con `data.size != 4` (log warning), clampa 0-100, scrive su `cmd_queue` con `xQueueOverwrite`. `task_motors` legge la queue e aggiorna il timestamp watchdog: se nessun cmd entro `MOTOR_CMD_TIMEOUT_MS` (500ms) → motori a 0.

Callback `arm_cb`: aggiorna l'`atomic_bool g_armed` (definito in `main.c`, dichiarato in `drone_types.h`). On-change setta `s_armed_pub_pending` consumato dal main loop di `task_microros` per pubblicare `/armed`. Boot: DISARMED. `task_motors` controlla `g_armed` ogni ciclo (1kHz) e forza i motori a 0 quando disarmato. Sulla transizione disarm→arm il task resetta il watchdog cmd: serve un nuovo `cmd_motor_test` per far girare i motori (anti-replay di cmd stantii pre-disarm).

## Configurazione

WiFi e IP agent via `idf.py menuconfig` → "Drone — micro-ROS / WiFi":
- `CONFIG_DRONE_WIFI_SSID`, `CONFIG_DRONE_WIFI_PASSWORD`
- `CONFIG_DRONE_UROS_AGENT_IP`, `CONFIG_DRONE_UROS_AGENT_PORT`

Limiti micro-ROS in `app-colcon.meta` (root progetto):
- `RMW_UXRCE_MAX_PUBLISHERS=12`, `MAX_SUBSCRIPTIONS=4`
- `UCLIENT_UDP_TRANSPORT_MTU=2048`, `STREAM_HISTORY=8`

Modifiche a `app-colcon.meta` richiedono rebuild manuale di `libmicroros.a`:
```bash
rm -f managed_components/micro_ros_espidf_component/libmicroros.a
rm -rf managed_components/micro_ros_espidf_component/micro_ros_src/{build,install}
rm -rf build
idf.py build   # ~5 min
```

## Note di robustezza

- **WiFi retry infinito**: nessun abort/reboot loop su fallimento connessione. Reset contatore su `WIFI_EVENT_STA_CONNECTED` (associato) oltre che su `IP_EVENT_STA_GOT_IP` — evita di consumare retry quando l'AP kicka tra associazione e DHCP. Log ogni 10 tentativi dopo i primi 10.
- **Ping retry infinito** in `uros_init`: drone aspetta indefinitamente l'agent senza panic/reboot. Log ogni 10 tentativi.
- **WiFi WPA2/WPA3 transition**: `threshold.authmode=WIFI_AUTH_OPEN` + `pmf_cfg.capable=true` per supportare reti SAE.
- **`RCSOFT` macro** logga errori `rcl_publish` ma non aborta — la perdita di un singolo messaggio non interrompe il task.

## Console di debug `/drone_1/log`

Foxglove → Add panel → **Log**: filtra per livello (DEBUG/INFO/WARN/ERROR/FATAL), per `name`, scrubba sulla timeline. Niente filtro = mostra tutto.

Eventi loggati attualmente:
- **Boot**: `boot reset_reason=...` con livello WARN/ERROR se anomalo (BROWNOUT/PANIC/WDT). Diagnostico chiave per cali di tensione dovuti al supply motori.
- **WiFi**: `connected IP=...`, `disconnect retry=N reason=M`, `associato attesa DHCP`.
- **Arm**: `ARM DISARMED -> ARMED` (e viceversa) con livello WARN.
- **Watchdog motori**: edge `cmd timeout >500ms motori azzerati` (WARN) e `cmd flow ripreso` (INFO). Solo in stato armed.
- **cmd_motor_test malformato**: `size=N (atteso 4) scartato` (WARN).

Aggiungere log da altri task: `#include "uros_interface.h"` poi `uros_log(UROS_LOG_INFO, "fmt", arg);`. La queue interna è 16 slot: in burst i log oltre soglia vengono droppati (drop silenzioso, mai blocca il chiamante).

## Dipendenze

`common`, `esp_wifi`, `esp_netif`, `esp_event`, `esp_timer`, `nvs_flash`, `micro_ros_espidf_component` (managed component).

Vedi `docs/specs/2026-05-07-piano-implementativo-microros-tethered.md` sezione 10bis per le criticità incontrate.
