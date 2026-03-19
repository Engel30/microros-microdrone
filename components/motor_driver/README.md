# motor_driver

Controllo PWM per 4 motori brushed via LEDC ESP-IDF.

## Stato: stub (da implementare in Fase 0B)

## API

- `motors_init()` — Configura 4 canali LEDC PWM sui pin D0-D3
- `motors_set(const motor_cmd_t *cmd)` — Imposta duty % per ogni motore (0-100%)
- `motors_stop()` — Porta tutti i motori a duty 0%

## Mappatura motori

| Pin | GPIO | Motore |
|-----|------|--------|
| D0 | GPIO_NUM_1 | Front-Left |
| D1 | GPIO_NUM_2 | Rear-Left |
| D2 | GPIO_NUM_3 | Rear-Right |
| D3 | GPIO_NUM_4 | Front-Right |

Hardware: MOSFET SI2302 low-side switch + diodo flyback 1N5819 per motore.

## Dipendenze

`common`, `driver` (ESP-IDF LEDC)
