# ros2_ws — Workspace ROS2 lato PC

Contiene tutto ciò che gira sul PC per il drone tethered:
- `micro_ros_agent` (ponte XRCE-DDS ↔ ROS2)
- `foxglove_bridge` (WebSocket per Foxglove Studio)
- `drone_bringup` (launch unico)
- (in futuro) nodi di planning, gestione sciame, GUI custom

Distro target: **ROS2 Humble** su Ubuntu 22.04 / WSL2 Ubuntu 22.04.

---

## Setup iniziale (una volta sola)

### 1. Installa ROS2 Humble e dipendenze apt

```bash
# ROS2 Humble (https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debs.html)
# ... [ometti se già installato]

sudo apt update
sudo apt install -y \
    python3-colcon-common-extensions \
    python3-vcstool \
    python3-rosdep \
    ros-humble-foxglove-bridge

# rosdep init (solo prima volta sulla macchina)
sudo rosdep init || true
rosdep update
```

### 2. Scarica i sorgenti di terze parti

Dal `ros2_ws/`:
```bash
cd ros2_ws
vcs import src < microros.repos
```

Questo popola `src/micro_ros_agent/` e `src/micro_ros_msgs/` (ignorati da git, gestiti da `microros.repos`).

### 3. Risolvi le dipendenze ROS

```bash
source /opt/ros/humble/setup.bash
rosdep install --from-paths src --ignore-src -y
```

### 4. Build

```bash
colcon build --symlink-install
```

La prima build dell'agent può richiedere alcuni minuti (clona e compila Micro-XRCE-DDS-Agent come ExternalProject CMake).

---

## Avvio

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch drone_bringup drone.launch.py
```

Argomenti opzionali:
```bash
ros2 launch drone_bringup drone.launch.py uros_port:=8888 uros_verbose:=4 fg_port:=8765
```

A bordo del drone, in `idf.py menuconfig` → `Drone — micro-ROS / WiFi`, imposta `micro-ROS Agent IP` con l'IP della macchina che esegue il launch (es. `192.168.1.9`).

---

## Verifica

In altre shell:
```bash
source /opt/ros/humble/setup.bash
ros2 topic list
ros2 topic hz /drone_1/imu/raw
ros2 topic echo /drone_1/imu/raw --once
```

Foxglove Studio (Windows o nativo): `Open Connection → Foxglove WebSocket → ws://localhost:8765`.

---

## Strumenti — `tools/`

### `motor_steps.py` — sequenza di duty senza buchi

`ros2 topic pub` non può cambiare valore senza Ctrl+C e rilancio (> 500 ms → il watchdog di `task_motors` azzera i motori, che ripartono da fermo). Questo script tiene un publisher unico a 10 Hz e attraversa i gradini in sequenza. Serve per i test di alimentazione: distingue l'inrush da fermo (rampa passa, gradino secco 0→30% no) dal picco a regime (resetta anche l'ultimo gradino della rampa).

```bash
source /opt/ros/humble/setup.bash
python3 tools/motor_steps.py "10,10,10,0:3" "20,20,20,0:3" "30,30,30,0:5"
```

Argomenti `"FL,RL,RR,FR:secondi"` (secondi opzionali, default 3). Opzioni `--ns drone_1`, `--rate 10`. A fine sequenza o a Ctrl+C pubblica `[0,0,0,0]`. Esce con errore se nessun subscriber aggancia il topic entro 5 s (agent giù). **Pubblica appena parte: switch arm e drone pronti prima di lanciarlo.**

---

## Aggiornare i sorgenti di terze parti

```bash
cd ros2_ws
vcs pull src
colcon build --symlink-install
```

## Pulire la build

```bash
rm -rf build install log
```
