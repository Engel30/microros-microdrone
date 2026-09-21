# STATO — microros-microdrone

**Aggiornato:** 2026-09-21 · **Ultima sessione:** [2026-09-21](sessions/2026-09-21-alimentazione-18650-e-ripresa.md) · **Branch:** `main`

> Questo è l'unico documento del progetto che contiene informazione **volatile**.
> I doc in `grounding/` descrivono *come funziona*, quelli in `specs/` *cosa è stato deciso e quando*, quelli in `sessions/` *cosa è successo quel giorno*. Qui c'è *a che punto siamo*.
> Si aggiorna con `/chiudi-sessione`, non a mano.

---

## In una riga

Il drone è di nuovo vivo dopo 4 mesi: pacco da banco 1S2P (Golisi G30) costruito, boot pulito, WiFi e micro-ROS connessi, Foxglove collegato. Prossimo passo: fusibile sul pacco e test motori sopra il 20% PWM per chiudere il brownout.

## Dove siamo

Il firmware è quello del 2026-05-09 (`5296be5`), ricompilato il 21/09 solo per cambiare l'IP dell'agent (`192.168.1.9` → `.7`). Nessuna modifica al codice.

**Cosa funziona, con evidenza:**

| Cosa | Evidenza |
|---|---|
| Sensori raw (IMU, flow, ToF, battery) | 5 topic a frequenza nominale su Foxglove (maggio) |
| micro-ROS su WiFi UDP → agent → Foxglove | `uros_init OK` e Foxglove connesso il 21/09, con IP prenotati (PC `.7`, drone `.15`) |
| Arm software, watchdog 500ms, console `/drone_1/log` | test di maggio |
| Motori sotto PWM | 4 motori @10% puliti su PCB v1.0 (maggio, buck-boost) |
| PCB custom v1.0 | saldata, pull-down 10kΩ e switch arm |
| **Pacco da banco 1S2P G30** | boot senza `BROWNOUT`, WiFi associato in 3 s, RSSI −48 (21/09). **Motori sul pacco non ancora provati** |
| Ambiente di build | `sdkconfig` ricostruito, `managed_components` rigenerati, build+flash OK (21/09) |

**Cosa non è verificato:** che il pacco regga i 4 motori sopra il 20% PWM. È la misura che decide se il blocker del brownout è chiuso.

## Blockers attivi

| # | Blocker | Impatto | Soluzione |
|---|---|---|---|
| 1 | **Brownout sopra il 20% PWM** — diagnosticato a maggio sul buck-boost. Sostituito dal pacco 1S2P G30 ([spec](specs/2026-09-21-alimentazione-banco-18650.md)), ma il test motori sul pacco non è stato fatto. | Blocca il PID finché non è misurato | Test di accettazione [`docs/grounding/05-BRINGUP-QUICKSTART.md` §5.5](grounding/05-BRINGUP-QUICKSTART.md): caduta su `TP_VBAT` < 0.3 V a 100% PWM, nessun `BROWNOUT` al boot dopo |
| 2 | **Fusibile mancante sul pacco.** I pad `B+`/`B-` sono a monte della protezione; in corto le G30 danno 80–120 A. | Sicurezza: regole transitorie (pacco collegato solo durante il test, `SW1` a portata, cavi ispezionati) | Lama mini 10 A + portafusibile volante sul `B+`, vicino alla scheda |
| 3 | **Rate di `cmd_motor_test`.** Il watchdog scatta a 500ms; il pannello Publish di Foxglove non ha rate. | I test da Foxglove si fermano da soli | `ros2 topic pub -r 10` da CLI |
| 4 | **Vibrazioni dei coreless sulla cella IMU.** | Degraderà il tuning del PID | Frame 2.0 — [`grounding/08-FRAME-DESIGN-GENERATIVO.md`](grounding/08-FRAME-DESIGN-GENERATIVO.md) |

**Vincoli di sicurezza permanenti:** eliche staccate fino a fine Fase 1. **Mai USB-C e pacco batteria insieme** (`VUSB` della XIAO è il VBUS senza diodo: i 5 V del PC finirebbero nelle celle).

## Debiti aperti

Non bloccano il volo, ma mordono se ignorati.

- **Layout Foxglove da rifare**: `drone_1-tethered.json` in `foxglove-layouts/` non esiste. Tabella in [`docs/grounding/05-BRINGUP-QUICKSTART.md` §4.2](grounding/05-BRINGUP-QUICKSTART.md).
- **`sdkconfig` è gitignored** e il 14/09 era sparito insieme a `build/` e ai componenti esp-idf-lib. Tenerne una copia fuori dal repo; recupero da `sdkconfig.old` documentato in [`docs/grounding/04-SETUP-AMBIENTE.md` §6](grounding/04-SETUP-AMBIENTE.md).
- **30Q superstite** (3.1 V): prova delle 24 h prima di considerarla riserva. L'altra 30Q (era a 0 V, si è scaldata in carica) va smaltita.
- **Batteria di volo** LiPo 1S 300–600 mAh 25C ancora da procurare: il pacco 18650 pesa ~110 g e non vola.
- Codici LCSC di AO3400A e SS14 `DA VERIFICARE` in `pcb-custom/` — prima di qualsiasi riordino della PCB.
- `feature/microros-tethered` è dentro `main`, da cancellare.
- [`specs/2026-04-26`](specs/2026-04-26-stato-progetto-e-roadmap.md) senza header "STORICO".

## Prossimi 3 passi

1. **Fusibile mini 10 A sul `B+` del pacco**, poi **test di accettazione** con i 4 motori a 50% e 100% PWM, eliche staccate, multimetro su `TP_VBAT`. Se passa: blocker #1 e #2 chiusi, Fase 0B completa. **Senza questo il passo 3 non è testabile.**
2. **Layout Foxglove** salvato in `foxglove-layouts/` (5 minuti, tabella in quickstart §4.2).
3. **Fase 1 — PID di assetto.** Componente `pid_controller` + `task_pid_attitude` @ 1 kHz su Core 1 (inner loop rate roll/pitch/yaw + outer loop angle). Topic `/drone_1/cmd_attitude`. Tuning in tethered con il cavo da 45 cm che arriva dall'alto.

## Alla ripresa, da verificare fisicamente

- [ ] Switch arm motori **OFF**, eliche staccate
- [ ] Pacco **scollegato** dal BT2.0 prima di attaccare la USB-C
- [ ] Tensione delle G30 (devono stare entro 0.05 V tra loro; sotto 3.0 V ricaricare prima)
- [ ] `hostname -I` in WSL dà `192.168.1.7`, altrimenti il firmware non trova l'agent
- [ ] `sdkconfig` presente con `grep DRONE_ sdkconfig` coerente (SSID `"WiFi LiboHouse"`, agent `192.168.1.7`)

## Roadmap fasi

| Fase | Stato | Obiettivo | Criterio di successo |
|---|---|---|---|
| 0A | ✅ | Sensori raw + telemetria Foxglove | Tutti i topic visibili e coerenti |
| 0B | 🟡 | Motor driver + bring-up PCB | 4 motori controllabili senza reset — **alimentazione sostituita, test da fare** |
| 1 | 📅 | PID di assetto, hover stabile | Hover con la sola IMU |
| 2 | 📅 | Velocity hold con optical flow | Drone fermo in aria senza drift |
| 3 | 📅 | Position control a waypoint | Il drone raggiunge un target dal PC |
| Swarm | 📅 | `uros_interface` → `comm_protocol` ESP-NOW | 4 droni (Explorer/Relay/Rescue/Bridge) operativi insieme |

## Decisioni consolidate

Non ricopiate qui: stanno nelle spec.

- **Alimentazione da banco 1S2P Golisi G30 su scheda UPS, fusibile obbligatorio, cavo 20 AWG 45 cm** — [`specs/2026-09-21`](specs/2026-09-21-alimentazione-banco-18650.md)
- **IP di PC e drone prenotati sul router** (l'IP dell'agent è compilato nel firmware) — [`grounding/04-SETUP-AMBIENTE.md` §6](grounding/04-SETUP-AMBIENTE.md)
- **micro-ROS sul singolo drone fino a fine Fase 3, ESP-NOW con lo swarm** — [`specs/2026-04-26`](specs/2026-04-26-stato-progetto-e-roadmap.md)
- **Architettura swarm, Approccio B (ESP-NOW + Bridge AP, 4 ruoli)** — [`specs/2026-04-14`](specs/2026-04-14-architettura-swarm-brainstorming.md), dettaglio in [`grounding/07-ARCHITETTURA-SWARM.md`](grounding/07-ARCHITETTURA-SWARM.md)
- **Pivot STM32+UWB scartato**, si resta su ESP32-S3 + optical flow — [`specs/2026-04-26`](specs/2026-04-26-stato-progetto-e-roadmap.md)
- **Architettura dual-core e code inter-task** — [`specs/2026-03-10`](specs/2026-03-10-swarm-drone-architecture-design.md), operativa in [`grounding/03-FIRMWARE-ARCHITETTURA.md`](grounding/03-FIRMWARE-ARCHITETTURA.md)
- **Pull-down 10kΩ obbligatorie sui gate** — lezione del [2026-03-19](sessions/2026-03-19-motor-driver-esp32-bruciato.md), recepita nella PCB v1.0
- **Stato volatile solo in questo file, chiusura sessione via comando** — [`specs/2026-09-14`](specs/2026-09-14-riorganizzazione-documentale.md)

## Ultime 3 sessioni

- [2026-09-21 — Pacco da banco 18650 e ripresa del drone dopo 4 mesi](sessions/2026-09-21-alimentazione-18650-e-ripresa.md)
- [2026-09-14 — Riorganizzazione documentale e sistema di continuità sessioni](sessions/2026-09-14-riorganizzazione-documentale.md)
- [2026-05-09 — Test motori: brownout buck-boost + console Foxglove](sessions/2026-05-09-test-motori-brownout.md)

[Tutte le sessioni →](sessions/README.md)
