# common

Tipi e configurazioni condivisi da tutti i componenti del progetto.

## File

- **`drone_config.h`** — Definizioni pin GPIO, indirizzi I2C, frequenze task, priorità FreeRTOS, profondità code. Punto unico di configurazione hardware.
- **`drone_types.h`** — Struct condivise per comunicazione inter-task via FreeRTOS queues:
  - `imu_data_t` — accelerometro (g) + giroscopio (deg/s) + timestamp
  - `flow_data_t` — delta pixel flow + distanza ToF (mm) + qualità + timestamp
  - `state_t` — stato fuso: posizione, velocità, angoli (Fase 2+)
  - `motor_cmd_t` — duty % per 4 motori + timestamp

## Dipendenze

Nessuna. Questo componente è importato da tutti gli altri.
