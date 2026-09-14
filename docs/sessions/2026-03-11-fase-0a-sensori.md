# 2026-03-11 → 2026-03-19 — Fase 0A: sensori

**Commit:** `2d33e33` (protoboard), `c3107c1` (calibrazione imu e flow, aggiunto logging), `b0222b6`, `1979feb`

Periodo di lavoro, non singola sessione.

## Fatto

- `imu_driver`: MPU6050 via I2C, calibrazione, output in body frame NED.
- `flow_driver`: parser CXOF, calibrazione `FLOW_SCALE_RAD`.
- `battery_monitor`: ADC con partitore.
- `uros_interface`: bridge micro-ROS ↔ code FreeRTOS.
- Logging CSV su Foxglove funzionante.

## Scoperto

La calibrazione dell'optical flow ha richiesto un fattore di scala `1.294e-2 rad/count`, **7.35× il valore ArduPilot originale**: il clone P3901 ha uno scaler interno diverso dal PMW3901 genuino. Errore medio ~8% su test a 10cm da 10-11cm di altezza (range 86-104%).

## Esito

**Fase 0A completata.**
