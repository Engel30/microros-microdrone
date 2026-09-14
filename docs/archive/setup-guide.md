# Guida Setup Ambiente di Sviluppo

**Data:** 2026-03-19
**Sistema:** Windows 11 + WSL2 (Ubuntu 22.04)

---

## 1. USB Passthrough (Windows → WSL)

L'ESP32-S3 è collegato via USB a Windows. WSL2 non vede le porte USB di default, serve `usbipd`.

### Installazione usbipd (una tantum, da PowerShell Admin su Windows)

```powershell
winget install usbipd
```

Dopo l'installazione, **chiudere e riaprire** PowerShell (il PATH si aggiorna solo nelle nuove sessioni).

### Collegare USB a WSL (da fare ogni volta che si ricollega l'ESP32)

Da **PowerShell Admin** su Windows:

```powershell
# Lista dispositivi USB
usbipd list

# Identifica l'ESP32-S3: VID 303a, PID 1001, BUSID 1-6 (può variare)

# Bind (una tantum per device)
usbipd bind --busid 1-6

# Attach a WSL (da rifare ogni volta che si scollega/ricollega)
usbipd attach --wsl --busid 1-6
```

### Verifica su WSL

```bash
ls /dev/ttyACM0    # Deve esistere
groups $USER       # Deve contenere "dialout"
```

Se l'utente non è nel gruppo dialout:
```bash
sudo usermod -aG dialout $USER
# Poi riavviare WSL
```

---

## 2. Installazione ESP-IDF v5.4

### Dipendenze sistema

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip \
    python3-venv python3.10-venv cmake ninja-build ccache libffi-dev \
    libssl-dev dfu-util libusb-1.0-0
```

### Clone e installazione

```bash
mkdir -p ~/esp && cd ~/esp
git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git

cd ~/esp/esp-idf
./install.sh esp32s3
```

### Attivazione ambiente

Da eseguire **ogni volta** che si apre un nuovo terminale:

```bash
. ~/esp/esp-idf/export.sh
```

Per renderlo automatico:

```bash
echo '' >> ~/.bashrc
echo '# ESP-IDF' >> ~/.bashrc
echo '. $HOME/esp/esp-idf/export.sh' >> ~/.bashrc
```

### Verifica

```bash
idf.py --version
```

---

## 3. ROS2 (già installato)

ROS2 era già presente su WSL prima di questo setup. Serve per:
- `micro-ros-agent` (bridge tra ESP32 e rete ROS2)
- `ros2 topic echo` (debug)
- Foxglove Studio (visualizzazione)

---

## 4. Troubleshooting

### `python3-venv` non trovato
```bash
sudo apt install -y python3.10-venv
```
Poi rilanciare `./install.sh esp32s3`.

### USB non visibile dopo attach
- Verificare che l'ESP32 sia fisicamente collegato
- Rifare `usbipd attach --wsl --busid 1-6` da PowerShell Admin
- Il BUSID può cambiare se si usa una porta USB diversa: controllare con `usbipd list`

### `idf.py` non trovato
L'ambiente non è attivato. Lanciare:
```bash
. ~/esp/esp-idf/export.sh
```

### Errori pip dependency conflicts
Warning tipo "pip's dependency resolver does not currently take into account..." sono **non bloccanti** e possono essere ignorati.

### Monitor seriale non si connette dopo flash
Il `flash` esegue un hard reset dell'ESP32, che scollega la USB da WSL. Bisogna:
1. Rifare `usbipd attach --wsl --busid 1-6` da PowerShell Admin
2. Rilanciare `idf.py -p /dev/ttyACM0 monitor`

---

## 5. Verifica toolchain (test eseguito 2026-03-19)

```bash
cp -r ~/esp/esp-idf/examples/get-started/hello_world /tmp/hello_world
cd /tmp/hello_world
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Output atteso:
```
Hello world!
This is esp32s3 chip with 2 CPU core(s), WiFi/BLE, silicon revision v0.2, 2MB external flash
Minimum free heap size: 390340 bytes
```

**Stato: VERIFICATO OK**
