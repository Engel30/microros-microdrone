# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Educational low-cost drone swarm platform on ESP32-S3. Currently migrating from an Arduino prototype (`old/`) to **ESP-IDF + FreeRTOS + micro-ROS** architecture. The design spec is finalized, implementation is starting from Phase 0A.

**Design spec (source of truth):** `docs/specs/2026-03-10-swarm-drone-architecture-design.md`

## Architecture

- **Framework:** ESP-IDF (C), FreeRTOS tasks, micro-ROS (Micro XRCE-DDS over UDP WiFi)
- **MCU:** Seeed XIAO ESP32-S3 (dual core Xtensa LX7, 240MHz)
- **Core 0:** WiFi stack, task_microros (50Hz), task_battery (1Hz)
- **Core 1 (flight-critical):** task_imu (1kHz), task_flow (~20Hz), task_pid_attitude (1kHz), task_motors (1kHz)
- **Inter-task communication:** FreeRTOS queues (imu_queue, flow_queue, state_queue, cmd_queue, motor_queue)
- **Swarm scalability:** ROS2 namespace `/drone_N/`, same firmware with configurable DRONE_ID
- **PC side:** micro-ROS agent + ROS2 + Foxglove Studio for visualization

## Hardware

| Pin | Function | Bus |
|-----|----------|-----|
| D0-D3 | Motors 1-4 (FL/FR/BL/BR) | PWM (MOSFET SI2302 low-side) |
| D4/D5 | MPU6050 IMU | I2C 400kHz |
| D6/D7 | PMW3901+VL53L1X optical flow | UART 19200 baud (CXOF protocol) |
| D8 | Battery voltage sense | ADC (100k/100k divider) |
| D9 | Buzzer | PWM |
| D10 | Status LED | GPIO |

Motors are 8520 coreless brushed (3.7V 1S), driven directly via PWM through SI2302 MOSFETs with 1N5819 flyback diodes.

## ESP-IDF Component Structure (planned)

Components in `components/`: `common`, `imu_driver`, `flow_driver`, `motor_driver`, `pid_controller`, `sensor_fusion`, `battery_monitor`, `uros_interface`. Each has its own `CMakeLists.txt` and public `include/` header.

## Build Commands (ESP-IDF)

```bash
idf.py build                    # Build
idf.py -p /dev/ttyACM0 flash    # Flash
idf.py monitor                  # Serial monitor
idf.py -p /dev/ttyACM0 flash monitor  # Flash + monitor
```

## Key Sensor Details

**MPU6050:** Registers 0x3B-0x48 for raw data. DLPF enabled (~42Hz cutoff for motor vibration). SMPLRT_DIV=0 for 1kHz. Yaw integrated manually from gyroZ with 0.2 deg/s deadzone (no magnetometer).

**Optical Flow (CXOF protocol):** 11-byte binary frames `[FE 04 Xl Xh Yl Yh Dl Dh CK SQ AA]`. Distance in centimeters (convert to mm). State machine parser handles re-sync when header appears as checksum value. Reference implementation in `old/lib/Optical_Flow/OpticalFlow.cpp`.

## Development Roadmap

Phase 0A (current) → 0B → 1 → 2 → 3. Each phase validates the previous before building on top. See design spec for details.

## Conventions

- Language: C (required by ESP-IDF and micro-ROS APIs)
- User language: Italian
- All documentation in `docs/`, specs in `docs/specs/`
- Old Arduino reference code in `old/` — do not modify, use as logic reference for sensor drivers
