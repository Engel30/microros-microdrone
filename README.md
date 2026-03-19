# micro-ROS Microdrone — ESP32-S3 Swarm Platform

Piattaforma drone didattica low-cost basata su ESP32-S3 per sciami di micro-droni coordinati via WiFi.

**Framework:** ESP-IDF v5.4 + FreeRTOS + micro-ROS
**Hardware:** Seeed XIAO ESP32-S3, MPU6050, PMW3901+VL53L1X, motori brushed 8520
**Comunicazione:** micro-ROS (Micro XRCE-DDS) su UDP WiFi verso PC con Foxglove Studio

---

## Architettura

Ogni drone è un nodo micro-ROS con namespace `/drone_N/`. Il PC esegue il micro-ROS agent e Foxglove Studio per visualizzazione e comandi.

```
Drone (ESP32-S3)              PC
+------------------+  UDP   +------------------+
| micro-ROS client | -----> | micro-ROS agent  |
| (XRCE-DDS)       |        |       |          |
+------------------+        |       v          |
                             | ROS2 DDS         |
                             |       |          |
                             |       v          |
                             | Foxglove Studio  |
                             +------------------+
```

### Dual Core

- **Core 0:** WiFi, micro-ROS (50Hz), battery monitor (1Hz)
- **Core 1:** Sensori (IMU 1kHz, Flow 20Hz), PID (1kHz), motori (1kHz)

## Struttura progetto

```
microros-microdrone/
├── CMakeLists.txt                  # Progetto ESP-IDF top-level
├── sdkconfig.defaults              # Configurazione hardware
├── main/
│   └── main.c                      # Punto di ingresso: crea queue e lancia task
├── components/
│   ├── common/                     # Tipi condivisi e configurazione pin/frequenze
│   ├── imu_driver/                 # Driver MPU6050 via I2C a 1kHz
│   ├── flow_driver/                # Parser protocollo CXOF via UART a ~20Hz
│   ├── motor_driver/               # Controllo PWM 4 motori via LEDC
│   ├── pid_controller/             # Controller PID generico con anti-windup
│   ├── sensor_fusion/              # Fusione IMU + optical flow (Fase 2+)
│   ├── battery_monitor/            # Monitoraggio tensione + buzzer allarme
│   └── uros_interface/             # Bridge micro-ROS <-> FreeRTOS queues
├── docs/
│   ├── setup-guide.md              # Guida installazione ambiente
│   ├── specs/                      # Design spec dettagliato
│   ├── HARDWARE_DIAGRAM.md         # Schema connessioni
│   ├── HARDWARE_BOM.md             # Bill of Materials
│   ├── PROJECT_CONTEXT.md          # Contesto e architettura
│   └── PROJECT_CONCEPT.md          # Studio di fattibilità
└── old/                            # Vecchio firmware Arduino (riferimento)
```

Ogni componente ha un proprio `README.md` con documentazione di API, stato e dipendenze.

## Setup ambiente

**Requisiti:** Windows 11 + WSL2 (Ubuntu 22.04), ESP-IDF v5.4, ROS2

Guida completa: [`docs/setup-guide.md`](docs/setup-guide.md)

### Quick start

```bash
# Attiva ambiente ESP-IDF
. ~/esp/esp-idf/export.sh

# Compila
idf.py build

# Flash (prima fare usbipd attach dalla PowerShell Windows)
idf.py -p /dev/ttyACM0 flash

# Monitor seriale (Ctrl+] per uscire)
idf.py -p /dev/ttyACM0 monitor
```

## Hardware

| Pin | Funzione | Bus |
|-----|----------|-----|
| D0 | Motore Front-Left | PWM |
| D1 | Motore Rear-Left | PWM |
| D2 | Motore Rear-Right | PWM |
| D3 | Motore Front-Right | PWM |
| D4/D5 | MPU6050 IMU | I2C 400kHz |
| D6/D7 | Optical Flow + ToF | UART 19200 |
| D8 | Tensione batteria | ADC |
| D9 | Buzzer | PWM |
| D10 | LED status | GPIO |

## Roadmap

| Fase | Obiettivo | Stato |
|------|-----------|-------|
| 0A | Sensori raw + micro-ROS → Foxglove | In corso (IMU OK, Flow OK, battery e micro-ROS pending) |
| 0B | Test motori via topic ROS2 | Da implementare |
| 1 | Stabilizzazione attitudine (PID hover) | Da implementare |
| 2 | Velocity hold (optical flow) | Da implementare |
| 3 | Position control (comandi dal PC) | Da implementare |

## Documentazione

- [`docs/specs/`](docs/specs/) — Design spec (source of truth)
- [`docs/setup-guide.md`](docs/setup-guide.md) — Setup completo ambiente di sviluppo
- [`docs/HARDWARE_DIAGRAM.md`](docs/HARDWARE_DIAGRAM.md) — Schema connessioni
- [`docs/HARDWARE_BOM.md`](docs/HARDWARE_BOM.md) — Bill of Materials
