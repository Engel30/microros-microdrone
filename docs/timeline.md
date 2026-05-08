# Timeline progetto microros-microdrone

Cronologia delle attività significative. Aggiornata a fine sessione.

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
