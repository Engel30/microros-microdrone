# Design Spec: ESP32-S3 Swarm Drone — Architettura micro-ROS

**Data:** 2026-03-10
**Autore:** Angelo + Claude
**Stato:** Approvato

---

## 1. Obiettivo

Ristrutturare il firmware del micro-drone da un prototipo Arduino monolitico a un'architettura modulare basata su **ESP-IDF + FreeRTOS + micro-ROS**. Il drone deve:

- Pubblicare telemetria come nodo ROS2 (via micro-ROS) visibile su Foxglove Studio
- Ricevere comandi di attitudine e posizione dal PC centrale
- Essere scalabile a uno sciame: stesso firmware, namespace diverso (`/drone_N/`)
- Svilupparsi incrementalmente per fasi, validando ogni layer prima di costruirci sopra

---

## 2. Decisioni architetturali

| Decisione | Scelta | Motivazione |
|---|---|---|
| Framework | ESP-IDF (non Arduino) | Controllo totale su FreeRTOS, I2C, UART. Formativo. |
| RTOS | FreeRTOS (incluso in ESP-IDF) | Dual core, task con priorita, code inter-task |
| Comunicazione ROS2 | micro-ROS (Micro XRCE-DDS) | Nodo ROS2 nativo sull'ESP32, agent sul PC |
| Trasporto | UDP su WiFi (LAN comune) | Tutti i dispositivi sulla stessa rete |
| Visualizzazione | Foxglove Studio | Connessione diretta ai topic ROS2 |
| Linguaggio firmware | C puro | Richiesto da micro-ROS e ESP-IDF |
| Scalabilita sciame | Namespace ROS2 `/drone_N/` | Stesso firmware, ID configurabile |

---

## 3. Hardware

### Microcontrollore

| Parametro | Valore |
|---|---|
| Board | Seeed XIAO ESP32-S3 |
| MCU | ESP32-S3 Dual Core (Xtensa LX7) |
| Clock | 240 MHz |
| Flash | 8 MB |
| PSRAM | 8 MB (opzionale) |
| SRAM | 512 KB |

### Pinout definitivo

| Pin | Funzione | Bus/Tipo | Componente |
|---|---|---|---|
| D0 | Motore 1 (Front-Left) | PWM | MOSFET SI2302 + 1N5819 |
| D1 | Motore 2 (Rear-Left) | PWM | idem |
| D2 | Motore 3 (Rear-Right) | PWM | idem |
| D3 | Motore 4 (Front-Right) | PWM | idem |
| D4 | SDA | I2C (400kHz) | MPU6050 (3.3V, ~5mA) |
| D5 | SCL | I2C (400kHz) | MPU6050 |
| D6 | TX1 | UART (19200) | Optical Flow + ToF (protocollo CXOF) |
| D7 | RX1 | UART (19200) | idem |
| D8 | V-Sense | ADC | Partitore 100k/100k (meta V_bat) |
| D9 | Buzzer | PWM | Allarmi batteria |
| D10 | LED status | GPIO | Debug visivo |

### Sensori

| Sensore | Modello | Interfaccia | Frequenza |
|---|---|---|---|
| IMU 6-DoF | MPU6050 (GY-521) | I2C 400kHz | 1kHz (DLPF abilitato, SMPLRT_DIV=0) |
| Optical Flow | PMW3901 clone Thinary | UART 19200 (CXOF) | ~20Hz (vincolo hardware) |
| ToF altimetro | VL53L1X (integrato nel modulo flow) | UART (stesso frame CXOF) | ~20Hz |

### Propulsione

| Componente | Specifiche |
|---|---|
| Motori | 4x 8520 Coreless Brushed (3.7V, 1S) |
| Driver | MOSFET SI2302 (SOT-23), low-side switch |
| Protezione | Diodo flyback 1N5819 per motore |
| Controllo | PWM diretto dall'ESP32 (LEDC o MCPWM) |
| Eliche | 40mm 4-pale, foro 1.0mm |
| Frame | 75mm Whoop (BetaFPV 75 Pro o simili) |
| Batteria | LiPo 1S 450-550mAh (BT2.0) |
| Peso target | < 65g (TWR > 2.5:1) |

---

## 4. Architettura software

### 4.1 Assegnazione Core

```
Core 0 — Comunicazione e monitoring
  WiFi stack (ESP-IDF lo piazza qui di default)
  task_microros: bridge micro-ROS <-> FreeRTOS queue
  task_battery: lettura ADC, buzzer allarme

Core 1 — Flight-critical (real-time)
  task_imu: lettura MPU6050 a 1kHz
  task_flow: lettura CXOF a ~20Hz
  task_pid_attitude: PID roll/pitch/yaw a 1kHz (gyro raw diretto)
  task_pid_velocity: PID velocita a 50Hz (Fase 2)
  task_pid_position: PID posizione a 20Hz (Fase 3)
  task_fusion: fusione sensori (Fase 2+)
  task_motors: output PWM a 1kHz
```

Motivazione: il WiFi stack ESP-IDF e pinned a Core 0 di default (`CONFIG_ESP_WIFI_TASK_CORE_ID=0`). Mettere il flight control su Core 1 garantisce che non competa con i task di rete per il tempo CPU.

### 4.2 Task FreeRTOS — dettaglio

| Task | Core | Frequenza | Priorita | Fase | Funzione |
|---|---|---|---|---|---|
| task_imu | 1 | 1kHz | 5 | 0A | Legge MPU6050 via I2C, scrive su imu_queue |
| task_flow | 1 | ~20Hz | 4 | 0A | Parsa frame CXOF via UART, scrive su flow_queue |
| task_microros | 0 | 50Hz | 2 | 0A | Pubblica topic ROS2, riceve comandi, spin executor |
| task_battery | 0 | 1Hz | 1 | 0A | Legge ADC D8, attiva buzzer se V < soglia |
| task_motors | 1 | 1kHz | 6 | 0B | Scrive PWM su D0-D3 da motor_queue |
| task_pid_attitude | 1 | 1kHz | 6 | 1 | Gyro raw -> PID roll/pitch/yaw -> motor_queue |
| task_pid_velocity | 1 | 50Hz | 4 | 2 | Stima velocita -> setpoint attitudine |
| task_pid_position | 1 | 20Hz | 3 | 3 | Posizione fusa -> setpoint velocita |
| task_fusion | 1 | 100Hz | 4 | 2+ | Fonde IMU + flow in stato (posizione, velocita) |

### 4.3 FreeRTOS Queue (comunicazione inter-task)

| Queue | Produttore | Consumatori | Profondita | Note |
|---|---|---|---|---|
| imu_queue | task_imu | task_pid_attitude, task_fusion, task_microros | 5 | Dato piu recente = piu importante |
| flow_queue | task_flow | task_fusion, task_microros | 3 | |
| state_queue | task_fusion | task_pid_velocity, task_pid_position, task_microros | 3 | Stato fuso (Fase 2+) |
| cmd_queue | task_microros | task_pid_attitude / velocity / position | 1 | Solo ultimo comando |
| motor_queue | task_pid_attitude | task_motors, task_microros | 1 | Solo ultimo output |

### 4.4 Flusso dati

```
Core 1 (flight-critical)                     Core 0 (comunicazione)

  +----------+ 1kHz
  | task_imu |--+-> imu_queue ----------->+                          +
  +----------+  |                         |    task_microros (50Hz)   |
                |                         |                          |
  +----------+ ~20Hz                      |  PUB:                    |
  | task_flow|--+-> flow_queue ---------->|   /drone_N/imu/raw       |
  +----------+  |                         |   /drone_N/flow          |
                |                         |   /drone_N/range         |--> WiFi
                v                         |   /drone_N/attitude      |--> Agent
        task_pid_attitude (1kHz)          |   /drone_N/odom          |--> PC
                |                         |   /drone_N/motors        |
                v                         |   /drone_N/battery       |
        task_motors (1kHz)                |                          |
                                          |  SUB:                    |
        +---------------+                |   /drone_N/cmd_attitude  |<-- PC
        | task_fusion   |                |   /drone_N/cmd_position  |
        | (Fase 2+)     |                |   /drone_N/cmd_motor_test|
        +---------------+                |                          |
                                          |  task_battery (1Hz)      |
                                          |  ADC D8, buzzer D9       |
                                          +--------------------------+
```

Il PID di attitudine legge direttamente dalla imu_queue (gyro raw a 1kHz)
per massima reattivita. La fusion alimenta solo i loop esterni (Fase 2+).

### 4.5 Controllo a cascata

```
Fase 3          Fase 2               Fase 1
10-20Hz         20-50Hz              1kHz
cmd_pos --> PID posizione --> PID velocita --> PID attitudine --> motori
            "vai a x=1m"    "muoviti 0.5m/s"  "inclina 5 deg"   duty%
```

Ogni livello produce il setpoint per quello sotto.
Ogni fase aggiunge un task, il codice delle fasi precedenti non cambia.

---

## 5. micro-ROS — topic ROS2

### Topic pubblicati dal drone

| Topic | Tipo messaggio | Frequenza | Fase |
|---|---|---|---|
| `/drone_N/imu/raw` | `sensor_msgs/msg/Imu` | 100Hz | 0A |
| `/drone_N/flow` | `std_msgs/msg/Int16MultiArray` | 20Hz | 0A |
| `/drone_N/range` | `sensor_msgs/msg/Range` | 20Hz | 0A |
| `/drone_N/battery` | `std_msgs/msg/Float32` | 1Hz | 0A |
| `/drone_N/motors` | `std_msgs/msg/Float32MultiArray` | 50Hz | 0B |
| `/drone_N/attitude` | `sensor_msgs/msg/Imu` | 100Hz | 2 |
| `/drone_N/odom` | `nav_msgs/msg/Odometry` | 20Hz | 2 |

### Topic sottoscritti dal drone (comandi)

| Topic | Tipo messaggio | Fase | Uso |
|---|---|---|---|
| `/drone_N/cmd_motor_test` | `std_msgs/msg/Float32MultiArray` | 0B | Test singolo motore [m1,m2,m3,m4] duty% |
| `/drone_N/cmd_attitude` | `geometry_msgs/msg/Quaternion` | 1 | Setpoint roll/pitch/yaw |
| `/drone_N/cmd_position` | `geometry_msgs/msg/PoseStamped` | 3 | Setpoint posizione x,y,z |

### Configurazione micro-ROS

- Trasporto: UDP su WiFi (LAN)
- Agent: `micro-ROS agent` sul PC (Docker o nativo)
- Namespace: secondo parametro di `rclc_node_init` — configurabile in `drone_config.h`
- QoS: BEST_EFFORT per telemetria, RELIABLE per comandi

---

## 6. Configurazione MPU6050

| Registro | Valore | Effetto |
|---|---|---|
| SMPLRT_DIV (0x19) | 0x00 | Sample rate = 1kHz |
| CONFIG (0x1A) | 0x03 | DLPF ~42Hz (taglia vibrazioni motori) |
| GYRO_CONFIG (0x1B) | 0x08 | +-500 deg/s (range adeguato per drone) |
| ACCEL_CONFIG (0x1C) | 0x08 | +-4g (range adeguato per manovre) |
| PWR_MGMT_1 (0x6B) | 0x01 | Clock da gyro X (piu stabile) |

I2C configurato a 400kHz (fast mode) per raggiungere 1kHz di lettura.
Calibrazione: lettura di N campioni a drone fermo, calcolo offset medio.
Yaw: integrazione manuale del gyroZ con deadzone 0.2 deg/s (no magnetometro).

---

## 7. Struttura file del progetto

```
microros-microdrone/
|-- CMakeLists.txt                          # Progetto ESP-IDF top-level
|-- sdkconfig.defaults                      # WiFi, I2C speed, FreeRTOS tick rate
|
|-- main/
|   |-- CMakeLists.txt
|   |-- main.c                              # app_main(): crea queue e lancia task
|   +-- Kconfig.projbuild                   # Menu config (drone ID, WiFi SSID/pass)
|
|-- components/
|   |-- common/                             # Tipi e config condivisi
|   |   |-- CMakeLists.txt
|   |   +-- include/
|   |       |-- drone_types.h               # imu_data_t, flow_data_t, state_t, motor_cmd_t
|   |       +-- drone_config.h              # Pin, frequenze, costanti fisiche, DRONE_ID
|   |
|   |-- imu_driver/                         # MPU6050 via ESP-IDF I2C
|   |   |-- CMakeLists.txt
|   |   |-- include/imu_driver.h
|   |   +-- imu_driver.c
|   |
|   |-- flow_driver/                        # Parser CXOF via ESP-IDF UART
|   |   |-- CMakeLists.txt
|   |   |-- include/flow_driver.h
|   |   +-- flow_driver.c
|   |
|   |-- motor_driver/                       # PWM via LEDC su D0-D3
|   |   |-- CMakeLists.txt
|   |   |-- include/motor_driver.h
|   |   +-- motor_driver.c
|   |
|   |-- pid_controller/                     # PID generico riutilizzabile
|   |   |-- CMakeLists.txt
|   |   |-- include/pid_controller.h
|   |   +-- pid_controller.c
|   |
|   |-- sensor_fusion/                      # Pipeline fusione (Fase 2+)
|   |   |-- CMakeLists.txt
|   |   |-- include/sensor_fusion.h
|   |   +-- sensor_fusion.c
|   |
|   |-- battery_monitor/                    # ADC D8, buzzer D9, LED D10
|   |   |-- CMakeLists.txt
|   |   |-- include/battery_monitor.h
|   |   +-- battery_monitor.c
|   |
|   +-- uros_interface/                     # Bridge micro-ROS <-> FreeRTOS
|       |-- CMakeLists.txt
|       |-- include/uros_interface.h
|       +-- uros_interface.c
|
|-- managed_components/
|   +-- micro_ros_espidf_component/         # Scaricato automaticamente
|
|-- docs/
|   |-- HARDWARE_BOM.md
|   |-- HARDWARE_DIAGRAM.md
|   |-- PROJECT_CONCEPT.md
|   |-- PROJECT_CONTEXT.md
|   +-- specs/
|       +-- 2026-03-10-swarm-drone-architecture-design.md  # Questo file
|
+-- legacy/                                 # Vecchio firmware Arduino (riferimento)
    |-- src/main.cpp
    |-- lib/IMU_Driver/
    |-- lib/Optical_Flow/
    +-- include/
```

Ogni componente e indipendente con il proprio CMakeLists.txt e header pubblico.
Il vecchio firmware Arduino viene spostato in `legacy/` come riferimento per la logica dei sensori.

---

## 8. Roadmap di sviluppo

### Fase 0A — Architettura + sensori raw (hardware attuale, niente motori)
- Creare progetto ESP-IDF con struttura componenti
- Portare driver IMU (MPU6050 via I2C ESP-IDF, 400kHz, 1kHz sample rate)
- Portare driver Flow (parser CXOF via UART ESP-IDF, 19200 baud)
- Implementare task_battery (ADC su D8, buzzer su D9)
- Integrare micro-ROS: agent UDP, publisher per imu/raw, flow, range, battery
- Validare su Foxglove: plottare gyro, accel, flow_x/y, range, tensione batteria
- **Criterio di successo:** tutti i topic visibili e coerenti su Foxglove

### Fase 0B — Test motori
- Implementare motor_driver (PWM via LEDC su D0-D3)
- Topic /drone_N/cmd_motor_test per comandare ogni motore singolarmente dal PC
- Pubblicare /drone_N/motors (duty% attuali)
- Verificare: ogni motore gira, duty <-> velocita coerente, nessun surriscaldamento
- **Criterio di successo:** tutti e 4 i motori controllabili da Foxglove/CLI

### Fase 1 — Stabilizzazione attitudine
- Implementare pid_controller (generico, con anti-windup e saturazione)
- task_pid_attitude: 3 istanze PID (roll, pitch, yaw) a 1kHz
- Mixing: PID output -> duty% per 4 motori (configurazione quadricottero X)
- Subscribere a /drone_N/cmd_attitude per setpoint
- Tuning PID via Foxglove (visualizza errore, output, setpoint in tempo reale)
- **Criterio di successo:** hover stabile con solo IMU

### Fase 2 — Velocity hold
- Valutare precisione optical flow in volo
- Se sufficiente: implementare task_fusion e task_pid_velocity
- Stima velocita da flow compensato + altezza ToF
- PID velocita (50Hz) genera setpoint per PID attitudine
- **Criterio di successo:** drone fermo nell'aria senza drift

### Fase 3 — Position control
- Implementare task_pid_position (20Hz)
- Posizione fusa (flow + IMU) -> setpoint velocita -> setpoint attitudine -> motori
- Comandi dal PC via /drone_N/cmd_position
- **Criterio di successo:** drone raggiunge posizione target comandata dal PC

---

## 9. Cosa si porta dal codice Arduino

| Componente | Azione | Note |
|---|---|---|
| State machine CXOF | Porta la logica | Parsing corretto, reverse-engineered. Riscrivi con uart_read_bytes() |
| Integrazione yaw manuale | Porta il concetto | Deadzone 0.2 deg/s, integrazione gyroZ. Riscrivi con registri raw |
| Pipeline di fusione (4 step) | Parcheggiata | Compensazione gyro, tilt, body->world. Da rivalutare in Fase 2 |
| Calibrazione IMU | Riscrivi | Lettura registri raw, calcolo offset manuale (no libreria MPU6050_light) |
| Lettura MPU6050 | Riscrivi | Da mpu->getAccX() a lettura registri 0x3B-0x48 via I2C ESP-IDF |
| Comandi seriali (r/d/t/c/m) | Sostituisci | I comandi arrivano via topic micro-ROS, seriale solo per debug log |

---

## 10. Dipendenze esterne

| Componente | Sorgente | Note |
|---|---|---|
| ESP-IDF | v5.x (ultima stabile) | Framework, FreeRTOS, driver I2C/UART/LEDC |
| micro_ros_espidf_component | GitHub (managed component) | Libreria micro-ROS per ESP-IDF |
| micro-ROS agent | Docker o build nativo sul PC | Bridge XRCE-DDS <-> FastDDS |
| Foxglove Studio | Desktop app o web | Visualizzazione topic ROS2 |
| ROS2 (Humble/Jazzy) | Sul PC | Per ros2 topic echo, debug, nodi custom |
