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
- MOSFET: AO3400A (SOT-23) low-side switch, logic-level (Rds_on ~28mΩ @ Vgs=4.5V, Vgs(th) ~0.65-1.45V)
- Diodo flyback: SS14 (SMA, Schottky 40V 1A, Vf ~0.5V) per motore, catodo verso VBAT_MOTORS, anodo verso drain
- Drive: GPIO 3.3V → gate AO3400A (ampio margine sopra threshold)

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

## Spin-up (`motor_spinup`)

Un motore DC a PWM risponde a un gradino di duty con la costante di tempo elettrica (L/R ≈ 100 µs): la corrente salta subito a (D_nuovo − E)/R, dove la back-EMF E insegue il duty con la costante di tempo **meccanica**, lunga a basso duty senza eliche. Un gradino applicato a motori non ancora a regime somma corrente a corrente. Misure del 2026-09-21 sul pacco 18650 con `ros2_ws/tools/motor_steps.py` (celle a 3.65 V, rail 3V3 già in dropout):

| Gradino | Esito |
|---|---|
| 0 → 30 su 3 motori | `BROWNOUT` |
| 8 → 30 su 4 motori dopo **300 ms** all'8% | `BROWNOUT` |
| 8 → 30 su 4 motori dopo **2 s** all'8% | ok |
| 10 → 20 → 30 → 50 → 70 su 4, gradini di 3 s | ok |
| 4 motori al 70% a regime | ok |
| **con eliche**, 4 motori 15% o 3 motori 20%, spin-up attivo | `BROWNOUT` |

Regola, applicata in `task_motors` dopo watchdog e arm gate, **per ogni motore indipendentemente**:

> Da 0 si esce solo con una rampa lineare fino a `MOTOR_SPIN_MIN_PCT` (150 ms) e una sosta a quel duty abbastanza lunga da raggiungere il regime meccanico (2000 ms). Sopra `MOTOR_SPIN_MIN_PCT` il comando passa intatto.

Lo spin-up copre i transitori a vuoto, **non il regime con le eliche**: il rail 3.3 V della XIAO sta sul rail motori tramite un LDO in dropout e con carico reale non regge (opzioni in `docs/specs/2026-09-21-alimentazione-logica-xiao.md`). La soglia è stretta (Σ Δduty ≈ 80 passa, ≈ 90 no, a 3.65 V): in Fase 1 il PID lavorerà sopra `MOTOR_SPIN_MIN_PCT` con correzioni differenziali (somma ≈ 0), ma variazioni **collettive** rapide di throttle superiori a ~20 punti su 4 motori richiederanno un limitatore di gradino collettivo. Decisione rimandata a quando ci sarà il PID.

- `motor_spinup_init(s, spin_min_pct, ramp_ms, hold_ms)` — parametri da `drone_config.h` (`MOTOR_SPIN_MIN_PCT` 8%, `MOTOR_SPINUP_RAMP_MS`, `MOTOR_SPINUP_HOLD_MS`).
- `motor_spinup_apply(s, req, out, now_us)` — `out` = comando effettivo; ritorna la bitmask dei motori che iniziano lo spin-up (loggata su `/drone_1/log` a INFO).
- Comando che torna a 0 (watchdog, disarm, o richiesta) → il motore è fermo e la prossima uscita rifà lo spin-up da capo.
- Comando sotto `MOTOR_SPIN_MIN_PCT` (es. 3%) attraversa la rampa e poi passa intatto.
- L'echo `/drone_1/motors` riporta il duty **applicato**, non quello richiesto: dopo un `[30,30,30,30]` da fermo si vede la rampa, 2 s a 8%, poi 30.

Il vincolo sta qui e non nel decollo perché è una proprietà dell'attuatore: vale per `cmd_motor_test` oggi, per il PID di Fase 1 domani, per un failsafe dopodomani. Il PID lavora sopra `MOTOR_SPIN_MIN_PCT` e non vede alcun limitatore; la fase di spin-up del decollo è una conseguenza di questa regola, non un caso a parte. L'arm resta "motori a 0" (scelta del 2026-09-21: un drone armato non deve per forza far girare le eliche).

Il modulo è C puro senza dipendenze ESP-IDF, testato su host:

```bash
cd components/motor_driver && ./test/run.sh     # gcc, 5 casi
```

## Comandi

I comandi entrano via subscriber `/drone_1/cmd_motor_test` (`std_msgs/Float32MultiArray`, 4 valori 0-100%, mapping FL/RL/RR/FR). L'arm via `/drone_1/arm` (`std_msgs/Bool`, sticky). Vedi `docs/grounding/06-MICROROS-TETHERED.md` §4-5 e `docs/grounding/05-BRINGUP-QUICKSTART.md` §5 per la guida operativa (Foxglove Publish panel).

## Test

- **Modalità `[1] uROS`:** comandi via topic ROS2, watchdog 500ms attivo. Testato sui motori reali su PCB v1.0 (maggio 2026 a 10%, 2026-09-21 fino a 4 motori @ 70% sul pacco 18650). Spin-up: test su host in `test/`, verifica sul drone in `docs/STATO.md`.
- **Modalità `[2] Motor test`:** menu USB Serial interattivo legacy, ogni motore singolarmente o tutti insieme. Validato in passato sul vecchio ESP32 (poi bruciato pre-PCB).

## Dipendenze

`common`, `esp_driver_ledc` (`motor_spinup` solo `common`)
