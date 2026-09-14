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
```

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

---

## 6. WiFi e micro-ROS

Nel firmware, aggiungi:
```c
#define WIFI_SSID "tuaRete"
#define WIFI_PASSWORD "password"
```

Scopri IP WSL:
```bash
hostname -I
```

Avvia agent (su PC/WSL):
```bash
micro_ros_agent udp4 --ip 192.168.x.x -p 8888
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
