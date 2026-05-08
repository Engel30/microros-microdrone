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

## Watchdog (modalità uROS)

Quando il firmware gira in modalità `[1] uROS` (default), `task_motors` (1kHz) implementa due gate di sicurezza:

1. **Watchdog cmd 500ms:** se non riceve un nuovo `motor_cmd_t` da `cmd_queue` entro `MOTOR_CMD_TIMEOUT_MS` (vedi `drone_config.h`) i 4 duty vengono forzati a 0. Al boot `last_cmd_us=0` mantiene i motori fermi finché non arriva il primo comando.
2. **Arm gate (sticky):** flag atomico globale `g_armed` (in `drone_types.h`, scritto dalla callback `/drone_1/arm` in `uros_interface`). Quando `false`, i motori sono forzati a 0 a ogni ciclo (latenza max ~1ms) **anche se** ci sono cmd validi in coda. Boot: DISARMED.

Sulla transizione disarm→arm il task resetta `last_cmd_us=0`: il watchdog richiede un nuovo `cmd_motor_test` prima di far girare i motori (impedisce che cmd stantii pre-disarm riprendano automaticamente).

I comandi entrano via subscriber `/drone_1/cmd_motor_test` (`std_msgs/Float32MultiArray`, 4 valori 0-100%, mapping FL/RL/RR/FR). L'arm via `/drone_1/arm` (`std_msgs/Bool`, sticky). Vedi `docs/07-MICROROS-TETHERED.md` §4-5 e `docs/08-BRINGUP-QUICKSTART.md` §5 per la guida operativa (Foxglove Publish panel).

## Test

- **Modalità `[1] uROS`:** comandi via topic ROS2, watchdog 500ms attivo. Implementato (Step 7 del piano `2026-05-07-piano-implementativo-microros-tethered`), **non ancora testato sui motori reali** (al 2026-05-08 motori non saldati al PCB v1.0).
- **Modalità `[2] Motor test`:** menu USB Serial interattivo legacy, ogni motore singolarmente o tutti insieme. Validato in passato sul vecchio ESP32 (poi bruciato pre-PCB).

## Dipendenze

`common`, `esp_driver_ledc`
