# micro-ROS Microdrone — ESP32-S3 Swarm Platform

Piattaforma drone didattica low-cost (<30€/unità) basata su ESP32-S3, pensata per studiare swarm intelligence, sistemi distribuiti e mesh networking su hardware reale.

**Framework:** ESP-IDF v5.4 + FreeRTOS + micro-ROS
**Hardware:** Seeed XIAO ESP32-S3, MPU6050, PMW3901 + VL53L1X, motori brushed 8520, PCB custom
**Telemetria:** micro-ROS (Micro XRCE-DDS) su UDP WiFi → agent sul PC → Foxglove Studio

> **Stato del progetto:** [`docs/STATO.md`](docs/STATO.md) — dove siamo, cosa blocca, prossimi passi.

---

## Architettura

Ogni drone è un nodo micro-ROS con namespace `/drone_N/`. Il PC esegue l'agent e Foxglove Studio per visualizzazione e comandi.

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

Il carico è diviso sui due core: **Core 0** tiene WiFi, micro-ROS e monitoraggio batteria; **Core 1** resta dedicato al volo (sensori, PID, motori), così la latenza del loop di controllo non dipende dalla rete. È anche ciò che rende sostituibile il trasporto: passare a ESP-NOW per lo sciame tocca solo il task di comunicazione.

Dettaglio: [`docs/grounding/03-FIRMWARE-ARCHITETTURA.md`](docs/grounding/03-FIRMWARE-ARCHITETTURA.md)

## Quick start

```bash
. ~/esp/esp-idf/export.sh    # Attiva ambiente ESP-IDF
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Su WSL serve prima `usbipd attach --wsl --busid 1-6` da PowerShell Admin.

Procedura completa di accensione (drone + agent + Foxglove): [`docs/grounding/05-BRINGUP-QUICKSTART.md`](docs/grounding/05-BRINGUP-QUICKSTART.md)
Installazione dell'ambiente da zero: [`docs/grounding/04-SETUP-AMBIENTE.md`](docs/grounding/04-SETUP-AMBIENTE.md)

## Hardware

| Pin | Funzione | Bus |
|-----|----------|-----|
| D0-D3 | Motori FL, RL, RR, FR | PWM LEDC 20kHz |
| D4/D5 | MPU6050 IMU | I2C 400kHz |
| D6/D7 | Optical flow + ToF | UART 19200 |
| D8 | Tensione batteria | ADC |
| D9 | Buzzer | PWM |
| D10 | LED status | GPIO |

Driver motori: MOSFET AO3400A low-side + diodo flyback SS14, **pull-down 10kΩ obbligatoria su ogni gate** (senza, i GPIO flottanti al boot accendono i motori a caso e gli spike distruggono l'ESP32 — è già successo).

BOM completa e connessioni: [`docs/grounding/02-HARDWARE-BOM.md`](docs/grounding/02-HARDWARE-BOM.md) · PCB: [`docs/pcb-custom/`](docs/pcb-custom/)

## Struttura

```
main/           app_main() + i task FreeRTOS
components/     driver e logica, un README per componente
ros2_ws/        workspace ROS2 nativo sul PC (agent + foxglove bridge)
docs/           documentazione — parti da docs/README.md
hardware/       modelli 3D del frame
logs/           CSV di calibrazione sensori
old/            firmware Arduino originale (solo riferimento)
```

## Roadmap

| Fase | Obiettivo | Stato |
|------|-----------|-------|
| 0A | Sensori raw + telemetria su Foxglove | ✅ Completata |
| 0B | Motor driver + bring-up PCB custom | 🟡 In corso |
| 1 | Stabilizzazione attitudine (PID hover) | 📅 |
| 2 | Velocity hold con optical flow | 📅 |
| 3 | Position control a waypoint | 📅 |
| Swarm | Migrazione a ESP-NOW peer-to-peer | 📅 |

Stato aggiornato e blockers correnti: [`docs/STATO.md`](docs/STATO.md)

## Documentazione

[`docs/README.md`](docs/README.md) — indice completo con percorsi di lettura.

- [`docs/STATO.md`](docs/STATO.md) — dove siamo (l'unico file con stato volatile)
- [`docs/grounding/`](docs/grounding/) — come funziona il sistema, 8 doc in ordine di lettura
- [`docs/specs/`](docs/specs/) — decisioni di progetto, datate e immutabili
- [`docs/sessions/`](docs/sessions/) — cosa è successo in ogni giornata di lavoro
