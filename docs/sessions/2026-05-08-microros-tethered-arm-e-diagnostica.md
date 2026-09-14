# 2026-05-08 — micro-ROS tethered: publisher, subscriber, arm software, diagnostica WiFi

**Branch:** `feature/microros-tethered` · **Commit:** da `e674d17` a `59751d7`

Giornata lunga, sette blocchi di lavoro. Ordine cronologico crescente.

---

## 1. Step 5+6 — publisher completi

### Fatto
- **Step 5:** `uros_init()` reale (rcl support + node `/drone_1/drone_node` + publisher BEST_EFFORT `/drone_1/imu/raw`). `task_microros` a 100Hz (da 50), `QUEUE_DEPTH_IMU=20` (da 5). Decimazione naturale 1/10 dai 1kHz dell'IMU. Conversioni `g→m/s²`, `deg/s→rad/s`. Frame_id `imu_link`, covarianze a -1 (unknown).
- **Step 6:** aggiunti 4 publisher (`/flow`, `/range`, `/battery`, `/motors`), tutti BEST_EFFORT.
- **`app-colcon.meta`** creato in root: alza i limiti hard-coded di micro-ROS (publishers 8, subscriptions 4, MTU 2048, stream history 8).
- **`ros2_ws/`** creato in repo: workspace ROS2 Humble nativo con `micro_ros_agent` + `micro_ros_msgs` via `vcs import`, pacchetto `drone_bringup` con launch unico (agent UDP 8888 + foxglove_bridge 8765).
- **WiFi:** `WIFI_AUTH_OPEN` + `pmf_cfg.capable=true` per transition WPA2/WPA3.
- **uROS init resiliente:** ping retry infinito prima di `support_init`, retry su `support_init`. Niente più reboot loop se l'agent è giù.

### Scoperto
- Senza `app-colcon.meta` i 5 publisher non entrano (default 2) e i buffer di output saturano causando drop massicci (battery 100%, imu 50%).
- L'agent in Docker su WSL2 ha problemi di IPC namespace → drop fino al 95% sui topic. Da qui la scelta del workspace nativo.
- `ros2 topic hz` di default usa QoS RELIABLE → frequenze inaffidabili sui nostri topic BEST_EFFORT. Misurare con `topic echo --qos-reliability best_effort` o Foxglove.

### Verificato (eliche staccate, USB power)
- 5 topic visibili: `/drone_1/{imu/raw, flow, range, battery, motors}`
- Frequenze nominali: IMU 103Hz std_dev 4ms, flow ~20Hz, range ~20Hz, battery 1Hz, motors ~100Hz
- Drone fermo: `linear_acceleration.z ≈ -9.74 m/s²` (NED, gravità giù → reading negativo, corretto), gyro ~0
- Range coerente con la distanza dal piano
- `voltage: 0.0` nel topic battery: atteso, drone su USB e BT2.0 staccato

14 criticità documentate nella sezione "10bis" del piano — leggere prima di future sessioni.

---

## 2. Step 7+8 — subscriber + chiusura documentale Fase 0B

### Fatto
- Subscriber RELIABLE `/drone_1/cmd_motor_test` (`std_msgs/Float32MultiArray`).
- Callback `cmd_motor_test_cb`: validazione `data.size==4` (warning + scarto altrimenti), clamp 0-100, `xQueueOverwrite` su `cmd_queue`.
- Buffer di deserializzazione capacity=8: publish lunghi non rompono il transport, vengono comunque scartati dalla callback.
- Executor 1 handle, `rclc_executor_spin_some(5ms)` nel loop di `task_microros`.
- Watchdog 500ms su `task_motors`, già operativo dallo Step 3, ora collegato ai comandi reali.
- Chiusura documentale: nuovo `docs/grounding/06-MICROROS-TETHERED.md`, Fase 0B → 🟡 in `docs/grounding/03-FIRMWARE-ARCHITETTURA.md`, aggiornati i README di `uros_interface` e `motor_driver`.

### Aperto
Non testato sui motori reali: non ancora saldati al PCB v1.0. Build ESP-IDF pulita, firmware non flashato.

---

## 3. Bring-up quickstart doc

### Fatto
Creato `docs/grounding/05-BRINGUP-QUICKSTART.md`: checklist sintetica di sessione (3 terminali + Foxglove), layout pannelli consigliato, pubblicazione `cmd_motor_test`, shutdown ordinato, troubleshooting rapido. Complementare a `docs/grounding/06-MICROROS-TETHERED.md` (guida estesa) e `ros2_ws/README.md` (setup iniziale).

---

## 4. Arm software (sub `/arm` + pub `/armed`) + gate motori

### Fatto
- **Flag globale `atomic_bool g_armed`**: dichiarato in `components/common/include/drone_types.h`, definito in `main/main.c` (boot DISARMED). Atomic per accesso cross-core safe (callback Core 0 ↔ `task_motors` Core 1 @ 1kHz).
- **uros_interface:** subscriber RELIABLE `/drone_1/arm` (`std_msgs/Bool`), callback `arm_cb` che aggiorna `g_armed` via `atomic_exchange` con log sulla transizione. Publisher RELIABLE `/drone_1/armed` on-change, con publish iniziale subito dopo `uros_init` (DISARMED) per i client che si collegano presto. Executor: 1 → 2 handle.
- **task_motors:** gate arm (motori forzati a 0 quando `g_armed=false`, latenza max ~1ms). Sulla transizione disarm→arm resetta `last_cmd_us=0` per anti-replay: il watchdog richiede un comando fresco prima di far girare.
- **Build:** `idf.py build` pulito (15% flash free), nessun warning. Limiti `app-colcon.meta` ok (6/8 pub, 2/4 sub).

### Aperto
Verifica non eseguita sui motori (non saldati al PCB v1.0).

---

## 5. Publisher temperatura on-die

### Fatto
- Publisher BEST_EFFORT `/drone_1/temp` (`sensor_msgs/Temperature`, 1 Hz, frame `esp32_die`, variance 4.0).
- Init `temperature_sensor` ESP-IDF (range 10–80°C, accuracy ±2°C nominale), gate 1 Hz dentro `task_microros` via `esp_timer_get_time`. Componente `esp_driver_tsens` aggiunto a `REQUIRES`.
- Limiti micro-ROS: 7/8 pub, 2/4 sub. Build pulito, 14% flash libero.

### Aperto
Verifica non eseguita (richiede flash + monitor).

---

## 6. WiFi retry infinito (niente reboot loop)

### Fatto
- Rimossi `WIFI_MAX_RETRY` e `WIFI_TIMEOUT_MS`. Retry infinito, allineato alla policy del ping-agent dello Step 5.
- Handler `WIFI_EVENT_STA_CONNECTED` per resettare `s_retry_num` sull'associazione, non solo su `GOT_IP`.
- `uros_wifi_connect()` blocca su `xEventGroupWaitBits(... portMAX_DELAY)` invece di abortire al timeout.
- Log: ogni retry per i primi 10, poi ogni 10 (riduce lo spam).

### Scoperto
Il drone si associava brevemente all'AP (visibile dalla pagina admin del router) ma veniva disconnesso prima di completare il DHCP. `s_retry_num` arrivava a 10 in pochi secondi → `ESP_ERROR_CHECK(uros_wifi_connect())` abortiva → reboot loop sterile. Resettare il contatore su CONNECTED evita di consumare retry quando l'AP accetta l'auth ma kicka durante il DHCP, caso transitorio frequente.

---

## 7. WiFi flapping = brownout sull'USB del PC

### Scoperto — la diagnosi più importante della giornata

**Sintomo:** il drone non si associava più al router di casa (`LiboHouse`, WPA2-PSK[ES]+WPA3-Personal con band steering unique SSID). Funzionava sull'hotspot del telefono. Il pattern dei retry era **inconsistente**: la stessa configurazione produceva di volta in volta `reason=201` (NO_AP_FOUND), `reason=2` (AUTH_LEAVE dopo ~1s di auth), `reason=205` (CONNECTION_FAIL), oppure auth-OK seguita da assoc-fail dopo 1s. Niente di deterministico.

**Diagnostica firmware, tutta a vuoto:**
- Threshold `WIFI_AUTH_WPA2_PSK` forzato → peggiora (deauth reason 2 in 1s).
- `pmf_cfg.required = true` → l'AP rifiuta la pre-auth (`AP not PMF Capable when STA requires`, reason 210). Vale solo con WPA3 attivo.
- WPA3 disabilitato sul router → stesso flapping.
- Aggiunto logging del reason code e finestra retry estesa da 10 a 30 per catturare i fallimenti di assoc che cadevano nel buco di logging.

**Causa reale: alimentazione.** Sull'USB del PC (laptop, WSL2 attivo) i burst TX WiFi (~300-500 mA istantanei) provocano brownout sul PA dell'ESP32-S3 → frame management persi in modo random → ogni tentativo fallisce in una fase diversa. Su powerbank o batteria il problema sparisce e l'associazione è immediata.

L'inconsistenza del pattern era il vero indizio: un problema di configurazione WiFi fallisce sempre allo stesso punto.

**Fix:** alimentare il drone da powerbank/batteria durante lo sviluppo, non dall'USB del laptop quando c'è WiFi attivo. L'USB resta buona per flash + monitor seriale (basso assorbimento).

**Lasciato nel firmware perché utile:** log del `reason` su `STA_DISCONNECTED`, finestra retry 1-30 + multipli di 10.

### Verificato
OK su powerbank: il drone associa subito a `LiboHouse` su canale 11.
