# Piano Implementativo — micro-ROS Tethered + Refactor Task/Queue

**Data:** 2026-05-07
**Autore:** Angelo + Claude
**Stato:** In esecuzione (Step 1-4 completati)
**Branch:** `feature/microros-tethered`
**Scope:** Fase 0A → Fase 0B chiuse via micro-ROS WiFi UDP, con refactor del firmware al modello multi-task previsto dalla spec.
**Leggi prima:** `docs/specs/2026-04-26-stato-progetto-e-roadmap.md` (source of truth), `docs/02-FIRMWARE-ARCHITETTURA.md`, `docs/01-HARDWARE-BOM.md`, `CLAUDE.md`.

---

## 0. Contesto operativo

Hardware disponibile **adesso**:
- PCB custom v1.0 montato (con pull-down 10kΩ + serie 100Ω + switch arm separato)
- Nuovo XIAO ESP32-S3 saldato e funzionante
- Motori testati e funzionanti via menu USB attuale
- Tutti i sensori (IMU, optical flow, battery) già validati in Fase 0A

Modalità di sviluppo: **tethered**.
- Alimentazione drone: alimentatore da banco 4.0V / 3A via BT2.0
- USB-C XIAO ↔ PC sempre collegata (flash, log, debug)
- WiFi del drone connesso a una LAN condivisa con il PC (WSL2 host)
- Eliche **staccate** durante tutta l'implementazione di questo piano

Obiettivo del documento: portare il firmware da single-threaded con menu USB a multi-task con micro-ROS funzionante, in modo che da WSL si possa:
1. Vedere telemetria sensori in tempo reale via Foxglove Studio
2. Pilotare i 4 motori (singoli o gruppi) via topic ROS2 con watchdog di sicurezza

---

## 1. Stato attuale del codice (snapshot 2026-05-07)

```
microros-microdrone/
├── main/
│   └── main.c                  # SINGLE-THREADED, menu USB Serial
│                               # mode 1: sensor_log_mode() loop CSV stdout
│                               # mode 2: motor_test_mode() menu interattivo
├── components/
│   ├── common/                 # drone_config.h, drone_types.h ✅
│   ├── imu_driver/             # imu_init/calibrate/read ✅
│   ├── flow_driver/            # flow_init/read ✅
│   ├── motor_driver/           # motors_init/set/stop (LEDC PWM) ✅
│   ├── battery_monitor/        # presente, da verificare API ✅
│   └── uros_interface/         # STUB: README + .c vuoto ❌
└── sdkconfig.defaults
```

**Cosa funziona:** sensori, motori, calibrazione IMU, flow scale calibrato (`1.294e-2 rad/count`), gyro compensation embrionale nel main loop.

**Cosa manca:**
- Architettura multi-task FreeRTOS con queue (oggi è un loop unico in `sensor_log_mode`)
- Trasporto micro-ROS reale (WiFi + UDP + executor)
- Subscriber `cmd_motor_test` con watchdog
- Documentazione setup tethered (agent + Foxglove + WSL networking)

---

## 2. Scelte architetturali (consolidate, non rinegoziabili in questo piano)

| Decisione | Valore | Motivo |
|---|---|---|
| Trasporto micro-ROS | **WiFi UDP** (Micro XRCE-DDS) | Già previsto da spec 04-26; USB resta libera per log/menu. |
| Menu USB | **Mantenere come modalità separata** | Utile per debug locale rapido; non collide con WiFi. |
| Refactor task/queue | **Completo, come da spec 02** | Base solida per Fase 1 (PID attitudine). |
| `DRONE_ID` | **1 hard-coded** in `drone_config.h` | Multi-drone in futuro, non ora. |
| Comando motori | `std_msgs/Float32MultiArray` (4 valori 0-100%) | Pilotabile a mano da Foxglove "Publish" panel. |
| Watchdog cmd | Stop motori se nessun `cmd_motor_test` da >500ms | Safety in tethering. |

---

## 3. Architettura target firmware

### 3.1 Task e core pinning

```
Core 0 (comms / monitoring)              Core 1 (flight)
─────────────────────────────────        ──────────────────────────────
WiFi stack (idle)                        task_imu      1kHz  prio 5
task_microros   50Hz  prio 2             task_flow    ~20Hz  prio 4
task_battery    1Hz   prio 1             task_motors   1kHz  prio 6
```

Le task fase 1+ (`task_pid_attitude`, `task_fusion`, ecc.) **non** vengono create in questo piano.

### 3.2 Queue inter-task (FreeRTOS)

| Queue | Tipo elemento | Depth | Produttore → Consumatori |
|---|---|---|---|
| `imu_queue` | `imu_data_t` | 5 | `task_imu` → `task_microros` (in fase 1: anche pid_att) |
| `flow_queue` | `flow_data_t` | 3 | `task_flow` → `task_microros` |
| `battery_queue` | `float` (volt) | 2 | `task_battery` → `task_microros` |
| `cmd_queue` | `motor_cmd_t` | 1 | `task_microros` (sub callback) → `task_motors` |
| `motor_echo_queue` | `motor_cmd_t` | 1 | `task_motors` → `task_microros` (echo telemetria) |

`xQueueOverwrite` per `cmd_queue` e `motor_echo_queue` (depth=1, sempre l'ultimo valore).
`xQueueSend` con timeout=0 per `imu_queue` / `flow_queue` (drop se piena, non bloccante).

### 3.3 Topic micro-ROS (namespace `/drone_1/`)

**Publisher:**

| Topic | Tipo | Freq | QoS |
|---|---|---|---|
| `/drone_1/imu/raw` | `sensor_msgs/msg/Imu` | 100 Hz | BEST_EFFORT |
| `/drone_1/flow` | `geometry_msgs/msg/Vector3Stamped` | ~20 Hz | BEST_EFFORT |
| `/drone_1/range` | `sensor_msgs/msg/Range` | ~20 Hz | BEST_EFFORT |
| `/drone_1/battery` | `sensor_msgs/msg/BatteryState` | 1 Hz | BEST_EFFORT |
| `/drone_1/motors` | `std_msgs/msg/Float32MultiArray` | 50 Hz | BEST_EFFORT |

**Subscriber:**

| Topic | Tipo | QoS | Comportamento |
|---|---|---|---|
| `/drone_1/cmd_motor_test` | `std_msgs/msg/Float32MultiArray` (data[0..3] = duty FL,RL,RR,FR in 0-100) | RELIABLE | Callback valida lunghezza ==4, clampa 0-100, invia su `cmd_queue`, aggiorna timestamp watchdog. |

### 3.4 Mapping motori (vincolante)

```
data[0] = FL (motor index 0, GPIO_NUM_1, D0)
data[1] = RL (motor index 1, GPIO_NUM_2, D1)
data[2] = RR (motor index 2, GPIO_NUM_3, D2)
data[3] = FR (motor index 3, GPIO_NUM_4, D3)
```

Lo `motor_cmd_t` esistente in `drone_types.h` ha già `motor[4]` in quest'ordine.

---

## 4. Modifiche ai file (puntuale)

### 4.1 `components/common/include/drone_config.h`

Aggiungere:
```c
// micro-ROS WiFi
#define WIFI_SSID            "YOUR_SSID"          // override via menuconfig
#define WIFI_PASS            "YOUR_PASS"
#define UROS_AGENT_IP        "192.168.1.100"      // IP raggiungibile dal drone
#define UROS_AGENT_PORT      8888
#define UROS_NODE_NAME       "drone_node"
#define UROS_NAMESPACE       "drone_1"            // = "drone_<DRONE_ID>"

// Queue depths
#define QUEUE_DEPTH_IMU      5
#define QUEUE_DEPTH_FLOW     3
#define QUEUE_DEPTH_BATT     2
#define QUEUE_DEPTH_CMD      1
#define QUEUE_DEPTH_MOTOR    1

// Task stack sizes (parole, non byte: ESP-IDF default size_t)
#define STACK_IMU            4096
#define STACK_FLOW           4096
#define STACK_BATTERY        3072
#define STACK_MOTORS         3072
#define STACK_MICROROS       8192   // micro-ROS richiede stack ampio

// Task priorities (FreeRTOS: numero alto = priorità alta)
#define PRIO_BATTERY         1
#define PRIO_MICROROS        2
#define PRIO_FLOW            4
#define PRIO_IMU             5
#define PRIO_MOTORS          6

// Watchdog comando motori
#define MOTOR_CMD_TIMEOUT_MS 500
```

**Nota:** `WIFI_SSID/PASS/AGENT_IP` vanno preferibilmente esposti via `Kconfig.projbuild` (`menuconfig`) per non committarli. Per ora hard-code è accettabile, ma aggiungere `Kconfig.projbuild` in `main/` è poco lavoro e va fatto.

### 4.2 `main/idf_component.yml`

```yaml
dependencies:
  micro_ros_espidf_component:
    git: https://github.com/micro-ROS/micro_ros_espidf_component.git
    version: humble  # o jazzy, a seconda della versione ROS2 del PC
```

Verificare che la versione `humble`/`jazzy` corrisponda a quella di `ros-<distro>-foxglove-bridge` installato in WSL.

### 4.3 `main/CMakeLists.txt`

Aggiungere `uros_interface` alle `REQUIRES`:
```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES common imu_driver flow_driver motor_driver battery_monitor uros_interface
)
```

### 4.4 `sdkconfig.defaults`

Aggiungere/verificare:
```
# micro-ROS / FreeRTOS
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_UNICORE=n
CONFIG_PTHREAD_TASK_STACK_SIZE_DEFAULT=4096

# WiFi (in pinning Core 0 di default ESP-IDF)
CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0=y

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# Heap richiesto da micro-ROS
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
```

### 4.5 `components/uros_interface/include/uros_interface.h`

```c
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "drone_types.h"

typedef struct {
    QueueHandle_t imu_queue;
    QueueHandle_t flow_queue;
    QueueHandle_t battery_queue;
    QueueHandle_t cmd_queue;          // OUT: callback subscriber scrive qui
    QueueHandle_t motor_echo_queue;
} uros_queues_t;

esp_err_t uros_wifi_connect(void);    // bloccante, fino a IP ottenuto o timeout
esp_err_t uros_init(const uros_queues_t *queues);
void task_microros(void *arg);        // arg = uros_queues_t*
```

### 4.6 `components/uros_interface/uros_interface.c`

Sezioni richieste (in ordine):
1. **WiFi connect** (event-loop ESP-IDF, attesa `IP_EVENT_STA_GOT_IP`, timeout 30s, retry).
2. **RMW transport setup** (`rmw_uros_set_custom_transport` non serve, basta `rmw_uros_options_set_udp_address` con `UROS_AGENT_IP/PORT` e `RMW_UXRCE_TRANSPORT_UDP`).
3. **`uros_init`**: alloca `rcl_init_options_t`, `rclc_support_t`, `rcl_node_t`, publisher/subscriber, `rclc_executor_t`. Topic con namespace prefix `drone_1/`.
4. **Subscriber callback** `cmd_motor_test_cb(const void *msgin)`:
   - Valida `Float32MultiArray.data.size == 4`
   - Clamp 0-100 ogni valore
   - Compone `motor_cmd_t`, `xQueueOverwrite(cmd_queue, &cmd)`
   - Aggiorna `last_cmd_us = esp_timer_get_time()` (statico nel file)
5. **`task_microros`** (Core 0, prio 2, 50Hz):
   - Drena `imu_queue`, decimando a 100Hz (1 ogni 10) → publish `/imu/raw`
   - Drena `flow_queue` → publish `/flow` + `/range`
   - Drena `battery_queue` (1Hz già) → publish `/battery`
   - Drena `motor_echo_queue` → publish `/motors`
   - `rclc_executor_spin_some(&executor, RCL_MS_TO_NS(5))`
   - Vincolo: tutto non bloccante, `vTaskDelayUntil` a 20ms.
6. **Header frame_id:** `imu_link`, `flow_link`, `range_link` per ogni messaggio con header.

### 4.7 `main/main.c` — refactor completo

Pseudocodice:
```c
void app_main(void) {
    init_led();
    motors_init(); motors_stop();

    // Init sensori sempre (servono in ogni mode)
    imu_init(); imu_calibrate(500);
    flow_init();
    battery_init();

    // Menu USB con timeout 3s, default uros_mode
    int mode = read_mode_with_timeout(3000, /*default*/ 1);

    switch (mode) {
        case 2: sensor_log_mode(); break;       // legacy CSV
        case 3: motor_test_mode(); break;       // legacy menu motori
        case 1:
        default:
            uros_mode();                         // nuovo
            break;
    }
}

static void uros_mode(void) {
    // Crea queue
    static uros_queues_t Q;
    Q.imu_queue       = xQueueCreate(QUEUE_DEPTH_IMU, sizeof(imu_data_t));
    Q.flow_queue      = xQueueCreate(QUEUE_DEPTH_FLOW, sizeof(flow_data_t));
    Q.battery_queue   = xQueueCreate(QUEUE_DEPTH_BATT, sizeof(float));
    Q.cmd_queue       = xQueueCreate(QUEUE_DEPTH_CMD, sizeof(motor_cmd_t));
    Q.motor_echo_queue= xQueueCreate(QUEUE_DEPTH_MOTOR, sizeof(motor_cmd_t));

    // WiFi prima di lanciare task (l'agent può non esserci ancora, ok)
    ESP_ERROR_CHECK(uros_wifi_connect());
    ESP_ERROR_CHECK(uros_init(&Q));

    // Task Core 1 (flight)
    xTaskCreatePinnedToCore(task_imu,     "imu",     STACK_IMU,     &Q, PRIO_IMU,     NULL, 1);
    xTaskCreatePinnedToCore(task_flow,    "flow",    STACK_FLOW,    &Q, PRIO_FLOW,    NULL, 1);
    xTaskCreatePinnedToCore(task_motors,  "motors",  STACK_MOTORS,  &Q, PRIO_MOTORS,  NULL, 1);

    // Task Core 0 (comms)
    xTaskCreatePinnedToCore(task_battery,  "batt",    STACK_BATTERY, &Q, PRIO_BATTERY, NULL, 0);
    xTaskCreatePinnedToCore(task_microros, "uros",    STACK_MICROROS,&Q, PRIO_MICROROS,NULL, 0);

    // app_main termina; FreeRTOS scheduler tiene tutto vivo
}
```

### 4.8 Task implementation

**`task_imu`** (in `imu_driver/` o file nuovo `tasks/task_imu.c`):
```c
void task_imu(void *arg) {
    uros_queues_t *Q = arg;
    imu_data_t imu;
    TickType_t last = xTaskGetTickCount();
    while (1) {
        if (imu_read(&imu) == ESP_OK) {
            xQueueSend(Q->imu_queue, &imu, 0);  // drop se piena
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1));  // 1kHz
    }
}
```

**`task_flow`**: stessa forma, `vTaskDelayUntil` 50ms.

**`task_battery`**: legge ADC, push float volt nella queue, 1Hz.

**`task_motors`** (CRITICO — gestisce watchdog):
```c
void task_motors(void *arg) {
    uros_queues_t *Q = arg;
    motor_cmd_t cmd = {0};
    int64_t last_cmd_us = 0;
    TickType_t last = xTaskGetTickCount();
    while (1) {
        motor_cmd_t new_cmd;
        if (xQueueReceive(Q->cmd_queue, &new_cmd, 0) == pdTRUE) {
            cmd = new_cmd;
            last_cmd_us = esp_timer_get_time();
        }

        // Watchdog: se nessun cmd da >500ms, stop
        int64_t age_us = esp_timer_get_time() - last_cmd_us;
        if (last_cmd_us == 0 || age_us > MOTOR_CMD_TIMEOUT_MS * 1000) {
            memset(cmd.motor, 0, sizeof(cmd.motor));
        }

        motors_set(&cmd);
        xQueueOverwrite(Q->motor_echo_queue, &cmd);

        vTaskDelayUntil(&last, pdMS_TO_TICKS(1));  // 1kHz
    }
}
```

Posizionare i task in `main/main.c` o in nuovi file `main/tasks/*.c`. Preferibile **nuovi file** (`main/task_imu.c`, ecc.) per leggibilità — aggiornare `main/CMakeLists.txt` con `SRCS "main.c" "task_imu.c" ...`.

---

## 5. Lato WSL — setup agent + Foxglove

### 5.1 Verificare WSL networking

In Windows PowerShell:
```powershell
wsl --version
```
Se Windows 11 + WSL >= 2.0.0 e file `%UserProfile%\.wslconfig` contiene:
```
[wsl2]
networkingMode=mirrored
```
allora WSL espone direttamente le porte sulla LAN — l'agent in WSL è raggiungibile dal drone senza altro.

**Se NON in mirrored mode (caso più probabile):** serve port-forward. In PowerShell **Admin**:
```powershell
# IP corrente di WSL
wsl hostname -I
# Aggiungi forwarding
netsh interface portproxy add v4tov4 listenport=8888 listenaddress=0.0.0.0 connectport=8888 connectaddress=<WSL_IP>
# Firewall
New-NetFirewallRule -DisplayName "uROS Agent UDP 8888" -Direction Inbound -Protocol UDP -LocalPort 8888 -Action Allow
```
Nota: `portproxy` in Windows fa solo TCP. **Per UDP non funziona.** Soluzioni:
- (consigliata) Usa **mirrored networking**
- oppure installa `micro_ros_agent` nativo su Windows (da sorgente, complicato)
- oppure usa un container Docker Desktop con `--network=host` su Windows
- oppure usa una macchina Linux fisica/VM separata

`UROS_AGENT_IP` in `drone_config.h` deve essere l'IP che il drone vede e che porta all'agent (LAN IP del PC se mirrored, o IP del PC con port-forward UDP funzionante).

### 5.2 Avvio agent

In WSL (assumendo ROS2 Humble installato):
```bash
# install
sudo apt install ros-humble-micro-ros-agent ros-humble-foxglove-bridge

# avvio agent
source /opt/ros/humble/setup.bash
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888 -v6
```

In un secondo terminale:
```bash
source /opt/ros/humble/setup.bash
ros2 run foxglove_bridge foxglove_bridge --ros-args -p port:=8765
```

Foxglove Studio (Windows): `Open Connection → Foxglove WebSocket → ws://localhost:8765`. I topic `/drone_1/*` appaiono dopo che il drone si è registrato all'agent.

### 5.3 Pannello "Publish" per cmd_motor_test in Foxglove

Pannello **Publish** → topic `/drone_1/cmd_motor_test` → tipo `std_msgs/Float32MultiArray` → JSON:
```json
{ "data": [10.0, 10.0, 10.0, 10.0] }
```
Pulsante "Publish" → tutti i motori al 10%. Watchdog spegne i motori se smetti di pubblicare per >500ms (richiede pubblicazione ripetuta o panel "Publish Repeated" a 5Hz).

---

## 6. Step di esecuzione (incrementali, con verifica)

Ogni step **deve** finire con `idf.py build` pulito + verifica funzionale prima di passare al successivo.

### Step 1 — Setup managed component micro-ROS ✅ COMPLETATO

**Note esecuzione:** servono dipendenze Python nel venv ESP-IDF prima del primo build:
```bash
pip install catkin_pkg lark-parser empy==3.3.4 colcon-common-extensions
```
(senza queste, il build di micro-ROS fallisce con `ModuleNotFoundError: catkin_pkg`).
- Creare `main/idf_component.yml` con dipendenza `micro_ros_espidf_component`
- Aggiornare `sdkconfig.defaults` (sezione 4.4)
- `idf.py reconfigure && idf.py build`
- **Verifica:** build completa senza errori. `managed_components/` contiene `micro_ros_espidf_component` e dipendenze.

### Step 2 — drone_config.h costanti ✅ COMPLETATO

**Note esecuzione:** WiFi/agent esposti via `main/Kconfig.projbuild` (non hard-coded). `sdkconfig` rimosso dal versionamento (`.gitignore`). Costanti aggiunte: `STACK_*`, `QUEUE_DEPTH_BATT`, `WIFI_*`/`UROS_*` (lette da `CONFIG_*`), `MOTOR_CMD_TIMEOUT_MS`. I PRIO_* esistenti (`PRIO_TASK_*`) sono stati riusati invece di creare alias.
- Aggiungere costanti sezione 4.1
- Niente codice nuovo, solo header
- **Verifica:** build pulita.

### Step 3 — Skeleton task senza micro-ROS ✅ COMPLETATO

**Note esecuzione:** task_microros placeholder a 50Hz drena al massimo `QUEUE_DEPTH_IMU=5` per ciclo → ~250 IMU/s su 1000 prodotti. **Da affrontare in Step 5**: alzare `task_microros` a 100Hz e/o aumentare `QUEUE_DEPTH_IMU` per evitare drop massicci sull'`/imu/raw` decimato.
- Creare `main/task_imu.c`, `task_flow.c`, `task_battery.c`, `task_motors.c`
- `task_microros` per ora un placeholder che fa solo `xQueueReceive` da tutte le queue e logga ogni 100 messaggi
- `main.c` refactor con menu+timeout, lancio task
- Modalità `[1]` lancia i task; `[2]` e `[3]` sono il vecchio `sensor_log_mode` e `motor_test_mode`, intatti
- **Verifica:** modalità 1 → log "imu_queue rcv N=100, flow_queue rcv N=20, batt_queue rcv N=1" senza crash, motori fermi (watchdog attivo, nessun cmd → stop).

### Step 4 — WiFi connect in `uros_interface` ✅ COMPLETATO

**Note esecuzione:** IP assegnato dal router al drone in test: `192.168.1.15`. Mirrored networking WSL attivo, ping drone↔WSL OK. Rete è WPA3-SAE: configurato `WIFI_AUTH_WPA2_PSK` come threshold minimo (compatibile sia WPA2 che WPA3).
- Implementare `uros_wifi_connect()`
- Chiamarlo da `uros_mode()` prima di lanciare i task
- `task_microros` ancora placeholder
- **Verifica:** boot mostra log `WiFi connected, IP=192.168.x.y`. Drone pingabile da WSL.

### Step 5 — micro-ROS init + 1 publisher (`/imu/raw`)
- Implementare `uros_init()` minimale: support, node, 1 publisher, executor
- `task_microros` pubblica `/drone_1/imu/raw` a 100Hz
- **Verifica:** in WSL `ros2 topic list` mostra `/drone_1/imu/raw`. `ros2 topic hz /drone_1/imu/raw` → ~100Hz. Foxglove plotta accel/gyro.

### Step 6 — Restanti publisher
- Aggiungere `/flow`, `/range`, `/battery`, `/motors`
- **Verifica:** tutti i topic visibili con frequenza corretta. Foxglove dashboard con plot multipli.

### Step 7 — Subscriber `cmd_motor_test` + watchdog
- Aggiungere subscriber RELIABLE
- Callback popola `cmd_queue` con `xQueueOverwrite`
- `task_motors` watchdog 500ms (già scritto in step 3, ora riceve cmd reali)
- **Verifica con eliche STACCATE:**
  1. Pubblica `{data:[5,5,5,5]}` da Foxglove → motori girano a duty basso
  2. Smetti di pubblicare → entro 500ms motori si fermano (osservabile, sentibile)
  3. Pubblica solo `data[0]=15` → solo FL gira
  4. Pubblica `data` di lunghezza ≠ 4 → callback rifiuta, log warning, nessun cambio

### Step 8 — Documentazione
- `docs/timeline.md`: aggiungere riga 2026-05-07
- Nuovo `docs/07-MICROROS-TETHERED.md`: guida operativa breve (avvio agent, Foxglove, troubleshooting WSL)
- Aggiornare README di `uros_interface/`, `motor_driver/`, eventuali `main/`
- Aggiornare `docs/02-FIRMWARE-ARCHITETTURA.md` (Fase 0B → ✅)
- Aggiornare `docs/specs/2026-04-26-stato-progetto-e-roadmap.md` (snapshot Fase 0B chiusa)
- **Verifica:** `git diff --stat` coerente; CLAUDE.md non richiede modifiche (la struttura task è già descritta).

---

## 7. Test plan finale

A piano completo, sequenza di test da eseguire (eliche staccate per i punti 4-7):

1. Boot con menu → timeout 3s → entra in uROS mode
2. Log mostra "WiFi OK", "uROS agent connected"
3. `ros2 topic list` da WSL: tutti i 5 topic publish + 1 subscriber visibili
4. `ros2 topic hz` su ogni topic: frequenze corrispondono alla tabella 3.3 (±10%)
5. Foxglove plotta IMU/flow in real-time, batteria coerente con tensione alimentatore
6. Publish `cmd_motor_test` con vari valori 0-30% → motori rispondono in <50ms
7. Watchdog: stop pubblicazione → motori fermi entro 500ms
8. Boot in modalità 2 (sensor log) e 3 (motor test) → entrambe ancora funzionano (regressione)
9. Reboot a freddo: nessun motore parte spontaneamente durante il boot (verifica pull-down + watchdog)

Criterio di chiusura: tutti i punti passano. A quel punto **Fase 0B è chiusa via micro-ROS** e il progetto è pronto per Fase 1 (PID attitudine).

---

## 8. Vincoli da rispettare

- **Lingua interazione**: italiano
- **Linguaggio firmware**: C puro (vincolo ESP-IDF)
- **Non modificare** `old/`
- Aggiornare README di **ogni** componente toccato
- A fine lavoro: aggiornare `docs/timeline.md`
- Ogni risposta finale deve avere **riassunto modifiche + spiegazione tecnica**
- Eliche **staccate** durante tutto il flusso di sviluppo
- Switch arm motori del PCB: **OFF** durante flash, **ON** solo quando si testa il watchdog

---

## 9. Rischi e mitigazioni

| Rischio | Probabilità | Mitigazione |
|---|---|---|
| WSL2 non in mirrored mode → UDP 8888 irraggiungibile | Alta | Documentato in §5.1; fallback: agent su VM Linux o Docker Desktop con host network |
| Stack overflow `task_microros` sotto carico | Media | Stack iniziale 8192, monitorare con `uxTaskGetStackHighWaterMark`; aumentare se necessario |
| Drift orologi → timestamp ROS2 inconsistenti | Bassa | Usare `rmw_uros_sync_session(1000)` dopo init per sync con agent |
| Subscriber RELIABLE blocca executor se agent down | Media | Verificare comportamento; se patologico passare a BEST_EFFORT con publish ripetuto da Foxglove |
| Eliche montate per errore durante test motori | Critica | Procedura: eliche fisicamente in un cassetto separato fino a fine Fase 1 |

---

## 10. File touched (atteso)

```
modified:   main/main.c                                  # refactor mode select + uros_mode
new:        main/task_imu.c
new:        main/task_flow.c
new:        main/task_battery.c
new:        main/task_motors.c
modified:   main/CMakeLists.txt                          # SRCS e REQUIRES
new:        main/idf_component.yml                       # micro_ros_espidf_component dep
modified:   sdkconfig.defaults
modified:   components/common/include/drone_config.h     # costanti WiFi/uros/queue/stack
new:        components/uros_interface/include/uros_interface.h
modified:   components/uros_interface/uros_interface.c   # implementazione vera
modified:   components/uros_interface/README.md
modified:   components/motor_driver/README.md            # menzione watchdog
modified:   docs/02-FIRMWARE-ARCHITETTURA.md             # Fase 0B → ✅
modified:   docs/specs/2026-04-26-stato-progetto-e-roadmap.md
modified:   docs/timeline.md
new:        docs/07-MICROROS-TETHERED.md
```

---

## 11. Avvio della prossima sessione

Apri una nuova chat di Claude Code in questa repo. Bastano le seguenti istruzioni:

> Leggi `docs/specs/2026-05-07-piano-implementativo-microros-tethered.md` ed eseguilo step-by-step. Ferma prima di ogni step di verifica e mostrami i comandi di test prima di procedere allo step successivo.

Il piano è pensato per essere autosufficiente: include contesto, scelte fatte, struttura attuale, struttura target, modifiche puntuali ai file, ordine di esecuzione, test plan, rischi.
