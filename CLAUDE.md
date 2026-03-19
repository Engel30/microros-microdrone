# CLAUDE.md

## Comportamento

- **Lingua:** rispondi sempre in italiano
- **Linguaggio firmware:** C puro (richiesto da ESP-IDF e micro-ROS)
- **Non modificare** il codice in `old/` — è riferimento per la logica dei sensori Arduino
- **Ogni componente ha un README.md** nella sua cartella: aggiornalo quando aggiungi/modifichi funzionalità
- **Design spec (source of truth):** `docs/specs/2026-03-10-swarm-drone-architecture-design.md`
- **Setup ambiente:** `docs/setup-guide.md`
- Documenti in `docs/`, specs in `docs/specs/`

## Build

```bash
. ~/esp/esp-idf/export.sh              # Attiva ambiente (ogni nuovo terminale)
idf.py build                           # Compila
idf.py -p /dev/ttyACM0 flash           # Flash
idf.py -p /dev/ttyACM0 monitor         # Monitor seriale (Ctrl+] per uscire)
idf.py -p /dev/ttyACM0 flash monitor   # Flash + monitor
```

Dopo il flash, l'ESP32 si resetta e la USB si scollega da WSL. Serve rifare da PowerShell Admin:
```powershell
usbipd attach --wsl --busid 1-6
```

## Architettura

- **Framework:** ESP-IDF v5.4, FreeRTOS, micro-ROS (Micro XRCE-DDS over UDP WiFi)
- **MCU:** Seeed XIAO ESP32-S3 (dual core Xtensa LX7, 240MHz, 8MB flash, 512KB SRAM)
- **Core 0:** WiFi stack, task_microros (50Hz), task_battery (1Hz)
- **Core 1 (flight-critical):** task_imu (1kHz), task_flow (~20Hz), task_pid_attitude (1kHz), task_motors (1kHz)
- **Comunicazione inter-task:** FreeRTOS queues (imu_queue, flow_queue, state_queue, cmd_queue, motor_queue)
- **Scalabilità sciame:** namespace ROS2 `/drone_N/`, stesso firmware con DRONE_ID configurabile
- **PC side:** micro-ROS agent + ROS2 + Foxglove Studio

## Hardware — Pinout

| Pin | GPIO | Funzione | Bus/Tipo |
|-----|------|----------|----------|
| D0 | GPIO_NUM_1 | Motore 1 (Front-Left) | PWM LEDC |
| D1 | GPIO_NUM_2 | Motore 2 (Rear-Left) | PWM LEDC |
| D2 | GPIO_NUM_3 | Motore 3 (Rear-Right) | PWM LEDC |
| D3 | GPIO_NUM_4 | Motore 4 (Front-Right) | PWM LEDC |
| D4 | GPIO_NUM_5 | SDA (MPU6050) | I2C 400kHz |
| D5 | GPIO_NUM_6 | SCL (MPU6050) | I2C 400kHz |
| D6 | GPIO_NUM_43 | TX1 (Optical Flow) | UART 19200 |
| D7 | GPIO_NUM_44 | RX1 (Optical Flow) | UART 19200 |
| D8 | GPIO_NUM_7 | Tensione batteria | ADC |
| D9 | GPIO_NUM_8 | Buzzer | PWM |
| D10 | GPIO_NUM_9 | LED status | GPIO |

Motori: 8520 coreless brushed 3.7V 1S, MOSFET SI2302 low-side, diodo flyback 1N5819.

## Struttura progetto

```
microros-microdrone/
├── CMakeLists.txt                  # Progetto ESP-IDF top-level
├── sdkconfig.defaults              # Flash 8MB, FreeRTOS tick 1kHz, WiFi Core 0
├── main/
│   ├── CMakeLists.txt
│   └── main.c                      # app_main(): crea queue e lancia task
├── components/
│   ├── common/                     # Tipi e config condivisi da tutti
│   │   └── include/
│   │       ├── drone_config.h      # Pin, frequenze, priorità, costanti
│   │       └── drone_types.h       # imu_data_t, flow_data_t, state_t, motor_cmd_t
│   ├── imu_driver/                 # MPU6050 via I2C (Fase 0A)
│   ├── flow_driver/                # Parser CXOF via UART (Fase 0A)
│   ├── motor_driver/               # PWM via LEDC (Fase 0B)
│   ├── pid_controller/             # PID generico con anti-windup (Fase 1)
│   ├── sensor_fusion/              # Fusione IMU + flow (Fase 2+)
│   ├── battery_monitor/            # ADC batteria + buzzer (Fase 0A)
│   └── uros_interface/             # Bridge micro-ROS <-> FreeRTOS queues (Fase 0A)
├── docs/
│   ├── setup-guide.md              # Installazione ambiente completa
│   └── specs/                      # Design spec
└── old/                            # Vecchio firmware Arduino (solo riferimento)
```

## Sensori — dettagli chiave

**MPU6050 (IMU):** Registri 0x3B-0x48 per dati raw. DLPF ~42Hz (CONFIG=0x03). SMPLRT_DIV=0 → 1kHz. Gyro ±500°/s, Accel ±4g. Yaw integrata da gyroZ con deadzone 0.2°/s (no magnetometro). I2C addr 0x68.

**Optical Flow (CXOF):** Frame 11 byte `[FE 04 Xl Xh Yl Yh Dl Dh CK SQ AA]`. Distanza in cm (convertire in mm). State machine parser con re-sync. Riferimento: `old/lib/Optical_Flow/OpticalFlow.cpp`.

## Roadmap

- **Fase 0A (corrente):** sensori raw + micro-ROS → Foxglove
- **Fase 0B:** test motori via topic ROS2
- **Fase 1:** PID attitudine, hover stabile
- **Fase 2:** velocity hold con optical flow
- **Fase 3:** position control

## FreeRTOS — task e code

| Task | Core | Freq | Priorità | Fase |
|------|------|------|----------|------|
| task_imu | 1 | 1kHz | 5 | 0A |
| task_flow | 1 | ~20Hz | 4 | 0A |
| task_microros | 0 | 50Hz | 2 | 0A |
| task_battery | 0 | 1Hz | 1 | 0A |
| task_motors | 1 | 1kHz | 6 | 0B |
| task_pid_attitude | 1 | 1kHz | 6 | 1 |

| Queue | Produttore → Consumatori | Depth |
|-------|--------------------------|-------|
| imu_queue | task_imu → task_pid, task_fusion, task_microros | 5 |
| flow_queue | task_flow → task_fusion, task_microros | 3 |
| state_queue | task_fusion → task_pid_vel, task_pid_pos, task_microros | 3 |
| cmd_queue | task_microros → task_pid | 1 |
| motor_queue | task_pid → task_motors, task_microros | 1 |
