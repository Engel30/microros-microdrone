# Firmware: Architettura ESP-IDF + micro-ROS

**Framework:** ESP-IDF v5.4, FreeRTOS, micro-ROS (XRCE-DDS over UDP WiFi)

---

## 1. Dual-Core Architecture

**Core 0 (Communications):**
- WiFi stack
- task_microros (50Hz): riceve comandi, pubblica telemetria
- task_battery (1Hz): ADC batteria, alert buzzer

**Core 1 (Flight Control):**
- task_imu (1kHz): MPU6050 I2C
- task_flow (~20Hz): CXOF UART parser
- task_pid_attitude (1kHz): stabilizzazione attitudine
- task_motors (1kHz): LEDC PWM motori
- task_fusion (100Hz, Fase 2+): sensor fusion
- task_pid_velocity (50Hz, Fase 2): velocity hold
- task_pid_position (20Hz, Fase 3): position control

---

## 2. Queue System (FreeRTOS)

| Queue | Produttore → Consumatori | Scopo |
|:------|:--------------------------|:------|
| **imu_queue** | task_imu → pid_att, fusion, microros | Accel + Gyro |
| **flow_queue** | task_flow → fusion, microros | Pixel displacement + ToF |
| **state_queue** | task_fusion → pid_vel, pid_pos, microros | Attitude + velocity |
| **cmd_queue** | task_microros → task_pid_attitude | Comandi attitude |
| **motor_queue** | task_pid_attitude → motors, microros | PWM duty 4 motori |

---

## 3. Sensori

**IMU (MPU-6050 I2C 0x68):**
- Gyro ±500°/s, Accel ±4g, DLPF ~42Hz
- Sample rate 1kHz (clock PLL gyro X)
- Calibrazione offset al boot (static 30s)

**Optical Flow (CXOF UART 19200):**
- PMW3901 + VL53L0X (Matek 3901-L0X clone)
- Frame 11-byte, re-sync parser
- Scale: 1.294e-2 rad/count (7.35× ArduPilot)
- Gyro compensation in main loop

**Battery (ADC D8):**
- Partitore 100k/100k su VBAT
- Read 1Hz, alert se < 3.0V

---

## 4. Roadmap Fasi

| Fase | Status | Obiettivo |
|:-----|:-------|:----------|
| **0A** | ✅ | Sensori raw, logging CSV |
| **0B** | ⏸️ | Motor driver (ESP32 bruciato, fix: pull-down 10kΩ) |
| **1** | 📅 | PID attitudine, hover stabile |
| **2** | 📅 | Velocity hold optical flow |
| **3** | 📅 | Position control waypoint |

---

## 5. Build e Flash

```bash
# Attiva environment (ogni terminale)
. ~/esp/esp-idf/export.sh

# Build
idf.py build

# Flash + Monitor
idf.py -p /dev/ttyACM0 flash monitor

# Solo Monitor
idf.py -p /dev/ttyACM0 monitor
```

**Post-flash (WSL only):** Rifare da PowerShell Admin su Windows
```powershell
usbipd attach --wsl --busid 1-6
```

---

## Struttura Progetto

```
components/
├── common/              # drone_config.h, drone_types.h
├── imu_driver/          # MPU6050 I2C
├── flow_driver/         # CXOF UART parser
├── motor_driver/        # LEDC PWM (Fase 0B)
├── pid_controller/      # Generic PID (Fase 1)
├── sensor_fusion/       # IMU+flow fusion (Fase 2)
├── battery_monitor/     # ADC + buzzer
└── uros_interface/      # micro-ROS bridge
```

---

## Riferimenti

- Design spec completo: `docs/specs/2026-03-10-swarm-drone-architecture-design.md`
- Setup ambiente: `docs/03-SETUP-AMBIENTE.md`
- Hardware: `docs/01-HARDWARE-BOM.md`
