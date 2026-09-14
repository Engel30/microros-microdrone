# Timeline progetto microros-microdrone

Cronologia delle attività significative. Aggiornata a fine sessione.

---

## 2026-05-09 — Test motori: brownout buck-boost + console di debug Foxglove

**Branch:** `feature/microros-tethered`

**Test motori (su PCB v1.0, alimentazione bench 5V 10A → buck-boost AliExpress → 4.1V):**
- 4 motori @ 10% PWM: gira pulito.
- 2 motori @ 20% PWM: drone si resetta. Diagnosi: il buck-boost ha transient response insufficiente (cap di output piccolo, induttore lento) → al transitorio PWM dei coreless 8520 il rail dell'ESP32 collassa sotto la soglia brownout. Pubblicazione del topic `cmd_motor_test` da Foxglove richiede inoltre rate ≥ 5 Hz per non far scattare il watchdog motori 500ms (Foxglove Publish panel non ha rate nativo → workaround `ros2 topic pub -r 10`).
- Soluzione fisica raccomandata: alimentare da LiPo 1S 300-600 mAh 25C (eroga 7-15A picco senza fiatare). Workaround temporaneo se si resta sul buck: cap elettrolitico 1000 µF + 100 nF ceramico sull'uscita del buck, vicino ai source dei MOSFET.

**Console di debug Foxglove `/drone_1/log` (rcl_interfaces/Log) implementata:**
- Nuovo publisher BEST_EFFORT `/drone_1/log` (8/12 slot ora occupati). Foxglove Log panel lo riconosce nativamente con filtri per livello (DEBUG/INFO/WARN/ERROR/FATAL), name, timeline scrubbabile.
- API `uros_log(level, fmt, ...)` printf-style, non-blocking, drop-on-full. Queue interna 16 item × 192 byte (~3KB RAM). Sicura prima di `uros_init` (no-op). Non da ISR.
- Drain in `task_microros` cap 5 msg/ciclo (10ms) → 500/s effettivi. Buffer messaggio pre-allocato (no malloc per publish).
- Hook implementati: **reset reason al boot** (POWERON/BROWNOUT/PANIC/WDT/EXT/SW), WiFi connect/disconnect+reason, transizioni arm/disarm, edge watchdog motori (running↔expired in stato armed), cmd_motor_test malformato.
- `app-colcon.meta`: `RMW_UXRCE_MAX_PUBLISHERS` 8→12 per non saturare a 8/8.
- Build pulita, 14% flash free.

**Verifica eseguita a fine sessione:**
- `/drone_1/log` visibile in Foxglove Log panel: boot reset reason, eventi WiFi, transizioni arm, edge watchdog motori funzionano tutti come atteso.
- Motori girano correttamente al duty pubblicato finché si resta sotto la soglia di brownout del buck-boost.

**Prossima sessione (Fase 0B → Fase 1):**
1. **Alimentazione**: procurare una LiPo 1S 25C 300–600 mAh (BT2.0) per alimentare i motori con la corrente nominale di una cella. Bypassa il buck-boost AliExpress, eliminando i brownout sui transitori PWM.
2. **Frame 2.0**: stampare un nuovo frame che avvolga i motori coreless 8520, smorzandone le vibrazioni meccaniche trasmesse alla cella IMU (riduce il rumore di gyro/accel → PID più stabile). Vedi `docs/06-FRAME-DESIGN-GENERATIVO.md` per la pipeline Fusion 360 Generative Design.
3. **Fase 1 — PID attitude**: implementazione del controllore di assetto (roll/pitch/yaw rate + outer loop angle), tuning sul drone in tethered. Topic `/drone_1/cmd_attitude` (`geometry_msgs/Quaternion`) + nuovo task `task_pid_attitude` @ 1 kHz Core 1.

**Modifiche file:**
- `app-colcon.meta`: pub limit 12 (richiede rebuild di `libmicroros.a` se si cambia di nuovo).
- `components/uros_interface/include/uros_interface.h`: API `uros_log()`, enum `uros_log_level_t`.
- `components/uros_interface/uros_interface.c`: queue + publisher + helper + drain + hook WiFi/arm/cmd.
- `main/task_motors.c`: edge detection watchdog + log.
- `components/uros_interface/README.md`: tabella topic aggiornata + sezione "Console di debug".

---

## 2026-05-08 — WiFi flapping = brownout su USB del PC

**Branch:** `feature/microros-tethered`

**Sintomo:** drone non si associava più al router di casa (`WiFi LiboHouse`, WPA2-PSK[ES]+WPA3-Personal con band steering unique SSID). Funzionava su hotspot del telefono. Pattern dei retry **inconsistente**: stessa configurazione produceva di volta in volta `reason=201` (NO_AP_FOUND), `reason=2` (AUTH_LEAVE dopo ~1 s di auth), `reason=205` (CONNECTION_FAIL), oppure auth-OK seguita da assoc-fail dopo 1 s. Niente di deterministico.

**Diagnostica firmware (tutta a vuoto):**
- Threshold `WIFI_AUTH_WPA2_PSK` forzato → peggiora (deauth reason 2 in 1 s).
- `pmf_cfg.required = true` → AP rifiuta pre-auth (`AP not PMF Capable when STA requires`, reason 210). Vale solo con WPA3 attivo.
- Test con WPA3 disabilitato sul router → stesso flapping.
- Logging del reason code aggiunto (`wifi_event_sta_disconnected_t.reason`) e finestra retry estesa da 10 a 30 per catturare i fallimenti di assoc che cadevano nel buco di logging.

**Causa reale:** **alimentazione**. Sull'USB del PC (laptop, WSL2 attivo) i burst TX WiFi (~300-500 mA istantanei) provocano brownout sul PA dell'ESP32-S3 → frame management persi in modo random → ogni tentativo fallisce in una fase diversa. Su powerbank o batteria singola il problema sparisce e l'associazione è immediata.

**Fix:** alimentare il drone da powerbank/batteria durante lo sviluppo, NON dalla USB del laptop quando c'è da fare WiFi attivo. La USB del laptop resta utile per flash + monitor seriale (basso assorbimento).

**Modifiche firmware lasciate (utili anche in futuro):**
- `components/uros_interface/uros_interface.c`: log del `reason` su `STA_DISCONNECTED`, finestra retry 1-30 + multipli di 10 (era 1-10). Threshold `WIFI_AUTH_OPEN` e `pmf_cfg.required=false` come da origine.

**Verifica:** OK su powerbank, drone associa subito a `WiFi LiboHouse` su canale 11.

---

## 2026-05-08 — WiFi retry infinito (no reboot loop)

**Branch:** `feature/microros-tethered`

**Sintomo osservato:** drone si associava brevemente all'AP (visibile dalla pagina admin del router) ma veniva disconnesso prima di completare il DHCP. Il contatore `s_retry_num` arrivava a 10 in pochi secondi → `ESP_ERROR_CHECK(uros_wifi_connect())` aborta → reboot loop sterile.

**Fix:**
- Rimosso `WIFI_MAX_RETRY` e `WIFI_TIMEOUT_MS`. Retry infinito allineato alla policy del ping-agent (Step 5).
- Aggiunto handler `WIFI_EVENT_STA_CONNECTED` per resettare `s_retry_num` su associazione, non solo su `GOT_IP`. Questo evita di consumare retry quando l'AP accetta l'auth ma kicka durante DHCP (caso transitorio frequente).
- `uros_wifi_connect()` blocca su `xEventGroupWaitBits(... portMAX_DELAY)` invece di abortire al timeout.
- Log: ogni retry per i primi 10, poi ogni 10 (riduce spam).

**Verifica:** build pulito. Test sul drone reale richiesto (l'utente deve riflashare).

---

## 2026-05-08 — Publisher temperatura on-die

**Branch:** `feature/microros-tethered`

**Attività svolta:**
- Aggiunto publisher BEST_EFFORT `/drone_1/temp` (`sensor_msgs/Temperature`, 1 Hz, frame `esp32_die`, variance 4.0).
- `uros_interface`: init `temperature_sensor` ESP-IDF (range 10–80°C, accuracy ±2°C nominale), gate 1 Hz dentro `task_microros` via `esp_timer_get_time`. Componente `esp_driver_tsens` aggiunto a `REQUIRES`.
- Limiti micro-ROS: 7/8 pub, 2/4 sub. Build pulito, 14% flash libero.
- Docs aggiornati (07 §3.1 → 7 publisher; 08 layout pannelli; uros_interface README).

**Verifica:** non eseguita (richiede flash + monitor).

---

## 2026-05-08 — Arm software (sub /arm + pub /armed) + gate motori

**Branch:** `feature/microros-tethered`

**Attività svolta:**
- **Flag globale `atomic_bool g_armed`** dichiarato in `components/common/include/drone_types.h`, definito in `main/main.c` (boot DISARMED). Atomic per accesso cross-core safe (callback Core 0 ↔ task_motors Core 1 @ 1kHz).
- **uros_interface**:
  - Subscriber RELIABLE `/drone_1/arm` (`std_msgs/Bool`), callback `arm_cb` aggiorna `g_armed` via `atomic_exchange`, log su transizione.
  - Publisher RELIABLE `/drone_1/armed` (`std_msgs/Bool`) on-change. Publish iniziale subito dopo `uros_init` (DISARMED) per i client che si collegano presto.
  - Executor: 1 → 2 handle (cmd_motor_test + arm).
- **task_motors**: gate arm (motori forzati a 0 quando `g_armed=false`, latenza max ~1ms). Sulla transizione disarm→arm resetta `last_cmd_us=0` per anti-replay (watchdog richiede cmd fresco prima di girare).
- **Docs**: `07-MICROROS-TETHERED.md` (nuova §4 arm, sub-§5 cmd_motor_test rinumerate, tabella topic 6 pub/2 sub, sequenza test estesa con casi 6-7 disarm in volo + re-arm), `08-BRINGUP-QUICKSTART.md` (§5.0 arm, layout pannelli con gauge motori + indicator armed + 2 publish), README `uros_interface` e `motor_driver`.
- **Build:** ESP-IDF `idf.py build` pulito (15% flash free), no warning. Limiti `app-colcon.meta` ok (6/8 pub, 2/4 sub).

**Verifica:** non eseguita sui motori (motori non saldati al PCB v1.0). Test funzionale rinviato al bring-up motori.

**Prossima sessione:** saldatura coreless + BT2.0 + switch arm hardware sul PCB v1.0; poi sequenza §5.2 di doc 07 (arm/disarm via Foxglove + cmd_motor_test).

---

## 2026-05-08 — Bring-up quickstart doc

**Branch:** `feature/microros-tethered`

**Attività svolta:**
- Creato `docs/08-BRINGUP-QUICKSTART.md`: checklist sintetica di sessione (3 terminali + Foxglove), layout pannelli consigliato, pubblicazione `cmd_motor_test`, shutdown ordinato, troubleshooting rapido. Complementare a `07-MICROROS-TETHERED.md` (guida estesa) e `ros2_ws/README.md` (setup iniziale workspace).

**Verifica:** non eseguita (solo doc).

---

## 2026-05-08 — Step 7+8 micro-ROS tethered (subscriber + chiusura documentale Fase 0B)

**Branch:** `feature/microros-tethered`

**Attività svolta:**
- **Step 7 implementato** (non testato sui motori reali, non ancora saldati al PCB v1.0):
  - Subscriber RELIABLE `/drone_1/cmd_motor_test` (`std_msgs/Float32MultiArray`).
  - Callback `cmd_motor_test_cb`: validazione `data.size==4` (warning + scarto altrimenti), clamp 0-100, `xQueueOverwrite` su `cmd_queue`.
  - Buffer di deserializzazione capacity=8 (publish lunghi non rompono il transport, scartati comunque dalla callback).
  - Executor 1 handle, `rclc_executor_spin_some(5ms)` nel loop di `task_microros`.
  - Watchdog 500ms su `task_motors` già operativo dallo Step 3, ora collegato ai cmd reali.
  - Build ESP-IDF pulita, firmware non flashato (motori non operativi).
- **Step 8 — chiusura documentale Fase 0B**:
  - Nuovo `docs/07-MICROROS-TETHERED.md`: guida operativa (avvio agent + Foxglove, topic, **istruzioni pannello Publish per `cmd_motor_test`**, troubleshooting, stato test).
  - `docs/02-FIRMWARE-ARCHITETTURA.md`: Fase 0B → 🟡 (firmware pronto, bring-up motori pendente).
  - `docs/specs/2026-04-26-stato-progetto-e-roadmap.md`: stato Fase 0B aggiornato.
  - `components/uros_interface/README.md`: subscriber documentato.
  - `components/motor_driver/README.md`: sezione "Watchdog (modalità uROS)" aggiunta.

**Verifica:** non eseguita per scelta dell'utente — motori non saldati e non alimentati. Step 7 va testato quando il bring-up motori sarà pronto, seguendo §4.2 di `docs/07-MICROROS-TETHERED.md`.

**Prossima sessione:** bring-up motori sul PCB v1.0 (saldatura coreless 8520, BT2.0, switch arm), poi test funzionale Step 7 via Foxglove Publish panel. Solo dopo si apre la Fase 1 (PID attitudine).

---

## 2026-05-08 — Step 5+6 micro-ROS tethered (publisher completi)

**Branch:** `feature/microros-tethered`

**Attività svolta:**
- **Step 5 chiuso**: implementato `uros_init()` reale (rcl support + node `/drone_1/drone_node` + 1 publisher BEST_EFFORT `/drone_1/imu/raw`). Task `task_microros` a 100Hz (alzata da 50), `QUEUE_DEPTH_IMU=20` (alzata da 5). Decimazione naturale 1/10 da 1kHz IMU. Conversioni `g→m/s²`, `deg/s→rad/s`. Frame_id `imu_link`, covarianze a -1 (unknown).
- **Step 6 chiuso**: aggiunti 4 publisher (`/flow`, `/range`, `/battery`, `/motors`). Tutti BEST_EFFORT.
- **`app-colcon.meta`** creato in root: alza i limiti hard-coded di micro-ROS (publishers 8, subscriptions 4, MTU 2048, stream history 8). Senza questo i 5 publisher non entrano (default 2) e i buffer di output saturano causando drop massicci (battery 100%, imu 50%).
- **`ros2_ws/`** creato in repo: workspace ROS2 Humble nativo con `micro_ros_agent` + `micro_ros_msgs` via `vcs import`, pacchetto `drone_bringup` con launch unico (agent UDP 8888 + foxglove_bridge 8765). Sostituisce l'agent in Docker (problemi IPC namespace su WSL2 → drop fino a 95% sui topic).
- **WiFi fix**: `WIFI_AUTH_OPEN` + `pmf_cfg.capable=true` per transition WPA2/WPA3 (auth fail con threshold restrittivo).
- **uROS init resilient**: ping retry infinito prima di `support_init`, retry su `support_init`. Niente più reboot loop se l'agent è giù.

**Verifiche eseguite (eliche staccate, USB power):**
- 5 topic visibili: `/drone_1/{imu/raw, flow, range, battery, motors}`
- Frequenze nominali (con `--qos-reliability best_effort` o Foxglove): IMU 103Hz std_dev 4ms, flow ~20Hz, range ~20Hz, battery 1Hz, motors ~100Hz
- Drone fermo: `linear_acceleration.z ≈ -9.74 m/s²` (NED, gravità giù → reading negativo, OK), gyro ~0
- Range coerente con distanza dal piano

**Osservazioni:**
- `voltage: 0.0` nel topic battery: atteso, drone alimentato da USB e BT2.0 staccato.
- `ros2 topic hz` di default usa QoS RELIABLE → frequenze inaffidabili sui nostri topic BEST_EFFORT. Misurare con `topic echo --qos-reliability best_effort` o Foxglove.
- 14 criticità documentate nel piano sezione "10bis" — leggere prima di future sessioni.

**Prossima sessione:** Step 7 (subscriber `/cmd_motor_test` + watchdog motori 500ms).

---

## Sessioni precedenti

Vedi i log git di `feature/microros-tethered` per Step 1-4 (managed component, Kconfig, skeleton task, WiFi connect).
