# STATO — microros-microdrone

**Aggiornato:** 2026-09-21 (sera) · **Ultima sessione:** [2026-09-21 §2](sessions/2026-09-21-alimentazione-18650-e-ripresa.md) · **Branch:** `main`

> Questo è l'unico documento del progetto che contiene informazione **volatile**.
> I doc in `grounding/` descrivono *come funziona*, quelli in `specs/` *cosa è stato deciso e quando*, quelli in `sessions/` *cosa è successo quel giorno*. Qui c'è *a che punto siamo*.
> Si aggiorna con `/chiudi-sessione`, non a mano.

---

## In una riga

Senza eliche i 4 motori girano fino al 70% e lo spin-up in `task_motors` copre i transitori; **con le eliche il drone va in brownout già a 15%** perché il rail 3.3 V della XIAO sta sul rail motori con l'LDO in dropout. Prossimo passo: decidere e montare il boost su `VUSB` ([spec opzioni](specs/2026-09-21-alimentazione-logica-xiao.md)).

## Dove siamo

Firmware modificato oggi: spin-up per motore in `task_motors` (rampa 150 ms fino all'8% + sosta 2 s, poi comando intatto), flashato e verificato senza eliche. Primo test automatico del firmware (`components/motor_driver/test/run.sh`, gcc su host). Script `ros2_ws/tools/motor_steps.py` per sequenze di duty senza buchi di watchdog.

**Cosa funziona, con evidenza:**

| Cosa | Evidenza |
|---|---|
| Sensori raw (IMU, flow, ToF, battery) | 5 topic a frequenza nominale su Foxglove (maggio) |
| micro-ROS su WiFi UDP → agent → Foxglove | `uros_init OK`, Foxglove connesso, IP prenotati (PC `.7`, drone `.15`) |
| Arm software, watchdog 500ms, console `/drone_1/log` | test di maggio; log `BROWNOUT` e `spin-up mask` usati oggi |
| Motori a regime **senza eliche** | rampa 10→20→30→50→70 su 4 motori, gradini di 3 s, nessun reset (21/09) |
| Partenza da fermo con spin-up, senza eliche | `"30,30,30,30:3"` da fermo ok con `MOTOR_SPINUP_HOLD_MS` 2000 (resettava con 300) |
| Tutti e 4 i motori girano, anche con elica all'8% | test con eliche del 21/09 sera |
| Pacco da banco 1S2P G30 | boot pulito; le celle erogano, il problema è a valle |
| PCB custom v1.0 | saldata, pull-down 10kΩ e switch arm |
| Ambiente di build | build+flash OK (21/09) |

**Cosa non funziona:** con le eliche, 3 motori a 20% o 4 a 15% → `BROWNOUT` (celle a 3.6 V, 3V3 a 3.26 V a riposo, 0.75 V di margine totale). In hover servono 4–6 A: irraggiungibile finché `VUSB` sta su `VBAT_SW`.

**Cosa non è verificato:** la stessa prova con celle a 4.2 V (misura del margine); il 3V3 sotto carico con eliche; `MOTOR_SPINUP_HOLD_MS` sotto i 2 s.

## Blockers attivi

| # | Blocker | Impatto | Soluzione |
|---|---|---|---|
| 1 | **Rail 3.3 V della XIAO sul rail motori.** L'LDO interno è in dropout con celle < 3.9 V; con carico reale (eliche) il rail buca i 2.51 V già a 15% su 4 motori. Vale anche per la LiPo di volo. | **Blocca il volo e la Fase 1.** | Opzione B della [spec](specs/2026-09-21-alimentazione-logica-xiao.md): boost 1S → 5 V sul pin `VUSB` + 1000 µF, con isolamento di `VUSB` dalla net `VBAT_SW` (da verificare sul layout). Nuovo criterio di accettazione: 3V3 ≥ 3.0 V a 4 motori 100% con eliche |
| 2 | **Fusibile mancante sul pacco.** I pad `B+`/`B-` sono a monte della protezione; in corto le G30 danno 80–120 A. | Sicurezza: regole transitorie (pacco collegato solo durante il test, `SW1` a portata, cavi ispezionati) | Lama mini 10 A + portafusibile a bassa resistenza sul `B+`, nella stessa sessione del boost |
| 3 | **Vibrazioni dei coreless sulla cella IMU.** | Degraderà il tuning del PID | Frame 2.0 — [`grounding/08-FRAME-DESIGN-GENERATIVO.md`](grounding/08-FRAME-DESIGN-GENERATIVO.md) |

Risolto oggi: *rate di `cmd_motor_test` da Foxglove* → `motor_steps.py`; *brownout da transitorio senza eliche* → spin-up. Declassato a debito: la soglia Σ Δduty ≈ 80–90 del PID collettivo sparisce come vincolo una volta disaccoppiato il rail logico.

**Vincoli di sicurezza permanenti:** eliche montate solo con drone vincolato al banco, fino a fine Fase 1. **Mai USB-C e pacco batteria insieme** (`VUSB` della XIAO è il VBUS senza diodo; con il boost saranno due sorgenti a 5 V). `motor_steps.py` pubblica appena trova un subscriber: lanciarlo solo con drone pronto e mano su `SW1`.

## Debiti aperti

Non bloccano il volo, ma mordono se ignorati.

- **Celle mai ricaricate** dal 21/09 (3.6 V): ricaricare a 4.2 V prima del prossimo test, e misurare.
- **"Motore che non gira"** segnalato a metà sessione, al test dopo giravano tutti: non isolato, da tenere d'occhio.
- **`MOTOR_SPINUP_HOLD_MS` 2000 non ottimizzato**: con il boost montato provare 1 s / 0.5 s con `"8,8,8,8:1" "30,30,30,30:2"`.
- **Limitatore di gradino collettivo** per il PID: probabilmente non serve più con il boost; da riverificare con le eliche dopo la modifica.
- La modalità `[2] Motor test` (menu seriale) non passa dallo spin-up: solo `task_motors` lo applica.
- **Layout Foxglove da rifare**: `drone_1-tethered.json` in `foxglove-layouts/` non esiste. Tabella in [`grounding/05-BRINGUP-QUICKSTART.md` §4.2](grounding/05-BRINGUP-QUICKSTART.md).
- **`sdkconfig` è gitignored**: tenerne una copia fuori dal repo; recupero in [`grounding/04-SETUP-AMBIENTE.md` §6](grounding/04-SETUP-AMBIENTE.md).
- **30Q superstite** (3.1 V): prova delle 24 h prima di considerarla riserva. L'altra 30Q va smaltita.
- **Batteria di volo** LiPo 1S 300–600 mAh 25C ancora da procurare.
- Codici LCSC di AO3400A e SS14 `DA VERIFICARE` in `pcb-custom/`.
- `feature/microros-tethered` è dentro `main`, da cancellare.
- [`specs/2026-04-26`](specs/2026-04-26-stato-progetto-e-roadmap.md) senza header "STORICO".

## Prossimi 3 passi

1. **Celle a 4.2 V, 4 motori a 15% con eliche, multimetro sul 3V3** (5 minuti): conferma della diagnosi e misura di quanto si è corti.
2. **Decidere l'opzione della [spec alimentazione logica](specs/2026-09-21-alimentazione-logica-xiao.md)** (raccomandata: boost su `VUSB`), procurare boost + 1000 µF, verificare sul layout PCB v1.0 come isolare il pin `VUSB`, montare, misurare 3V3 sotto carico fino a 4 motori 70% con eliche. Fusibile nella stessa sessione. **Senza questo il passo 3 non è testabile.**
3. **Fase 1 — PID di assetto.** Componente `pid_controller` + `task_pid_attitude` @ 1 kHz su Core 1. Lo spin-up esistente diventa la fase di decollo (arm = motori fermi, per scelta).

## Alla ripresa, da verificare fisicamente

- [ ] Switch arm motori **OFF**; con le eliche montate il drone deve essere vincolato al banco
- [ ] Pacco **scollegato** dal BT2.0 prima di attaccare la USB-C
- [ ] Tensione delle G30 sul metallo (entro 0.05 V tra loro; **ricaricare a 4.2 V**: il margine di brownout dipende dalla carica)
- [ ] `hostname -I` in WSL dà `192.168.1.7`, altrimenti il firmware non trova l'agent
- [ ] `sdkconfig` presente con `grep DRONE_ sdkconfig` coerente (SSID `"WiFi LiboHouse"`, agent `192.168.1.7`)
- [ ] `cd components/motor_driver && ./test/run.sh` verde prima di toccare lo spin-up

## Roadmap fasi

| Fase | Stato | Obiettivo | Criterio di successo |
|---|---|---|---|
| 0A | ✅ | Sensori raw + telemetria Foxglove | Tutti i topic visibili e coerenti |
| 0B | 🟡 | Motor driver + bring-up PCB | 4 motori controllabili senza reset — **ok senza eliche; con eliche serve il rail logico disaccoppiato (blocker #1)** |
| 1 | 📅 | PID di assetto, hover stabile | Hover con la sola IMU |
| 2 | 📅 | Velocity hold con optical flow | Drone fermo in aria senza drift |
| 3 | 📅 | Position control a waypoint | Il drone raggiunge un target dal PC |
| Swarm | 📅 | `uros_interface` → `comm_protocol` ESP-NOW | 4 droni (Explorer/Relay/Rescue/Bridge) operativi insieme |

## Decisioni consolidate

Non ricopiate qui: stanno nelle spec.

- **Spin-up nel layer motori, arm = motori fermi, decollo come conseguenza dello spin-up** — [sessione 2026-09-21 §2](sessions/2026-09-21-alimentazione-18650-e-ripresa.md), dettaglio in `components/motor_driver/README.md`
- **Alimentazione logica: opzioni documentate, decisione aperta** (raccomandata boost su `VUSB`) — [`specs/2026-09-21-alimentazione-logica-xiao.md`](specs/2026-09-21-alimentazione-logica-xiao.md)
- **Alimentazione da banco 1S2P Golisi G30 su scheda UPS, fusibile obbligatorio, cavo 20 AWG 45 cm** — [`specs/2026-09-21`](specs/2026-09-21-alimentazione-banco-18650.md); criterio di accettazione §6 superato dalla spec sopra
- **IP di PC e drone prenotati sul router** (l'IP dell'agent è compilato nel firmware) — [`grounding/04-SETUP-AMBIENTE.md` §6](grounding/04-SETUP-AMBIENTE.md)
- **micro-ROS sul singolo drone fino a fine Fase 3, ESP-NOW con lo swarm** — [`specs/2026-04-26`](specs/2026-04-26-stato-progetto-e-roadmap.md)
- **Architettura swarm, Approccio B (ESP-NOW + Bridge AP, 4 ruoli)** — [`specs/2026-04-14`](specs/2026-04-14-architettura-swarm-brainstorming.md), dettaglio in [`grounding/07-ARCHITETTURA-SWARM.md`](grounding/07-ARCHITETTURA-SWARM.md)
- **Pivot STM32+UWB scartato**, si resta su ESP32-S3 + optical flow — [`specs/2026-04-26`](specs/2026-04-26-stato-progetto-e-roadmap.md)
- **Architettura dual-core e code inter-task** — [`specs/2026-03-10`](specs/2026-03-10-swarm-drone-architecture-design.md), operativa in [`grounding/03-FIRMWARE-ARCHITETTURA.md`](grounding/03-FIRMWARE-ARCHITETTURA.md)
- **Pull-down 10kΩ obbligatorie sui gate** — lezione del [2026-03-19](sessions/2026-03-19-motor-driver-esp32-bruciato.md), recepita nella PCB v1.0
- **Stato volatile solo in questo file, chiusura sessione via comando** — [`specs/2026-09-14`](specs/2026-09-14-riorganizzazione-documentale.md)

## Ultime 3 sessioni

- [2026-09-21 — Pacco da banco 18650 e ripresa del drone; §2 spin-up, poi con le eliche il rail 3.3 V non regge](sessions/2026-09-21-alimentazione-18650-e-ripresa.md)
- [2026-09-14 — Riorganizzazione documentale e sistema di continuità sessioni](sessions/2026-09-14-riorganizzazione-documentale.md)
- [2026-05-09 — Test motori: brownout buck-boost + console Foxglove](sessions/2026-05-09-test-motori-brownout.md)

[Tutte le sessioni →](sessions/README.md)
