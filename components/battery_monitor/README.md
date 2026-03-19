# battery_monitor

Monitoraggio tensione batteria via ADC e allarme buzzer.

## Stato: stub (da implementare in Fase 0A)

## API

- `battery_init()` — Configura ADC su D8 e PWM buzzer su D9
- `battery_read_voltage()` — Legge ADC, applica BATTERY_DIVIDER_RATIO (2.0), ritorna tensione in V

## Soglie

| Soglia | Tensione | Azione |
|--------|----------|--------|
| Low | 3.3V | Buzzer allarme |
| Critical | 3.0V | Atterraggio forzato |

## Hardware

Partitore resistivo 100k/100k: V_adc = V_bat / 2. ADC legge 0-3.3V → range batteria 0-6.6V (batteria 1S LiPo 3.0-4.2V).

## Dipendenze

`common`, `driver`, `esp_adc`
