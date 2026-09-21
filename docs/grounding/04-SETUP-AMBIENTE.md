# Setup: Ambiente Sviluppo

**Piattaforma:** Windows 11 + WSL2 (Ubuntu 22.04)

---

## 1. USB Passthrough (Windows → WSL)

```powershell
# PowerShell Admin su Windows (una tantum)
winget install usbipd

# Bind device
usbipd list
usbipd bind --busid 1-6

# Ogni volta si ricollega ESP32
usbipd attach --wsl --busid 1-6
```

Verifica su WSL:
```bash
ls /dev/ttyACM0
groups $USER  # deve contenere "dialout"
sudo usermod -aG dialout $USER  # se manca
```

---

## 2. Installazione ESP-IDF v5.4

```bash
# Dipendenze
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip \
    python3-venv python3.10-venv cmake ninja-build ccache libffi-dev \
    libssl-dev dfu-util libusb-1.0-0

# Clone e install
mkdir -p ~/esp && cd ~/esp
git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/esp-idf
./install.sh esp32s3

# Attiva ogni terminale
. ~/esp/esp-idf/export.sh

# Automatico in .bashrc
echo '. $HOME/esp/esp-idf/export.sh' >> ~/.bashrc

# Verifica
idf.py --version
```

---

## 3. ROS2 (già installato)

```bash
source /opt/ros/humble/setup.bash

# Verifica
ros2 --version
```

---

## 4. Build e Flash

```bash
cd ~/microros-microdrone

# Build
idf.py build

# Flash + Monitor
idf.py -p /dev/ttyACM0 flash monitor

# Solo Monitor
idf.py -p /dev/ttyACM0 monitor
# Esci: Ctrl+]

# Monitor "puro" senza toccare la build (se build/ o sdkconfig non ci sono)
python -m serial.tools.miniterm --rts 0 --dtr 0 /dev/ttyACM0 115200
```

> ⚠️ `idf.py monitor` **configura il progetto** se `build/` è vuota. Senza `sdkconfig` prende il target di default `esp32` (sbagliato: la XIAO è `esp32s3`) e lascia una cache CMake avvelenata. Se `sdkconfig` manca, prima ricostruiscilo (§6), oppure usa il miniterm.

Dopo flash, ESP32 resetta USB. Riattiva da PowerShell Admin:
```powershell
usbipd attach --wsl --busid 1-6
```

---

## 5. Troubleshooting

| Problema | Soluzione |
|:---------|:----------|
| `/dev/ttyACM0` non esiste | Rifare `usbipd attach --wsl --busid 1-6` |
| `idf.py` non trovato | Lanciare `. ~/esp/esp-idf/export.sh` |
| USB in uso | `kill $(lsof /dev/ttyACM0 \| tail -1 \| awk '{print $2}')` |
| Python venv error | `sudo apt install -y python3.10-venv` |
| `ERROR: File .component_hash or CHECKSUMS.json for component ... does not exist` | `managed_components/<comp>` svuotato: `rm -rf managed_components/esp-idf-lib__* dependencies.lock` e `idf.py build` li riscarica. **Non** cancellare `micro_ros_espidf_component`: contiene `libmicroros.a` (5–10 min di build) |
| `IDF_TARGET not set, using default target: esp32` | `sdkconfig` mancante. `rm -rf build`, ricostruisci `sdkconfig` (§6), poi `idf.py build` |
| `Agent non risponde` all'infinito con WiFi connesso | IP del PC cambiato (lease DHCP scaduto). Vedi §6 |

---

## 6. WiFi e micro-ROS

SSID, password e IP dell'agent sono opzioni **Kconfig** (`main/Kconfig.projbuild`), salvate in `sdkconfig`:

```bash
idf.py menuconfig      # → "Drone — micro-ROS / WiFi"
grep DRONE_ sdkconfig  # verifica
```

`sdkconfig` è **gitignored** (contiene la password) e viene compilato nel binario. Regole:
- non cancellarlo: senza, `idf.py` rigenera i default `YOUR_SSID`/`YOUR_PASS`/`192.168.1.100`
- se sparisce, ripartire da `sdkconfig.old` (copia automatica dell'ultima versione precedente): `cp sdkconfig.old sdkconfig`, poi correggere SSID/password/IP con `menuconfig` o `sed`
- l'IP dell'agent è **hardcoded nel firmware** → l'IP del PC deve essere **prenotato sul router** (DHCP reservation su MAC), altrimenti dopo una pausa il lease scade, il router lo riassegna a un altro dispositivo e il drone bussa alla porta sbagliata. Lezione del [2026-09-21](../sessions/2026-09-21-alimentazione-18650-e-ripresa.md)

Configurazione attuale (prenotata sul router `LiboHouse`): PC `192.168.1.7`, drone `192.168.1.15`, SSID `"WiFi LiboHouse"` (con lo spazio).

Avvio agent + foxglove bridge (su PC/WSL):
```bash
cd ~/microros-microdrone/ros2_ws
source /opt/ros/humble/setup.bash && source install/setup.bash
ros2 launch drone_bringup drone.launch.py
```

---

## Verifica Completa

Test con hello_world esempio:
```bash
cp -r ~/esp/esp-idf/examples/get-started/hello_world /tmp/hw
cd /tmp/hw
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Output atteso: `Hello world!` + chip info

**Status (2026-03-19): ✅ VERIFICATO**
