# sensor_fusion

Pipeline di fusione sensori: IMU + optical flow → stato stimato (posizione, velocità, angoli).

## Stato: stub (da implementare in Fase 2+)

## API

- `fusion_init()` — Inizializza filtro di fusione
- `fusion_update(imu_data_t *imu, flow_data_t *flow, state_t *state)` — Aggiorna stima stato

## Pipeline prevista

1. Compensazione gyro
2. Compensazione tilt
3. Trasformazione body → world
4. Integrazione velocità/posizione

Riferimento logica: vecchio codice Arduino in `old/`

## Dipendenze

`common`
