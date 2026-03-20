# Timeline Progetto Microdrone

Registro cronologico delle attività. Aggiornare ad ogni sessione di lavoro.

---

## 2026-03-10 — Architettura e Design Spec
- Scritto design spec completo (`docs/specs/2026-03-10-swarm-drone-architecture-design.md`)
- Definito architettura ESP-IDF + FreeRTOS + micro-ROS
- Pinout definitivo, task allocation dual-core, queue system
- Migrazione concettuale da Arduino a ESP-IDF completata

## 2026-03-11 → 2026-03-19 — Fase 0A: Sensori
- Implementato imu_driver (MPU6050 via I2C, calibrazione, output NED)
- Implementato flow_driver (parser CXOF, calibrazione FLOW_SCALE_RAD)
- Implementato battery_monitor (ADC, partitore)
- Implementato uros_interface (bridge micro-ROS ↔ FreeRTOS queues)
- Logging CSV su Foxglove funzionante
- **Fase 0A completata**

## 2026-03-19 — Fase 0B: Motor Driver
- Implementato motor_driver (LEDC PWM 20kHz, 10-bit, 4 canali)
- Test motori: **ESP32 bruciato**
- Causa: GPIO flottanti durante boot → MOSFET SI2302 accesi casualmente → spike induttivi → cortocircuito 3V3-GND permanente
- Mancavano pull-down 10kΩ esterne sui gate (le pull-down interne si attivano solo dopo il boot)
- **Fase 0B bloccata** — serve nuovo ESP32

## 2026-03-20 — PCB Custom: Design
- Analisi causa burning del regolatore 3.3V
- Decisione: passare da perfboard a PCB custom per risolvere ground loop e protezioni
- Design spec PCB completato (`docs/pcb-custom/pcb-design-spec.md`)
  - Carrier board 2-layer, componenti solo top, ground plane pieno bottom
  - 4× driver motori con pull-down 10kΩ + serie 100Ω (lezione dal burning)
  - Switch arm motori separato (protezione hardware)
  - JST-PH per motori e switch, BT2.0 femmina sulla PCB
  - Target: ≤45×35mm, spessore 1.0mm, peso PCB ≤3g
- Guida teoria PCB per principianti (`docs/pcb-custom/pcb-design-teoria.md`)
- Guida uso EasyEDA Std (`docs/pcb-custom/easyeda-guida-uso.md`)
- EDA scelto: EasyEDA Std → JLCPCB
- **Prossimo step:** disegnare schematic in EasyEDA, poi layout, poi ordinare
