# Modalità tethered micro-ROS — guida operativa

**Stato:** Fase 0B chiusa via micro-ROS (publisher validati, subscriber implementato ma non testato sul lato motori).
**Branch firmware:** `feature/microros-tethered`.
**Riferimenti:** piano `docs/specs/2026-05-07-piano-implementativo-microros-tethered.md` (sezione 10bis = criticità incontrate, leggere PRIMA di toccare il setup).

Documento operativo: come avviare il drone in modalità tethered, vedere telemetria su Foxglove, e (quando i motori saranno operativi) pubblicare comandi di test motori.

---

## 1. Topologia

```
┌─────────────┐  USB-C   ┌──────────────┐
│  XIAO ESP32 │──────────│   PC (WSL2)  │
│  drone_1    │          │              │
└──────┬──────┘          │  ros2_ws/    │
       │ WiFi STA        │  ├ agent UDP │
       │ 192.168.1.15    │  │  :8888    │
       │                 │  └ foxglove  │
       └─── UDP 8888 ────┤    bridge ws │
            (uXRCE-DDS)  │    :8765     │
                         │              │
                         │  Foxglove    │
                         │  Studio      │
                         │  (Win)       │
                         └──────────────┘
```

- USB resta collegata: log seriale (`idf.py monitor`), flash, alimentazione XIAO durante sviluppo
- WiFi mirrored mode WSL2 → l'agent in WSL è raggiungibile dal drone come IP del PC sulla LAN
- Eliche **staccate**, switch arm motori del PCB **OFF** durante flash; **ON** solo durante test motori

## 2. Avvio

### 2.1 Drone

```bash
. ~/esp/esp-idf/export.sh
idf.py -p /dev/ttyACM0 flash monitor
```

Log atteso (in ordine):
1. `WiFi connected, IP=192.168.1.15`
2. `uROS agent target = <PC_IP>:8888`
3. `Ping agent in attesa che sia online...`
4. (se agent non ancora avviato) `Agent non risponde (tentativo N), continuo a riprovare...` ogni 10 ping
5. `Agent reachable dopo N tentativi`
6. `uros_init OK: node=/drone_1/drone_node`
7. `task_microros (Step 6) avviato — task @ 100 Hz`
8. `task_motors @ 1000Hz Core 1 (watchdog 500ms)`
9. log periodico: `pub: imu=100 (drain=1000) flow=20 batt=1 motors=100`

### 2.2 PC (agent + Foxglove bridge)

In un terminale WSL:
```bash
cd ~/microros-microdrone/ros2_ws
ros2 launch drone_bringup drone.launch.py
```
Avvia in un solo processo:
- `micro_ros_agent udp4 --port 8888 -v6` (telemetria + comandi)
- `foxglove_bridge --ros-args -p port:=8765` (visualizzazione)

### 2.3 Foxglove Studio (Windows)

`Open Connection` → `Foxglove WebSocket` → `ws://localhost:8765`. I topic `/drone_1/*` appaiono dopo il primo handshake col drone.

---

## 3. Topic disponibili

### 3.1 Pubblicati dal drone (8)

| Topic | Tipo | Freq | QoS | Frame ID |
|---|---|---|---|---|
| `/drone_1/imu/raw` | `sensor_msgs/Imu` | 100 Hz | BEST_EFFORT | `imu_link` |
| `/drone_1/flow` | `geometry_msgs/Vector3Stamped` | ~20 Hz | BEST_EFFORT | `flow_link` |
| `/drone_1/range` | `sensor_msgs/Range` | ~20 Hz | BEST_EFFORT | `range_link` |
| `/drone_1/battery` | `sensor_msgs/BatteryState` | 1 Hz | BEST_EFFORT | `battery_link` |
| `/drone_1/motors` | `std_msgs/Float32MultiArray` | ~100 Hz (echo) | BEST_EFFORT | — |
| `/drone_1/armed` | `std_msgs/Bool` | on-change + 1 al boot | RELIABLE | — |
| `/drone_1/temp` | `sensor_msgs/Temperature` | 1 Hz | BEST_EFFORT | `esp32_die` |
| `/drone_1/log` | `rcl_interfaces/Log` | event-driven (max ~500/s) | BEST_EFFORT | — |

Limiti micro-ROS in `app-colcon.meta`: `MAX_PUBLISHERS=12`, `MAX_SUBSCRIPTIONS=4`. 8/12 e 2/4 attualmente.

### 3.2 Sottoscritti dal drone (2)

| Topic | Tipo | QoS | Comportamento |
|---|---|---|---|
| `/drone_1/cmd_motor_test` | `std_msgs/Float32MultiArray` | RELIABLE | `data[0..3]` = duty FL,RL,RR,FR in 0-100. Lunghezza ≠ 4 → scartato + warning. Clamp 0-100. Watchdog 500ms su `task_motors`: assenza di nuovo comando → motori a 0. **Quando disarmato i motori sono forzati a 0** indipendentemente dal cmd ricevuto. |
| `/drone_1/arm` | `std_msgs/Bool` | RELIABLE | Sticky software arm. `true` → motori abilitati a girare (subordinatamente al watchdog cmd). `false` → motori forzati a 0 immediatamente (latenza ≤1ms a 1kHz). Boot: **DISARMED**. Sulla transizione disarm→arm il watchdog cmd è resettato: serve un nuovo `cmd_motor_test` prima che i motori possano girare. Echo dello stato su `/drone_1/armed`. |

---

## 4. Arm software (`/drone_1/arm` + `/drone_1/armed`)

Lo stato di arm è un flag atomico in firmware (`g_armed`, default `false` al boot). Cambia **solo** quando arriva un messaggio sul topic `/drone_1/arm`. Quando disarmato, `task_motors` forza l'output PWM a 0 a ogni ciclo (1kHz, latenza max ~1ms) ignorando qualsiasi `cmd_motor_test`.

### 4.1 Pannello Publish — arm/disarm

In Foxglove aggiungi un pannello **Publish**:
- **Topic:** `/drone_1/arm`
- **Schema:** `std_msgs/msg/Bool`
- **Button mode:** `Publish` (one-shot — lo stato è sticky lato firmware)
- Payload ARM: `{ "data": true }` — Payload DISARM: `{ "data": false }`

Comoda anche una coppia di pulsanti separati (due pannelli Publish, uno per `true` e uno per `false`).

### 4.2 Indicatore stato (`/drone_1/armed`)

Aggiungi un pannello **Indicator** o **Plot** su `/drone_1/armed.data`. Il drone ripubblica lo stato:
- una volta dopo `uros_init` (DISARMED)
- a ogni transizione (callback `arm_cb`)

Se il pannello Indicator non vede nulla all'apertura, premi una volta arm/disarm — il messaggio successivo riempie l'indicatore.

### 4.3 CLI equivalente

```bash
ros2 topic pub --once /drone_1/arm  std_msgs/msg/Bool "{data: true}"
ros2 topic pub --once /drone_1/arm  std_msgs/msg/Bool "{data: false}"
ros2 topic echo /drone_1/armed
```

### 4.4 Comportamento sulle transizioni

| Transizione | Effetto |
|---|---|
| boot | `g_armed=false`, motori a 0, /armed pubblicato (false) |
| disarmato → armato | `last_cmd_us=0` → motori restano 0 finché non arriva un nuovo `cmd_motor_test` (anti-replay di cmd stantii) |
| armato → disarmato | motori a 0 entro 1ms (gate in `task_motors`) |
| arm con stesso valore | nessun publish duplicato su `/armed` (only on-change) |

---

## 5. Pubblicare `cmd_motor_test` da Foxglove

Foxglove Studio supporta la pubblicazione di topic ROS2 attraverso il `foxglove_bridge`. Il pannello dedicato è **Publish**.

### 5.1 Setup pannello Publish

1. In Foxglove Studio: pannello in alto a destra → "+" → cerca **Publish** → aggiungi
2. Nel pannello, configura:
   - **Topic:** `/drone_1/cmd_motor_test`
   - **Message schema:** `std_msgs/msg/Float32MultiArray`
   - **Button mode:** scegli `Publish` (one-shot) o `Publish on hold`/`Publish at rate` (per il watchdog)
3. Nel campo testo del messaggio, inserisci JSON:
   ```json
   {
     "layout": { "dim": [], "data_offset": 0 },
     "data": [10.0, 10.0, 10.0, 10.0]
   }
   ```
   `data` è l'unico campo che leggiamo: 4 float in 0-100, mapping FL, RL, RR, FR.

### 5.2 Test sequenza (eliche STACCATE, switch arm hardware ON, **arm software ON**)

> Prerequisito: pubblica `/drone_1/arm` con `data: true` prima di iniziare. Senza arm i motori restano a 0 anche con cmd validi.

| # | Azione | Atteso |
|---|---|---|
| 1 | Pubblica `[5,5,5,5]` ripetuto a ≥5Hz (Publish at rate) | Tutti i motori al 5%. `/drone_1/motors` echo coerente. |
| 2 | Smetti di pubblicare (stop rate / chiudi pannello) | Entro 500ms motori a 0. Verificabile su `/drone_1/motors`. |
| 3 | Pubblica `[15,0,0,0]` once | Solo FL gira. |
| 4 | Pubblica `[150,-10,50,200]` once | Echo `[100,0,50,100]` (clamp). |
| 5 | Pubblica `data` di lunghezza 2 | Serial monitor: `W (uros) cmd_motor_test: size=2 atteso 4 — scartato`. Motori restano fermi. |
| 6 | In pieno duty `[20,20,20,20]` @ 5Hz, pubblica `arm: false` | Motori a 0 entro 1ms. /armed → false. cmd_motor_test continua ad arrivare ma è ignorato. |
| 7 | Da disarmato, pubblica `arm: true` senza inviare cmd | Motori restano 0 (anti-replay: watchdog richiede un cmd nuovo). |

> **Watchdog:** il `task_motors` azzera i motori se non riceve un cmd entro `MOTOR_CMD_TIMEOUT_MS=500ms`. Per duty sostenuto serve `Publish at rate` ≥ 3Hz (3 cmd/s, margine 333ms < 500ms).

### 5.3 Alternativa CLI

```bash
ros2 topic pub -r 5 /drone_1/cmd_motor_test std_msgs/msg/Float32MultiArray \
  "{data: [5.0, 5.0, 5.0, 5.0]}"
# Ctrl+C → motori a 0 entro 500ms
```

> **Nota:** alcune versioni del Publish panel di Foxglove **non hanno l'opzione "Publish at rate" nativa** (solo bottone one-shot o "publish on hold"). In quel caso è obbligatorio usare il `ros2 topic pub -r` da terminale. Il drone non si comporta diversamente: vale solo il rate effettivo a cui i messaggi arrivano sul topic.

---

## 6. Console di debug Foxglove (`/drone_1/log`)

Il drone pubblica una console di log strutturata su `/drone_1/log` (`rcl_interfaces/msg/Log`, BEST_EFFORT). Foxglove la mostra nativamente nel **Log panel**, con filtri per livello e per `name`.

### 6.1 Aggiungere il pannello

`+` → cerca **Log** → Add. Settings → Topic = `/drone_1/log`. Default OK: mostra tutti i livelli, `name` filterable.

Livelli usati:

| Livello | Valore | Quando |
|---|---|---|
| INFO  | 20 | eventi normali (boot, WiFi up, ARM ack) |
| WARN  | 30 | retry, transizioni di safety, anomalie recuperabili |
| ERROR | 40 | reset reason critici (BROWNOUT, PANIC, WDT) |

### 6.2 Eventi loggati attualmente

| Evento | Livello | Messaggio tipico |
|---|---|---|
| Reset reason al boot | INFO/WARN/ERROR | `boot reset_reason=BROWNOUT (15)` |
| uROS pronto | INFO | `uROS ready: node=/drone_1/drone_node, agent=...:8888` |
| WiFi associato | INFO | `WiFi associato, attesa DHCP` |
| WiFi disconnesso | WARN | `WiFi disconnect retry=3 reason=201` |
| WiFi connesso (IP) | INFO | `WiFi connected IP=192.168.1.15` |
| Arm/disarm | WARN | `ARM DISARMED -> ARMED` |
| Watchdog motori scattato | WARN | `motors watchdog: cmd timeout >500ms, motori azzerati` |
| Flusso cmd ripreso | INFO | `motors: cmd flow ripreso` |
| `cmd_motor_test` malformato | WARN | `cmd_motor_test size=2 (atteso 4) scartato` |

> **Caso d'uso primario adesso:** se il drone si resetta durante un test motori (sospetto brownout dovuto al supply / buck-boost), al boot successivo il primo log su `/drone_1/log` ti dice `boot reset_reason=BROWNOUT` con livello ERROR. Conferma diagnosi senza dover essere collegato al monitor seriale.

### 6.3 CLI equivalente

```bash
# Live tail di tutti i log del drone (BEST_EFFORT obbligatorio)
ros2 topic echo /drone_1/log --qos-reliability best_effort

# Solo le entry warning+ (filtro client-side)
ros2 topic echo /drone_1/log --qos-reliability best_effort \
  --filter "m.level >= 30"
```

### 6.4 Vincoli

- Implementazione: queue FreeRTOS 16 slot, drain in `task_microros` cap 5 msg/ciclo (10ms) → 500/s effettivi. **Burst oltre soglia: drop silenzioso** (la perdita di un log non è critica).
- Lunghezza messaggio troncata a ~190 byte.
- `uros_log()` (vedi `components/uros_interface/include/uros_interface.h`) **non va chiamata da ISR**.
- BEST_EFFORT: i log persi non vengono ritrasmessi. È accettabile per un canale diagnostico; per eventi critici di safety usare un publisher RELIABLE dedicato.

---

## 7. Troubleshooting

### Drone non si connette al WiFi
- Verifica credenziali in `sdkconfig` (gitignored, possono tornare al default dopo `idf.py reconfigure`):
  ```bash
  grep DRONE_WIFI sdkconfig
  ```
- WPA2/WPA3 transition: già gestito (`WIFI_AUTH_OPEN` + `pmf_cfg.capable=true`). Reti pure-WPA3 senza transition non sono testate.

### Drone connette ma `Ping agent` continua a fallire
- Agent in ascolto? `ss -unlp | grep 8888` in WSL
- Windows Defender blocca UDP 8888 inbound:
  ```powershell
  # PowerShell admin, una tantum
  New-NetFirewallRule -DisplayName "uROS Agent UDP 8888" -Direction Inbound -Protocol UDP -LocalPort 8888 -Action Allow
  ```
- WSL networking mirrored attivo? `cat /mnt/c/Users/<user>/.wslconfig` deve contenere `networkingMode=mirrored`.
- IP agent nel firmware: `idf.py menuconfig` → "Drone — micro-ROS / WiFi" → verifica `CONFIG_DRONE_UROS_AGENT_IP`.

### `ros2 topic hz /drone_1/imu/raw` mostra ~50% drop
- Default QoS RELIABLE → mismatch con publisher BEST_EFFORT. Misurare con:
  ```bash
  ros2 topic echo /drone_1/imu/raw --qos-reliability best_effort
  ```
  oppure usare Foxglove (autonegozia QoS).

### `ros2 topic list` blocca
- Daemon ROS2 stantio dopo restart agent:
  ```bash
  pkill -9 -f _ros2_daemon
  ros2 daemon start
  # oppure
  ros2 topic list --no-daemon
  ```

### Drone in reboot loop
- Agent non raggiungibile + bug in `RCCHECK` → fixato (Step 5): ora si fa ping retry **prima** di `support_init`. Se il sintomo torna, è una regressione.

### `voltage: 0.0` su `/battery`
- Atteso quando il drone è alimentato solo da USB-C (BT2.0 staccato → partitore VBAT scollegato). Connettendo l'alimentatore da banco via BT2.0 deve apparire la tensione vera.

### Modifiche a `app-colcon.meta` non vengono applicate
- `libmicroros.a` non rebuilda automaticamente. Forzare:
  ```bash
  rm -f managed_components/micro_ros_espidf_component/libmicroros.a
  rm -rf managed_components/micro_ros_espidf_component/micro_ros_src/{build,install}
  rm -rf build
  idf.py build   # ~5 min
  ```

---

## 8. Stato test

**2026-05-09:** console `/drone_1/log` aggiunta (rcl_interfaces/Log, BEST_EFFORT). 8/12 publisher utilizzati. Test motori in corso: drone si resetta sopra il 20% PWM su 2+ motori — sospetto brownout dovuto al buck-boost AliExpress; da confermare via reset reason loggato al boot successivo (`boot reset_reason=BROWNOUT` su `/drone_1/log`). Soluzione raccomandata: passaggio a LiPo 1S 25C, oppure cap 1000 µF + 100 nF sull'uscita del buck.

**2026-05-08:**

| Test | Stato | Note |
|---|---|---|
| WiFi connect | ✅ | LiboHouse, drone 192.168.1.15 |
| Ping agent + uros_init | ✅ | Resilient, niente reboot loop |
| 5 publisher attivi | ✅ | Frequenze nominali ±10% |
| IMU dati coerenti | ✅ | accel.z ≈ -9.74 m/s² fermo, gyro ~0 |
| Range ToF coerente | ✅ | Verificato vs piano |
| Battery placeholder | ⚠️ | `0.0` da USB, da rivalidare con BT2.0 |
| Subscriber `cmd_motor_test` callback | ⚠️ implementato non testato | Motori non saldati al 2026-05-08 |
| Watchdog 500ms motori | ⚠️ implementato non testato | Idem |
| Subscriber `/arm` + gate motori | ⚠️ implementato non testato | Build OK al 2026-05-08, test motori pendente |
| Publisher `/armed` on-change | ⚠️ implementato non testato | Idem |
| Foxglove Publish panel cmd_motor_test / arm | ⚠️ pendente | Quando motori operativi |

Il bring-up motori (saldatura, alimentazione BT2.0, switch arm, prima rotazione) chiuderà ufficialmente la Fase 0B.

---

## 8. Vincoli operativi (rimemoria)

- **Eliche STACCATE** in tutta la fase tethered (fino a fine Fase 1 PID).
- **Switch arm motori OFF** durante flash; ON solo per test attivi.
- **USB sempre collegata** durante sviluppo (log + flash).
- Sicurezza: il watchdog 500ms NON sostituisce un kill switch fisico. Tenere alimentatore BT2.0 a portata di mano.
