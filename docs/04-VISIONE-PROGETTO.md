# Progetto: Visione e Roadmap

**Obiettivo:** Piattaforma drone educativa low-cost (<30€) per studio swarm intelligence, sistemi distribuiti, mesh networking.

---

## 1. Componenti e Strategie

**Hardware:**
- MCU: Seeed XIAO ESP32-S3 (dual-core 240MHz, WiFi)
- IMU: MPU-6050 (I2C)
- Sensori: Optical flow PMW3901 + ToF VL53L0X (UART)
- Propulsione: 8520 coreless 3.7V 1S (4 motori)
- Telaio: 3D print PLA custom

**Software:**
- Framework: ESP-IDF + FreeRTOS + micro-ROS
- Core 0 (comms), Core 1 (flight control)
- Scalabilità: stesso firmware, DRONE_ID diverso per sciame

**Costruzione:**
- Iterazione 1: Perfboard + moduli breakout (rapido, prototipazione)
- Iterazione 2: PCB custom 2-layer 45×35mm (pull-down integrati, riproducibile)

---

## 2. Incident e Lezioni

**2026-03-19 — ESP32 Bruciato (Fase 0B)**

**Causa:** GPIO flottanti durante boot → MOSFET accesi casualmente → spike dreno → cortocircuito 3V3-GND

**Fix:** Pull-down 10kΩ hardwired su ogni gate MOSFET (D0-D3). Garantisce GPIO=0 durante POR.

---

## 3. Tethering: Soluzione per Development

Batteria 450mAh @ 1S = autonomia fisica ~7-10 min. Per tuning firmware senza cambiare batteria ogni 5 min:

**Setup tethering:**
- Alimentatore da banco: 4.0V / 3A
- Cavo lungo (3-5m) sottile (30AWG)
- Drone alimentato via tethering durante test indoor

**Vantaggi:** volo infinito, tuning senza interruzioni, safe (drone non scappa)

---

## 4. Timeline Cronologica

| Data | Fase | Evento | Status |
|:-----|:-----|:-------|:------:|
| 2026-01-28 | Concept | Studio fattibilità | ✅ |
| 2026-03-10 | Design | Spec architettura ESP-IDF | ✅ |
| 2026-03-11→19 | 0A | Sensori (IMU, flow, battery) | ✅ |
| 2026-03-19 | 0B | Motor test → ESP32 bruciato | ⚠️ |
| 2026-03-20 | PCB | Design PCB custom EasyEDA | ✅ |
| 2026-04-14 | Swarm | Brainstorming architettura swarm | ✅ |
| 2026-04-16 | Doc | Riorganizzazione documentazione | ✅ |
| 2026-04-20 | PCB | EasyEDA layout completo | 🔄 |
| 2026-05-05 | PCB | Ordine JLCPCB | 📅 |
| 2026-05-20 | HW | Montaggio PCB, test motori | 📅 |
| 2026-06-01 | 1 | PID attitudine, hover | 📅 |
| 2026-07-01 | 2 | Velocity hold flow | 📅 |
| 2026-08-01 | 3 | Position control | 📅 |
| 2026-08-15 | Swarm | Architettura ESP-NOW | 📅 |
| 2026-09-15 | Val | Test flotta 4 droni | 📅 |
| 2026-10-01 | Tesi | Submission | 📅 |

---

## 5. Roadmap Fasi

**Fase 0A (✅ Completata):** Sensori raw, logging CSV Foxglove
- task_imu, task_flow, task_battery, task_microros

**Fase 0B (⏸️ Bloccata):** Motor driver LEDC
- Fix: nuovo ESP32 + PCB custom con pull-down

**Fase 1:** Stabilizzazione attitudine
- task_pid_attitude, tuning PID empirico

**Fase 2:** Velocity hold optical flow
- task_fusion (sensor fusion), task_pid_velocity

**Fase 3:** Position control
- task_pid_position, waypoint navigation

---

## 6. Vincoli Progettuali

| Vincolo | Target | Motivo |
|:--------|:------:|:-------|
| Costo unità | <30€ | Flotta 4-8 droni |
| Peso | <65g | TWR > 2.5:1 |
| Autonomia | 7-10 min | Limite fisico brushed |
| Sviluppo | <12 mesi | Tesi magistrale |

---

## 7. Prossimi Step

1. **PCB:** Completare layout EasyEDA, ordine JLCPCB
2. **Hardware:** Montaggio PCB, test motori
3. **Fase 1:** PID tuning, hover stabile
4. **Swarm:** Design protocollo ESP-NOW
5. **Tesi:** Documentazione + demo
