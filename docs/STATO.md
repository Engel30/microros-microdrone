# STATO — microros-microdrone

**Aggiornato:** 2026-09-14 · **Ultima sessione:** [2026-05-09](sessions/2026-05-09-test-motori-brownout.md) · **Branch:** `main`

> Questo è l'unico documento del progetto che contiene informazione **volatile**.
> I doc in `grounding/` descrivono *come funziona*, quelli in `specs/` *cosa è stato deciso e quando*, quelli in `sessions/` *cosa è successo quel giorno*. Qui c'è *a che punto siamo*.
> Si aggiorna con `/chiudi-sessione`, non a mano.

---

## In una riga

Il drone vola-a-banco: telemetria completa su Foxglove e motori controllabili via micro-ROS, ma l'alimentazione da buck-boost va in brownout sopra il 20% di PWM. Prossimo passo: LiPo 1S e poi il PID di assetto (Fase 1).

## Dove siamo

**Il progetto è fermo dal 2026-05-09** (4 mesi). Nessun lavoro fuori dal repo in questo periodo. Lo stato qui sotto è quello di fine giornata del 9 maggio.

**Cosa funziona, con evidenza:**

| Cosa | Evidenza |
|---|---|
| Sensori raw (IMU, flow, ToF, battery) | 5 topic a frequenza nominale su Foxglove; `accel.z ≈ -9.74 m/s²` da fermo |
| micro-ROS su WiFi UDP → agent → Foxglove | 8 publisher, 2 subscriber; workspace nativo in `ros2_ws/` |
| Arm software (`/arm` → `/armed`) | Gate su `task_motors`, latenza max ~1ms, anti-replay sulla transizione |
| Watchdog motori 500ms | Edge detection loggata su `/drone_1/log` |
| Console di debug `/drone_1/log` | Log panel Foxglove: reset reason, eventi WiFi, arm, watchdog |
| Motori sotto PWM | 4 motori @10% girano puliti su PCB v1.0 |
| PCB custom v1.0 | Saldata e funzionante, con pull-down 10kΩ e switch arm |

**Cosa non funziona:** l'alimentazione. Sopra il 20% di PWM su due motori il drone si resetta.

## Blockers attivi

| # | Blocker | Impatto | Soluzione |
|---|---|---|---|
| 1 | **Brownout del buck-boost.** Transient response insufficiente (cap di output piccolo, induttore lento): al transitorio PWM dei coreless 8520 il rail dell'ESP32 collassa sotto la soglia di brownout. | Blocca qualsiasi test sopra il 20% PWM, quindi blocca il PID | LiPo 1S 300-600mAh 25C (eroga 7-15A di picco). Workaround sul buck: 1000µF + 100nF ceramico sull'uscita, vicino ai source dei MOSFET |
| 2 | **Rate di `cmd_motor_test`.** Il watchdog motori scatta a 500ms; il pannello Publish di Foxglove non ha un rate nativo. | I test da Foxglove si fermano da soli | `ros2 topic pub -r 10` da CLI |
| 3 | **Vibrazioni dei coreless sulla cella IMU.** Rumore su gyro/accel. | Degraderà il tuning del PID | Frame 2.0 che avvolge i motori — pipeline in [`grounding/08-FRAME-DESIGN-GENERATIVO.md`](grounding/08-FRAME-DESIGN-GENERATIVO.md) |

**Vincolo di sicurezza permanente:** eliche staccate fino a fine Fase 1.

## Prossimi 3 passi

1. **Alimentazione da LiPo 1S 25C 300–600 mAh (BT2.0).** Bypassa il buck-boost. Ri-test dei 4 motori sopra il 20% PWM per confermare che il brownout sparisce. Senza questo il punto 3 non è testabile.
2. **Frame 2.0**, stampato in modo da avvolgere i coreless 8520 e smorzare le vibrazioni verso l'IMU.
3. **Fase 1 — PID di assetto.** Componente `pid_controller` + `task_pid_attitude` @ 1 kHz su Core 1 (inner loop rate roll/pitch/yaw + outer loop angle). Topic `/drone_1/cmd_attitude` (`geometry_msgs/Quaternion`). Tuning in tethered, eliche staccate.

## Alla ripresa, da verificare fisicamente

Il progetto è stato fermo 4 mesi. Prima di ricollegare alimentazione:

- [ ] Switch arm motori **OFF**
- [ ] Eliche staccate
- [ ] Continuità delle pull-down 10kΩ su tutti e 4 i gate
- [ ] Stato della LiPo (le celle 1S si gonfiano o si scaricano sotto soglia se lasciate ferme)
- [ ] Il drone associa ancora alla rete WiFi configurata
- [ ] `idf.py build` pulito con la toolchain attuale

## Roadmap fasi

| Fase | Stato | Obiettivo | Criterio di successo |
|---|---|---|---|
| 0A | ✅ | Sensori raw + telemetria Foxglove | Tutti i topic visibili e coerenti |
| 0B | 🟡 | Motor driver + bring-up PCB | 4 motori controllabili senza reset — **bloccato dall'alimentazione** |
| 1 | 📅 | PID di assetto, hover stabile | Hover con la sola IMU |
| 2 | 📅 | Velocity hold con optical flow | Drone fermo in aria senza drift |
| 3 | 📅 | Position control a waypoint | Il drone raggiunge un target dal PC |
| Swarm | 📅 | `uros_interface` → `comm_protocol` ESP-NOW | 4 droni (Explorer/Relay/Rescue/Bridge) operativi insieme |

## Decisioni consolidate

Non ricopiate qui: stanno nelle spec.

- **micro-ROS sul singolo drone fino a fine Fase 3, ESP-NOW con lo swarm** — [`specs/2026-04-26`](specs/2026-04-26-stato-progetto-e-roadmap.md)
- **Architettura swarm, Approccio B (ESP-NOW + Bridge AP, 4 ruoli)** — [`specs/2026-04-14`](specs/2026-04-14-architettura-swarm-brainstorming.md), dettaglio in [`grounding/07-ARCHITETTURA-SWARM.md`](grounding/07-ARCHITETTURA-SWARM.md)
- **Pivot STM32+UWB scartato**, si resta su ESP32-S3 + optical flow — [`specs/2026-04-26`](specs/2026-04-26-stato-progetto-e-roadmap.md)
- **Architettura dual-core e code inter-task** — [`specs/2026-03-10`](specs/2026-03-10-swarm-drone-architecture-design.md), operativa in [`grounding/03-FIRMWARE-ARCHITETTURA.md`](grounding/03-FIRMWARE-ARCHITETTURA.md)
- **Pull-down 10kΩ obbligatorie sui gate** — lezione del [2026-03-19](sessions/2026-03-19-motor-driver-esp32-bruciato.md), recepita nella PCB v1.0

## Ultime 3 sessioni

- [2026-05-09 — Test motori: brownout buck-boost + console Foxglove](sessions/2026-05-09-test-motori-brownout.md)
- [2026-05-08 — micro-ROS tethered: publisher, subscriber, arm, diagnostica WiFi](sessions/2026-05-08-microros-tethered-arm-e-diagnostica.md)
- [2026-04-26 — Consolidamento documentazione](sessions/2026-04-26-consolidamento-documentazione.md)

[Tutte le sessioni →](sessions/README.md)
