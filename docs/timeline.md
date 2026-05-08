# Timeline progetto microros-microdrone

Cronologia delle attività significative. Aggiornata a fine sessione.

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
