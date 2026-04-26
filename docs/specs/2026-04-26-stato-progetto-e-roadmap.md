# Stato Progetto e Roadmap — Source of Truth

**Data:** 2026-04-26
**Autore:** Angelo + Claude
**Stato:** Attivo (sostituisce le spec precedenti)

Questo documento è il riferimento unico per architettura, decisioni e roadmap del progetto. Le altre spec in questa cartella sono storiche.

---

## 1. Snapshot

| Asset | Stato |
|---|---|
| Fase 0A — Sensori raw (IMU + Flow + Battery + log Foxglove) | ✅ Completata |
| Fase 0B — Motor driver | ⚠️ Bloccata 2026-03-19 (ESP32 bruciato, GPIO floating + MOSFET) |
| PCB custom v1.0 (carrier 2-layer, pull-down 10kΩ, switch arm, partitore V-Sense) | 🔄 Layout EasyEDA in corso, ordine JLCPCB pendente |
| Nuovi ESP32-S3 XIAO | 📦 In arrivo |
| Componenti firmware esistenti | `common`, `imu_driver`, `flow_driver`, `motor_driver`, `battery_monitor`, `uros_interface` |
| Componenti firmware da scrivere | `pid_controller` (Fase 1), `sensor_fusion` (Fase 2), `comm_protocol` ESP-NOW (Fase Swarm) |

---

## 2. Decisioni di architettura (consolidate)

### 2.1 Hardware singolo drone — confermato

- **MCU:** Seeed XIAO ESP32-S3 (dual core LX7 240MHz, 8MB flash, 512KB SRAM)
- **IMU:** MPU6050 (GY-521) via I2C 400kHz, 1kHz sample, DLPF ~42Hz, ±500°/s, ±4g
- **Optical Flow:** modulo CXOF (PMW3901 clone P3901 + VL53L1X ToF) via UART 19200, ~20Hz, scale calibrata `1.294e-2 rad/count`
- **Propulsione:** 4× 8520 brushed 1S, MOSFET SI2302 low-side, diodo flyback 1N5819, eliche 40mm, frame 75mm, batteria LiPo 1S 450-550mAh BT2.0
- **Carrier board:** PCB custom v1.0 (vedi `docs/pcb-custom/pcb-design-spec.md`). Risolve l'incidente del 2026-03-19 con pull-down 10kΩ + serie 100Ω su ogni gate MOSFET e switch arm motori separato

Pinout XIAO confermato (D0-D3 motori PWM, D4/D5 I2C, D6/D7 UART flow, D8 V-Sense ADC, D9 buzzer, D10 LED status).

### 2.2 Stack software singolo drone — confermato con riserva

**Per il singolo drone (Fase 0B → 1 → 2 → 3) si tiene micro-ROS.**

Motivazioni:
- Già funzionante e validato su Foxglove
- Tooling ROS2 (rosbag, plotjuggler, Foxglove) prezioso per il PID tuning di Fase 1
- Tassa RAM (~50-80KB) e build time accettabili in single-drone
- Il flight control (`task_imu`, `task_pid_attitude`, `task_motors`) è disaccoppiato dal trasporto: la migrazione futura tocca solo il task di comunicazione

Stack:
- ESP-IDF v5.4 + FreeRTOS
- micro-ROS (Micro XRCE-DDS) over UDP/WiFi → agent sul PC → Foxglove Studio
- Namespace ROS2 `/drone_N/` configurabile (DRONE_ID in `drone_config.h`)

### 2.3 Stack software per lo sciame — confermato

**Quando si passa al multi-drone, micro-ROS viene rimosso e sostituito da ESP-NOW peer-to-peer (Approccio B).**

Motivazioni:
- WiFi infrastrutturato non disponibile nello scenario target
- Latenza ESP-NOW ~1-2ms vs UDP/micro-ROS ~10-50ms
- Liberazione RAM e build time
- Contributo originale per la tesi: protocollo binario strutturato con header, ACK, retry

Modello (riassunto, dettaglio in `docs/05-ARCHITETTURA-SWARM.md`):
- 4 ruoli: **Explorer** (mappa), **Relay** (riposizionato dal PC su RSSI), **Rescue** (missione), **Bridge** (AP locale + UDP↔ESP-NOW gateway)
- PC = cervello (mission planning, mappa, dashboard Flask+WebSocket)
- Drone = esecutore (PID, obstacle avoidance locale)
- Componente firmware nuovo: `comm_protocol/` (sostituisce `uros_interface/`)

### 2.4 Esclusioni esplicite

- **STM32 + UWB DW3000:** non in scope. Valutato e scartato. Il progetto resta su ESP32-S3 + optical flow per positioning.
- **ESP-WIFI-MESH:** documentato come future work, non implementato.
- **Magnetometro:** assente, yaw stimato per integrazione gyroZ con deadzone 0.2°/s.

---

## 3. Architettura firmware (singolo drone)

### 3.1 Dual-core split

**Core 0 — Comunicazione e monitoring**
- WiFi stack (pinned di default)
- `task_microros` 50Hz prio 2 — bridge micro-ROS ↔ FreeRTOS queue
- `task_battery` 1Hz prio 1 — ADC D8, allarme buzzer

**Core 1 — Flight-critical**
- `task_imu` 1kHz prio 5 — MPU6050 → `imu_queue`
- `task_flow` ~20Hz prio 4 — parser CXOF → `flow_queue`
- `task_pid_attitude` 1kHz prio 6 (Fase 1) — gyro raw → `motor_queue`
- `task_motors` 1kHz prio 6 (Fase 0B) — LEDC PWM D0-D3
- `task_fusion` 100Hz prio 4 (Fase 2+) — IMU+flow → `state_queue`
- `task_pid_velocity` 50Hz prio 4 (Fase 2) — `state_queue` → setpoint attitude
- `task_pid_position` 20Hz prio 3 (Fase 3) — waypoint → setpoint velocity

### 3.2 Queue inter-task

| Queue | Produttore → Consumatori | Profondità |
|---|---|---|
| `imu_queue` | `task_imu` → pid_attitude, fusion, microros | 5 |
| `flow_queue` | `task_flow` → fusion, microros | 3 |
| `state_queue` | `task_fusion` → pid_vel, pid_pos, microros | 3 |
| `cmd_queue` | `task_microros` → pid_* | 1 |
| `motor_queue` | `task_pid_attitude` → motors, microros | 1 |

Il PID di attitudine legge gyro raw direttamente da `imu_queue` per massima reattività; la fusion alimenta solo i loop esterni (Fase 2+).

### 3.3 Topic micro-ROS

Pubblicati: `/drone_N/imu/raw`, `/drone_N/flow`, `/drone_N/range`, `/drone_N/battery`, `/drone_N/motors`, `/drone_N/attitude` (F2), `/drone_N/odom` (F2).
Sottoscritti: `/drone_N/cmd_motor_test` (F0B), `/drone_N/cmd_attitude` (F1), `/drone_N/cmd_position` (F3).
QoS: BEST_EFFORT telemetria, RELIABLE comandi.

---

## 4. Roadmap operativa

| Fase | Stato | Obiettivo | Criterio di successo |
|---|---|---|---|
| 0A | ✅ | Sensori raw + log Foxglove | Tutti i topic visibili e coerenti |
| **0B** | **🔜 prossima** | Bring-up PCB custom + motor driver | 4 motori controllabili da Foxglove/CLI, no surriscaldamento, MOSFET stabili al boot |
| 1 | 📅 | PID attitudine, hover stabile | Hover con sola IMU |
| 2 | 📅 | Velocity hold con optical flow | Drone fermo nell'aria senza drift |
| 3 | 📅 | Position control waypoint | Drone raggiunge target da PC |
| Swarm | 📅 | Migrazione `uros_interface` → `comm_protocol` ESP-NOW + Bridge AP | 4 droni (Explorer/Relay/Rescue/Bridge) operativi simultanei |

### 4.1 Prossimi step concreti (Fase 0B)

1. Completare layout PCB EasyEDA (vedi `docs/pcb-custom/`)
2. Ordine JLCPCB
3. Saldatura + ispezione visiva (continuità pull-down, polarità diodi, switch arm OFF al boot)
4. Bring-up incrementale: alimentazione → XIAO → I2C IMU → UART flow → un motore alla volta con limite duty
5. Re-test stack Fase 0A su nuova board
6. Test motore singolo via `/drone_N/cmd_motor_test`
7. Test 4 motori coordinati

---

## 5. Vincoli e decisioni di principio

| Vincolo | Target | Ragione |
|---|---|---|
| Costo per unità | <30€ | Flotta 4-8 droni |
| Peso | <65g | TWR > 2.5:1 |
| Autonomia volo | 7-10 min | Limite fisico brushed 1S |
| Linguaggio firmware | C puro | Vincolo ESP-IDF + micro-ROS |
| Source of truth documentale | Questo file | Le altre spec sono storiche |

---

## 6. Mapping ai documenti operativi

| Tema | Documento operativo |
|---|---|
| Hardware / BOM | `docs/01-HARDWARE-BOM.md` |
| Firmware (riassunto operativo) | `docs/02-FIRMWARE-ARCHITETTURA.md` |
| Setup ambiente | `docs/03-SETUP-AMBIENTE.md` |
| Visione + timeline | `docs/04-VISIONE-PROGETTO.md` |
| Architettura swarm (dettaglio protocollo) | `docs/05-ARCHITETTURA-SWARM.md` |
| PCB | `docs/pcb-custom/` |
| Storico decisioni | `docs/specs/2026-03-10-...md`, `docs/specs/2026-04-14-...md` |

I documenti operativi devono restare allineati a questa spec. In caso di conflitto, vince questo file.

---

## 7. Storico

- **2026-03-10** — Spec architettura ESP-IDF + micro-ROS (singolo drone). Valida per HW/sensori/fasi 0-3, superata per la parte swarm.
- **2026-04-14** — Brainstorming swarm. Decisione: Approccio B (ESP-NOW + Bridge AP, 4 ruoli). Rimane riferimento per la fase swarm.
- **2026-04-16** — Esplorazione pivot STM32+UWB. **Scartato e rimosso il 2026-04-26.**
- **2026-04-26** — Questo documento. Consolidato. micro-ROS resta per single-drone, ESP-NOW arriva con lo swarm.
