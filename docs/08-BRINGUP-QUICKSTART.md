# Bring-up quickstart — sessione tipo

**Obiettivo:** in <5 minuti avere drone connesso, telemetria su Foxglove, e (motori montati) capacità di pubblicare `cmd_motor_test`.

Questo è il **flusso da seguire ogni sessione**. Per i dettagli (topologia, topic, troubleshooting esteso, sequenze di test) vedi `07-MICROROS-TETHERED.md`. Per il setup iniziale di ROS2/agent una tantum, vedi `../ros2_ws/README.md`.

---

## 0. Pre-flight checklist (ogni sessione)

- [ ] Eliche **STACCATE** (rimangono staccate fino a fine Fase 1 PID)
- [ ] Switch arm motori sul PCB **OFF** (durante flash e idle; ON solo per test attivi)
- [ ] BT2.0 staccato → drone alimentato solo USB-C (oppure BT2.0 collegato per leggere `voltage` reale)
- [ ] PC e drone sulla stessa rete WiFi (`LiboHouse` di default)
- [ ] WSL2 mirrored mode attivo (`cat /mnt/c/Users/<user>/.wslconfig` → `networkingMode=mirrored`)

---

## 1. Terminale 1 — Drone (firmware)

```bash
cd ~/microros-microdrone
. ~/esp/esp-idf/export.sh
idf.py -p /dev/ttyACM0 flash monitor
```

**Log atteso (in ordine):**
1. `WiFi connected, IP=192.168.1.15`
2. `uROS agent target = <PC_IP>:8888`
3. `Ping agent in attesa che sia online...` (se l'agent non è ancora avviato è normale; resterà in retry)
4. *(dopo aver avviato il T2)* `Agent reachable dopo N tentativi`
5. `uros_init OK: node=/drone_1/drone_node`
6. `task_microros (Step 6) avviato — task @ 100 Hz`
7. `task_motors @ 1000Hz Core 1 (watchdog 500ms)`
8. periodico: `pub: imu=100 (drain=1000) flow=20 batt=1 motors=100`

**Uscita dal monitor:** `Ctrl+]`.

> **USB persa dopo flash?** PowerShell admin: `usbipd attach --wsl --busid 1-6` (verifica busid con `usbipd list`).

---

## 2. Terminale 2 — Agent + Foxglove bridge

```bash
cd ~/microros-microdrone/ros2_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch drone_bringup drone.launch.py
```

Avvia in un solo processo:
- `micro_ros_agent udp4 --port 8888 -v6` (XRCE-DDS ↔ ROS2)
- `foxglove_bridge` su ws://localhost:8765

**Conferma di handshake** (nel log dell'agent):
```
create_participant ... | OK
create_topic         | OK
... session established ...
```
Quando vedi le `OK` ripetute, il drone ha completato la registrazione dei publisher.

> **IP dell'agent:** WSL2 mirrored = stesso IP del PC sulla LAN. Verifica con `hostname -I`. Deve coincidere con `CONFIG_DRONE_UROS_AGENT_IP` nel `sdkconfig` (`idf.py menuconfig` → *Drone — micro-ROS / WiFi*).

---

## 3. Terminale 3 — Sanity check CLI (opzionale)

```bash
source /opt/ros/humble/setup.bash

# I topic /drone_1/* devono comparire (8 publisher + 2 sub al 2026-05-09)
ros2 topic list | grep drone_1

# Lettura puntuale (occhio al QoS: i pub sono BEST_EFFORT)
ros2 topic echo /drone_1/imu/raw     --qos-reliability best_effort --once
ros2 topic echo /drone_1/range       --qos-reliability best_effort --once
ros2 topic echo /drone_1/battery     --qos-reliability best_effort --once

# Console di debug (live tail)
ros2 topic echo /drone_1/log --qos-reliability best_effort

# Frequenze (usa --qos-reliability best_effort sennò vedi falsi drop)
ros2 topic hz /drone_1/imu/raw --qos-reliability best_effort
```

**Valori attesi a drone fermo:**
- `imu/raw.linear_acceleration.z ≈ -9.74` m/s² (NED, gravità verso il basso → reading negativo)
- `imu/raw.angular_velocity` ≈ 0 su tutti gli assi
- `range.range` coerente con la distanza dal piano
- `battery.voltage` = `0.0` se alimentato solo da USB (atteso)

---

## 4. Foxglove Studio (Windows)

### 4.1 Connessione

`Open Connection` → **Foxglove WebSocket** → URL → Open.

| Dove gira Foxglove | URL |
|---|---|
| Windows host | `ws://<IP_PC_in_LAN>:8765` (es. `192.168.1.9`) — **NON** l'IP del drone |
| WSL/WSLg o Linux nativo sul PC | `ws://localhost:8765` |

> ⚠️ Il `foxglove_bridge` gira sul **PC**, non sul drone. `192.168.1.15` è l'IP del drone (ESP32) e non ascolta su 8765. Trova l'IP del PC con `hostname -I` (WSL) o `ipconfig` (PowerShell).
>
> Se la connessione fallisce dal Windows host: Firewall Windows blocca TCP 8765 inbound. Una tantum, PowerShell admin:
> ```powershell
> New-NetFirewallRule -DisplayName "Foxglove Bridge TCP 8765" -Direction Inbound -Protocol TCP -LocalPort 8765 -Action Allow
> ```

I topic `/drone_1/*` appaiono entro pochi secondi. Foxglove autonegozia il QoS (no problemi BEST_EFFORT vs RELIABLE).

### 4.2 Layout consigliato

Aggiungi pannelli (`+` in alto a destra) e configura:

| Pannello | Topic | Campo / Note |
|---|---|---|
| **Plot** | `/drone_1/imu/raw` | `linear_acceleration.x`, `.y`, `.z` (3 series) |
| **Plot** | `/drone_1/imu/raw` | `angular_velocity.x`, `.y`, `.z` (3 series) |
| **Plot** | `/drone_1/range` | `range` |
| **Plot** | `/drone_1/flow` | `vector.x`, `vector.y` |
| **Indicator** | `/drone_1/battery` | `voltage` |
| **Plot/Indicator** | `/drone_1/temp` | `temperature` (°C, on-die ESP32-S3, ±2°C) |
| **Gauge** ×4 | `/drone_1/motors` | `data[0]`, `[1]`, `[2]`, `[3]` separati (FL/RL/RR/FR, %PWM 0–100) |
| **Plot** | `/drone_1/motors` | `data[0..3]` insieme (storico) |
| **Indicator** | `/drone_1/armed` | `data` (ARMED/DISARMED) |
| **Publish** | `/drone_1/arm` | Bool — vedi §5 |
| **Publish** | `/drone_1/cmd_motor_test` | Float32MultiArray — vedi §5 |
| **Log** | `/drone_1/log` | console di debug (boot reset reason, WiFi, arm, watchdog) — vedi 07 §6 |

> Salva il layout (`Layout → Export…`) per non rifarlo a ogni sessione. Convenzione: salvalo in `~/microros-microdrone/foxglove-layouts/drone_1-tethered.json` (non versionato).

### 4.3 Anteprima da WSL (opzionale)

Foxglove esiste anche per Linux: se preferisci tutto in WSL2 con WSLg, scaricalo da `https://foxglove.dev/download` e collegati a `ws://localhost:8765` allo stesso modo.

---

## 5. Comandare i motori — arm + `cmd_motor_test`

> ⚠️ **Switch arm motori hardware ON solo durante questi test.** Eliche staccate. Tieni l'alimentazione BT2.0 a portata di mano per kill manuale.

### 5.0 Arm software (`/drone_1/arm` Bool)

Boot del drone = **DISARMED**. Senza arm i motori restano a 0 anche con `cmd_motor_test` valido.

Pannello **Publish** → topic `/drone_1/arm`, schema `std_msgs/msg/Bool`, button mode `Publish` (one-shot).
- ARM: `{"data": true}` → l'indicatore `/drone_1/armed` diventa true.
- DISARM: `{"data": false}` → motori a 0 entro 1ms.

CLI:
```bash
ros2 topic pub --once /drone_1/arm std_msgs/msg/Bool "{data: true}"
ros2 topic pub --once /drone_1/arm std_msgs/msg/Bool "{data: false}"
```

> Anti-replay: alla transizione disarm→arm il watchdog cmd è resettato. I motori NON ripartono col vecchio cmd: serve un nuovo `cmd_motor_test`. Vedi `07-MICROROS-TETHERED.md` §4.

### 5.1 Pannello Publish in Foxglove — `cmd_motor_test`

1. Aggiungi pannello **Publish**.
2. Configura:
   - **Topic:** `/drone_1/cmd_motor_test`
   - **Schema:** `std_msgs/msg/Float32MultiArray`
   - **Button mode:** `Publish at rate` (≥3Hz, raccomandato 5Hz) per duty sostenuto, oppure `Publish` one-shot per impulso singolo. **Se la tua versione di Foxglove non offre l'opzione "at rate"** (alcune build hanno solo one-shot / on-hold), usa il `ros2 topic pub -r 5 ...` di §5.3 da terminale.
3. Payload JSON:
   ```json
   {
     "layout": { "dim": [], "data_offset": 0 },
     "data": [10.0, 10.0, 10.0, 10.0]
   }
   ```
   `data` = duty `[FL, RL, RR, FR]` in 0–100. Lunghezza ≠ 4 → scartato + warning sul monitor seriale. Clamp 0–100.

### 5.2 Watchdog 500ms

`task_motors` azzera i motori se non riceve nuovi cmd entro 500ms.
- Per duty sostenuto serve **rate ≥ 3Hz** (margine 333ms).
- Stop manuale = chiudi il pannello / smetti di pubblicare → motori a 0 entro 500ms.

### 5.3 Alternativa CLI

```bash
# Tutti al 5%
ros2 topic pub -r 5 /drone_1/cmd_motor_test std_msgs/msg/Float32MultiArray \
  "{data: [5.0, 5.0, 5.0, 5.0]}"
# Ctrl+C → motori a 0 entro 500ms

# Singolo motore (FL al 15%, gli altri a 0)
ros2 topic pub -r 5 /drone_1/cmd_motor_test std_msgs/msg/Float32MultiArray \
  "{data: [15.0, 0.0, 0.0, 0.0]}"
```

### 5.4 Sequenza di test minima (riferimento §5.2 doc 07)

> Ordine: prima `arm: true`, poi cmd. Senza arm tutto resta a 0.

| # | Comando | Atteso |
|---|---|---|
| 0 | `arm: true` | `/armed` → true |
| 1 | `[5,5,5,5]` @ 5Hz | tutti al 5%, `/drone_1/motors` echo coerente |
| 2 | stop pubblicazione | motori a 0 entro 500ms |
| 3 | `[15,0,0,0]` once | solo FL gira |
| 4 | `[150,-10,50,200]` once | echo `[100,0,50,100]` (clamp) |
| 5 | `data` lunghezza 2 | warning su monitor, motori fermi |
| 6 | durante duty `[20,20,20,20]` @ 5Hz, `arm: false` | motori a 0 entro 1ms, /armed → false |
| 7 | da disarmato, `arm: true` (senza nuovo cmd) | motori restano 0 (anti-replay watchdog) |

---

## 6. Shutdown ordinato

1. **Foxglove:** stop pubblicazione `cmd_motor_test` (chiudi pannello Publish o disattiva `Publish at rate`) → attendi 500ms.
2. **Switch arm motori OFF.**
3. **Terminale 2:** `Ctrl+C` sul launch (chiude agent + bridge).
4. **Terminale 1:** `Ctrl+]` per uscire dal monitor (l'ESP32 resta in esecuzione finché alimentato).
5. Stacca USB.

---

## 7. Errori frequenti (vedi 07 §5 per dettagli)

| Sintomo | Causa probabile | Fix rapido |
|---|---|---|
| `Ping agent` continua a fallire | agent non in ascolto / firewall Windows UDP 8888 | `ss -unlp \| grep 8888` in WSL; aprire UDP 8888 inbound |
| `ros2 topic list` blocca | daemon ROS2 stantio | `pkill -9 -f _ros2_daemon && ros2 daemon start` |
| `topic hz` mostra ~50% drop | QoS RELIABLE di default vs publisher BEST_EFFORT | aggiungere `--qos-reliability best_effort` |
| Reboot loop drone | regressione del fix ping-prima-di-`support_init` | controlla log, segnala |
| `voltage: 0.0` | alimentazione USB-C, BT2.0 staccato | atteso |
| `app-colcon.meta` modificato non applicato | `libmicroros.a` non rebuilda | `rm -f managed_components/.../libmicroros.a` + `rm -rf build` + rebuild (~5 min) |
| Drone si resetta a duty motori >10% | brownout rail VCC: buck-boost AliExpress non regge transient PWM | controlla `/drone_1/log` al boot dopo: `boot reset_reason=BROWNOUT` conferma. Fix: LiPo 1S 25C, oppure cap 1000 µF sull'uscita buck. |

---

## 8. Riferimenti

- **Guida operativa estesa:** `07-MICROROS-TETHERED.md`
- **Setup iniziale ROS2 workspace:** `../ros2_ws/README.md`
- **Stato/roadmap progetto:** `specs/2026-04-26-stato-progetto-e-roadmap.md`
- **Piano implementativo + criticità:** `specs/2026-05-07-piano-implementativo-microros-tethered.md` (sezione 10bis)
- **Architettura firmware:** `02-FIRMWARE-ARCHITETTURA.md`
