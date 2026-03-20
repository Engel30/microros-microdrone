# motor_driver

Controllo PWM per 4 motori brushed via LEDC ESP-IDF.

## Stato: implementato (Fase 0B)

## Configurazione PWM

- **Frequenza:** 20 kHz (sopra soglia udibile)
- **Risoluzione:** 10 bit (1024 livelli, 0-1023)
- **Timer:** LEDC_TIMER_0, low-speed mode
- **Clock:** auto-select

## API

- `motors_init()` — Configura timer LEDC + 4 canali PWM sui pin D0-D3. Ritorna `ESP_OK` o errore.
- `motors_set(const motor_cmd_t *cmd)` — Imposta duty % per ogni motore (0.0-100.0%, clampato). Aggiorna tutti e 4 i canali.
- `motors_stop()` — Porta tutti i motori a duty 0%.

## Mappatura motori

| Indice | Pin | GPIO | Motore | Canale LEDC |
|--------|-----|------|--------|-------------|
| 0 | D0 | GPIO_NUM_1 | Front-Left | LEDC_CHANNEL_0 |
| 1 | D1 | GPIO_NUM_2 | Rear-Left | LEDC_CHANNEL_1 |
| 2 | D2 | GPIO_NUM_3 | Rear-Right | LEDC_CHANNEL_2 |
| 3 | D3 | GPIO_NUM_4 | Front-Right | LEDC_CHANNEL_3 |

## Hardware

- Motori: 8520 coreless brushed 3.7V 1S
- MOSFET: SI2302 low-side switch (Rds_on ~40mΩ, Vgs_th ~1.2V)
- Diodo flyback: 1N5819 per motore (catodo verso +5V, anodo verso drain)
- Drive: GPIO 3.3V → gate SI2302 (ampio margine sopra threshold)

### ⚠️ Protezioni OBBLIGATORIE

```
GPIO ──[100Ω]── Gate ──┬── MOSFET
                       │
                    [10kΩ]
                       │
                     Source/GND
```

- **Pull-down 10kΩ gate-source**: OBBLIGATORIA. Durante il boot (~300ms) i GPIO ESP32 sono flottanti. Senza pull-down i MOSFET si accendono casualmente, lo switching caotico genera spike induttivi che distruggono i diodi di clamp interni dell'ESP32 → cortocircuito 3.3V-GND permanente.
- **Resistenza 100Ω in serie GPIO-gate**: consigliata. Limita corrente di spike verso il GPIO.
- **Condensatore 100μF** sulla linea 5V motori: consigliato. Filtra rumore.
- Le pull-down interne dell'ESP32 NON bastano (si attivano solo dopo il boot del firmware).

## Test

All'avvio del firmware, selezionare modalità [2] Motor test dal menu.
Menu interattivo permette di comandare ogni motore singolarmente o tutti insieme.

## Dipendenze

`common`, `esp_driver_ledc`
