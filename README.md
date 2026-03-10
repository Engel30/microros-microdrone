# micro-ROS Microdrone — ESP32-S3 Swarm Platform

Piattaforma drone didattica low-cost basata su ESP32-S3 per sciami di micro-droni coordinati via WiFi.

**Framework:** ESP-IDF + FreeRTOS + micro-ROS
**Hardware:** Seeed XIAO ESP32-S3, MPU6050, PMW3901+VL53L1X, motori brushed 8520
**Comunicazione:** micro-ROS (Micro XRCE-DDS) su UDP WiFi verso PC con Foxglove Studio

---

## Stato attuale

In fase di migrazione da prototipo Arduino (in `old/`) ad architettura ESP-IDF modulare.
Spec completa: [`docs/specs/2026-03-10-swarm-drone-architecture-design.md`](docs/specs/2026-03-10-swarm-drone-architecture-design.md)

## Architettura

Ogni drone e un nodo micro-ROS con namespace `/drone_N/`. Il PC esegue il micro-ROS agent e Foxglove Studio per visualizzazione e comandi.

```
Drone (ESP32-S3)              PC
+------------------+  UDP   +------------------+
| micro-ROS client | -----> | micro-ROS agent  |
| (XRCE-DDS)      |        |       |          |
+------------------+        |       v          |
                            | ROS2 DDS         |
                            |       |          |
                            |       v          |
                            | Foxglove Studio  |
                            +------------------+
```

### Dual Core

- **Core 0:** WiFi, micro-ROS, battery monitor
- **Core 1:** Sensori (IMU 1kHz, Flow 20Hz), PID (1kHz), motori

## Roadmap

| Fase | Obiettivo | Stato |
|---|---|---|
| 0A | Architettura ESP-IDF + sensori raw su Foxglove | Da implementare |
| 0B | Test motori (PWM da Foxglove/CLI) | Da implementare |
| 1 | Stabilizzazione attitudine (PID hover) | Da implementare |
| 2 | Velocity hold (optical flow) | Da implementare |
| 3 | Position control (comandi dal PC) | Da implementare |

## Documentazione

- [`docs/specs/`](docs/specs/) — Specifiche di design
- [`docs/HARDWARE_DIAGRAM.md`](docs/HARDWARE_DIAGRAM.md) — Schema connessioni
- [`docs/HARDWARE_BOM.md`](docs/HARDWARE_BOM.md) — Bill of Materials
- [`docs/PROJECT_CONTEXT.md`](docs/PROJECT_CONTEXT.md) — Contesto e architettura
- [`docs/PROJECT_CONCEPT.md`](docs/PROJECT_CONCEPT.md) — Studio di fattibilita
- [`old/`](old/) — Vecchio firmware Arduino (riferimento sensori)
