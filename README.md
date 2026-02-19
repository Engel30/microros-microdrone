# 🛸 Drone Firmware — ESP32-S3 Swarm Platform

> **Stato attuale:** Fase di validazione sensori e stima di posizione.
> Il firmware implementa la lettura di IMU e Optical Flow, la fusione dei dati con compensazione giroscopica e la stima di posizione 2D in tempo reale.

---

## Indice

1. [Panoramica](#panoramica)
2. [Hardware](#hardware)
3. [Struttura del Codice](#struttura-del-codice)
4. [Configurazione (`config.h`, `types.h`)](#configurazione)
5. [Driver IMU — `IMU_Driver`](#driver-imu--imu_driver)
6. [Driver Optical Flow — `OpticalFlow`](#driver-optical-flow--opticalflow)
7. [Loop Principale — `main.cpp`](#loop-principale--maincpp)
8. [Pipeline di Fusione Sensoriale](#pipeline-di-fusione-sensoriale)
9. [Protocollo CXOF (Optical Flow)](#protocollo-cxof-optical-flow)
10. [Calibrazione e Comandi Seriali](#calibrazione-e-comandi-seriali)
11. [Problematiche Note e Rumore](#problematiche-note-e-rumore)
12. [Soluzioni Implementate](#soluzioni-implementate)
13. [Build e Deploy (PlatformIO)](#build-e-deploy-platformio)
14. [TODO e Roadmap](#todo-e-roadmap)

---

## Panoramica

Questo firmware è la base del progetto **ESP32-S3 Drone Swarm**, una piattaforma educativa per sciami di micro-droni. L'obiettivo finale è il volo autonomo coordinato via WiFi/ESP-NOW, ma la fase attuale si concentra sulla **validazione dei sensori** e sulla **stima della posizione** a terra (test bench).

**Cosa fa il firmware oggi:**

- Legge l'IMU (`MPU6050`) via I2C → accelerometro, giroscopio, angoli roll/pitch/yaw.
- Legge l'Optical Flow (clone `PMW3901` + `VL53L1X`) via UART → spostamento pixel, distanza ToF.
- Fonde i dati in una posizione 2D (`fused_x`, `fused_y`) compensata per rotazione e tilt.
- Offre una posizione "raw" (solo flow, senza compensazione) per confronto.
- Espone comandi seriali per reset, debug, trace frame-per-frame e calibrazione scala.

---

## Hardware

### Microcontrollore

| Parametro     | Valore                             |
|---------------|------------------------------------|
| **Board**     | Seeed XIAO ESP32-S3                |
| **MCU**       | ESP32-S3 Dual Core (Xtensa LX7)   |
| **Framework** | Arduino (via PlatformIO)           |
| **Clock**     | 240 MHz                            |
| **Flash**     | 8 MB                              |
| **PSRAM**     | 8 MB (opzionale)                   |

### Sensori

| Sensore              | Modello                  | Interfaccia | Pin ESP32-S3   | Note                                    |
|----------------------|--------------------------|-------------|----------------|-----------------------------------------|
| **IMU** (6-DoF)      | MPU6050                  | I2C         | SDA=`D4`, SCL=`D5` | Accelerometro + Giroscopio           |
| **Optical Flow**     | PMW3901 (clone Thinary) | UART        | RX=`D7`, TX=`D6`   | Flusso ottico (spostamento pixel XY) |
| **ToF** (altimetro)  | VL53L1X (integrato)     | UART (stesso frame) | — (integrato nel modulo flow) | Range 0–1200 mm |

### Connessioni

```
ESP32-S3 (Seeed XIAO)
┌──────────┐
│     D4 ──┼──── SDA (MPU6050, pullup 4.7kΩ)
│     D5 ──┼──── SCL (MPU6050, pullup 4.7kΩ)
│     D6 ──┼──── TX ESP → RX Modulo Flow
│     D7 ──┼──── RX ESP ← TX Modulo Flow
│    3V3 ──┼──── VCC sensori
│    GND ──┼──── GND comune
│    USB ──┼──── Serial Monitor (115200 baud)
└──────────┘
```

> ⚠️ L'alimentazione dei sensori è a **3.3V**. Il modulo MPU6050 con breakout board potrebbe avere un regolatore integrato per 5V, ma è preferibile usare 3.3V diretto se la board lo supporta.

---

## Struttura del Codice

```
drone-firmware/
├── platformio.ini              # Configurazione build PlatformIO
├── include/
│   ├── config.h                # Pin, baud rate, parametri globali
│   └── types.h                 # Struct SensorData condivisa
├── lib/
│   ├── IMU_Driver/
│   │   ├── IMU_Driver.h        # Interfaccia driver IMU
│   │   └── IMU_Driver.cpp      # Implementazione (MPU6050 + yaw manuale)
│   └── Optical_Flow/
│       ├── OpticalFlow.h       # Interfaccia driver optical flow
│       └── OpticalFlow.cpp     # Parser protocollo CXOF + lettura sensore
├── src/
│   └── main.cpp                # Setup, loop principale, fusione, comandi
└── README.md                   # Questo file
```

### Dipendenze esterne

| Libreria                  | Versione | Uso                             |
|---------------------------|----------|---------------------------------|
| `rfetick/MPU6050_light`   | ^1.1.0   | Lettura MPU6050 con filtro complementare |

Installata automaticamente da PlatformIO tramite `lib_deps` in `platformio.ini`.

---

## Configurazione

### `config.h` — Parametri globali

```cpp
const bool ENABLE_DEBUG_PRINTS = true;  // Abilita log debug su seriale

// Pinout
#define PIN_I2C_SDA  D4
#define PIN_I2C_SCL  D5
#define PIN_FLOW_TX  D6   // TX dell'ESP -> RX del Modulo
#define PIN_FLOW_RX  D7   // RX dell'ESP <- TX del Modulo

// Sensori
#define IMU_UPDATE_RATE_HZ  100
#define FLOW_UART_BAUD      19200   // Clone PMW3901+VL53L1X, protocollo CXOF esteso
```

Il baud rate di `19200` è specifico per il clone Thinary del modulo Matek 3901-L0X. Il modulo Matek originale usa il protocollo MSP a 115200 baud.

### `types.h` — Struttura dati condivisa

Definisce `SensorData`, la struttura centrale usata per passare i dati tra i driver e il loop principale:

```cpp
struct SensorData {
    // IMU (MPU6050)
    float ax, ay, az;        // Accelerometro (g)
    float gx, gy, gz;        // Giroscopio (deg/s)
    float pitch, roll, yaw;  // Angoli stimati (deg)
    float temp;              // Temperatura chip (°C)

    // Optical Flow (PMW3901)
    int16_t flow_x_raw;      // Spostamento pixel X (counts)
    int16_t flow_y_raw;      // Spostamento pixel Y (counts)
    uint8_t flow_quality;    // Qualità lettura ottica (0-255)

    // Time of Flight (VL53L1X integrato)
    int32_t range_mm;        // Distanza dal suolo (mm)
};
```

---

## Driver IMU — `IMU_Driver`

### File: `lib/IMU_Driver/IMU_Driver.h`, `IMU_Driver.cpp`

Wrapper attorno alla libreria `MPU6050_light` con **integrazione manuale dello yaw**.

### Interfaccia pubblica

| Metodo               | Descrizione                                                  |
|----------------------|--------------------------------------------------------------|
| `begin(sda, scl)`   | Inizializza I2C, connette MPU6050, esegue calibrazione (3s) |
| `update()`           | Legge sensori e integra yaw manualmente                      |
| `getData(SensorData&)` | Riempie la struct con i dati correnti                     |
| `resetYaw()`         | Azzera lo yaw accumulato                                     |
| `calibrate()`        | Ricalibra offsets accel+gyro (fermo e in piano, 2s delay)    |

### Dettagli implementativi

**Filtro complementare** — La libreria `MPU6050_light` calcola roll e pitch con un filtro complementare configurabile. Il coefficiente è impostato a **0.995** (default 0.98):

```cpp
mpu->setFilterGyroCoef(0.995f);
```

Un valore più alto dà più peso al giroscopio, rendendo roll/pitch molto stabili durante traslazioni rapide a scapito di una convergenza più lenta.

**Yaw manuale** — Lo yaw **non** viene preso dalla libreria ma integrato manualmente nel metodo `update()`:

```cpp
float gz = mpu->getGyroZ();        // deg/s
if (abs(gz) > 0.2)                 // deadzone
    _my_yaw += gz * dt;
```

Questo per evitare la **deadzone interna** della libreria `MPU6050_light`, che risulta troppo aggressiva e "mangia" le rotazioni lente. La deadzone custom di `0.2 deg/s` è un compromesso tra filtraggio del rumore e reattività.

**Calibrazione** — Al boot viene eseguita una calibrazione automatica degli offset di accelerometro e giroscopio (`calcOffsets(true, true)`). L'utente ha 3 secondi per posare il sensore piatto e fermo. I valori di offset vengono stampati su seriale per debug.

**Timeout I2C** — Impostato a 10 ms per evitare hang del bus in caso di disconnessione:

```cpp
_wire.setTimeOut(10);
```

---

## Driver Optical Flow — `OpticalFlow`

### File: `lib/Optical_Flow/OpticalFlow.h`, `OpticalFlow.cpp`

Driver custom per il modulo clone PMW3901 + VL53L1X (Thinary). Implementa il parsing del **protocollo CXOF esteso** via UART.

### Interfaccia pubblica

| Metodo           | Tipo ritorno | Descrizione                                    |
|------------------|-------------|------------------------------------------------|
| `begin(baud, rx, tx)` | `void` | Inizializza UART con pin e baud rate configurati |
| `update()`       | `void`      | Legge e parsa tutti i byte disponibili         |
| `hasNewData()`   | `bool`      | `true` se c'è un frame nuovo (auto-reset)      |
| `getFlowX()`     | `int16_t`   | Spostamento pixel asse X (counts)              |
| `getFlowY()`     | `int16_t`   | Spostamento pixel asse Y (counts)              |
| `getDistance()`   | `int32_t`   | Distanza ToF dal suolo (mm)                    |
| `getQuality()`   | `uint8_t`   | Qualità superficie (0-255)                     |

### State Machine di parsing

Il metodo `update()` implementa una macchina a stati byte-per-byte:

```
Stato 0: Attendi header (0xFE)
Stato 1: Verifica protocol ID (0x04)
         → se diverso, controlla se è un nuovo header (0xFE come dato checksum)
Stato 2-10: Accumula byte nel buffer
         → a 11 byte verifica footer (0xAA) e checksum
```

**Gestione sincronizzazione persa**: Se il byte 1 non è `0x04`, il parser controlla se il byte è `0xFE` (possibile nuovo header). Questo gestisce il caso in cui il byte `0xFE` appaia come valore di checksum nel frame precedente.

---

## Loop Principale — `main.cpp`

Il file `main.cpp` è la versione **v3** del test di posizione e implementa il ciclo:

```
setup() → Inizializza IMU e Flow, stampa banner comandi

loop() →
  1. Gestione comandi seriali (r/d/t/c/m)
  2. Aggiornamento IMU + accumulo dt giroscopico
  3. Aggiornamento Optical Flow + fusione posizione
  4. Stampa periodica (ogni 200ms)
```

### Variabili di stato globali

| Variabile            | Tipo     | Descrizione                                         |
|----------------------|----------|-----------------------------------------------------|
| `raw_x`, `raw_y`    | `float`  | Posizione senza compensazione (solo flow × scala × h) |
| `fused_x`, `fused_y`| `float`  | Posizione con compensazione gyro, tilt e yaw         |
| `gyro_x_acc_rad`     | `float`  | Accumulatore gyro X tra frame flow (rad)             |
| `gyro_y_acc_rad`     | `float`  | Accumulatore gyro Y tra frame flow (rad)             |
| `FLOW_SCALE`         | `float`  | Fattore di conversione counts → angolo (rad/count)   |
| `FOCAL_COUNTS`       | `float`  | Costante fisica dell'ottica del sensore (counts)     |

### Costanti critiche

| Costante       | Valore    | Unità      | Significato                                          |
|----------------|-----------|------------|------------------------------------------------------|
| `FLOW_SCALE`   | 0.00282   | rad/count  | Scala calibrabile: converte counts in angolo, poi in mm |
| `FOCAL_COUNTS` | 568.0     | counts     | Lunghezza focale ottica in pixel (1/1.76e-3). **Non** cambia con la calibrazione |
| `MIN_QUALITY`  | 30        | —          | Qualità minima per accettare un frame flow            |
| `DEG2RAD`      | π/180     | rad/deg    | Conversione gradi-radianti                            |

> **Nota fondamentale**: `FLOW_SCALE` e `FOCAL_COUNTS` sono due concetti separati. `FLOW_SCALE` è la conversione complessiva counts→mm calibrata empiricamente. `FOCAL_COUNTS` è la costante fisica dell'ottica del sensore, usata **solo** per la compensazione giroscopica (convertire l'angolo del gyro in counts equivalenti). Aggiornare `FOCAL_COUNTS` con la calibrazione romperebbe la compensazione.

---

## Pipeline di Fusione Sensoriale

La stima di posizione avviene in due fasi parallele ad ogni frame flow valido:

### 1. Posizione RAW (senza compensazione)

```
raw_x += flow_x_raw × FLOW_SCALE × h
raw_y += flow_y_raw × FLOW_SCALE × h
```

Dove `h = distanza_ToF_mm`. Nessuna compensazione rotazionale — serve come baseline di confronto.

### 2. Posizione FUSED (compensata)

La pipeline fused è un'odometria ottica compensata in 4 step:

#### Step A — Compensazione gyro (rimozione rotazione)

```
fx_comp = flow_x_raw + gyro_y_acc × FOCAL_COUNTS
fy_comp = flow_y_raw + gyro_x_acc × FOCAL_COUNTS
```

Il giroscopio ha un rate di aggiornamento molto più alto del flow (~100Hz vs ~20Hz). Tra un frame flow e l'altro, il contributo rotazionale del giroscopio viene **accumulato** (`gyro_x_acc_rad`, `gyro_y_acc_rad`) e poi sommato al flow in counts equivalenti. Dopo ogni frame, gli accumulatori vengono azzerati.

**Conversione assi**: `gyro_y → flow_x` e `gyro_x → flow_y` (assi incrociati, standard ottico).

**Deadzone gyro**: Contributi sotto `0.2 deg/s` vengono ignorati per ridurre il drift.

#### Step B — Correzione altezza per tilt

```
h_corr = h × cos(roll) × cos(pitch)
```

Se il drone è inclinato, la distanza ToF verticale non corrisponde alla vera altezza dal suolo. Si corregge proiettando la distanza sulla verticale.

#### Step C — Conversione counts → mm nel body frame

```
dx_body = fx_comp × FLOW_SCALE × h_corr
dy_body = fy_comp × FLOW_SCALE × h_corr
```

#### Step D — Rotazione body → world frame (yaw)

```
fused_x += dx_body × cos(yaw) - dy_body × sin(yaw)
fused_y += dx_body × sin(yaw) + dy_body × cos(yaw)
```

Lo yaw dell'IMU ruota il vettore spostamento dal frame del drone al frame mondo.

### Diagramma di flusso

```
                   ┌───────────────┐
                   │  MPU6050 IMU  │
                   │  (I2C, 100Hz) │
                   └───────┬───────┘
                           │
            ┌──────────────┼──────────────┐
            │              │              │
        gyro X/Y/Z    roll, pitch      yaw
        (deg/s)        (deg)          (deg)
            │              │              │
      ┌─────▼─────┐       │              │
      │ Accumulo   │       │              │
      │ dt tra     │       │              │
      │ frame flow │       │              │
      └─────┬─────┘       │              │
            │              │              │
   ┌────────▼──────────────▼──────────────▼───────────┐
   │                                                   │
   │  ┌─────────────┐     FUSIONE (ogni frame flow)    │
   │  │ PMW3901     │                                  │
   │  │ + VL53L1X   │──► flow_x/y (counts)            │
   │  │ (UART 20Hz) │──► distanza (mm)                 │
   │  │             │──► qualità (0-255)               │
   │  └─────────────┘                                  │
   │                                                   │
   │  Step A: flow_comp = flow_raw + gyro_acc × FOCAL  │
   │  Step B: h_corr = dist × cos(roll) × cos(pitch)  │
   │  Step C: d_body = flow_comp × SCALE × h_corr     │
   │  Step D: d_world = R(yaw) × d_body               │
   │                                                   │
   │  → fused_x += d_world.x                          │
   │  → fused_y += d_world.y                          │
   └───────────────────────────────────────────────────┘
```

---

## Protocollo CXOF (Optical Flow)

Il modulo clone Thinary PMW3901 + VL53L1X utilizza un protocollo binario proprietario denominato **CXOF esteso**, diverso dal protocollo MSP usato dal Matek originale.

### Formato frame (11 byte)

```
Byte:  0     1     2     3     4     5     6     7     8     9     10
     ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
     │ HDR │ PID │ X_L │ X_H │ Y_L │ Y_H │ D_L │ D_H │ CK  │ SQ  │ FTR │
     └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
```

| Byte  | Campo         | Valore/Tipo       | Descrizione                              |
|-------|---------------|-------------------|------------------------------------------|
| 0     | Header        | `0xFE`            | Inizio frame                             |
| 1     | Protocol ID   | `0x04`            | Identificativo CXOF                      |
| 2-3   | Flow X        | `int16_t` (LE)    | Spostamento pixel asse X (counts)        |
| 4-5   | Flow Y        | `int16_t` (LE)    | Spostamento pixel asse Y (counts)        |
| 6-7   | Distance      | `uint16_t` (LE)   | Distanza ToF in **centimetri**           |
| 8     | Checksum      | `uint8_t`         | Somma byte 2-7                           |
| 9     | Quality       | `uint8_t`         | Qualità superficie (0-255)               |
| 10    | Footer        | `0xAA`            | Fine frame                               |

> **Nota**: La distanza è trasmessa in **centimetri** e convertita in **millimetri** dal driver (`× 10`).

### Configurazione UART

| Parametro     | Valore   |
|---------------|----------|
| Baud Rate     | 19200    |
| Data Bits     | 8        |
| Parity        | None     |
| Stop Bits     | 1        |
| Byte Order    | Little-Endian |

### Validazione frame

Un frame è accettato solo se **tutte** le condizioni sono soddisfatte:

1. Header `== 0xFE`
2. Protocol ID `== 0x04`
3. Footer (byte 10) `== 0xAA`
4. Checksum (somma byte 2-7) `== byte 8`

---

## Calibrazione e Comandi Seriali

Il firmware espone comandi interattivi via Serial Monitor (115200 baud):

| Comando | Azione                                                                |
|---------|-----------------------------------------------------------------------|
| `r`     | **Reset** posizione (raw + fused) e yaw a zero                       |
| `d`     | **Debug toggle** — mostra accel, scala, focale, gyro_acc, h_corr     |
| `t`     | **Trace toggle** — stampa ogni singolo frame flow con tutti i valori  |
| `c`     | **Calibra IMU** — ricalcola offset gyro+accel (tenere fermo 2s)      |
| `m`     | **Misura/Calibra scala** — procedura di calibrazione del FLOW_SCALE  |

### Procedura di calibrazione scala (`m`)

1. Premere `m` → inizio misura (salva posizione corrente)
2. Muovere il sensore di esattamente **100 mm** in linea retta
3. Premere `m` → fine misura
4. Il firmware calcola la distanza misurata e applica un fattore di correzione:

```
correzione = 100.0 / distanza_misurata
FLOW_SCALE *= correzione
```

Se la distanza è troppo piccola (< 5 mm), la calibrazione viene scartata. Il valore corretto di `FLOW_SCALE` va aggiornato manualmente nel sorgente.

> ⚠️ `FOCAL_COUNTS` **non viene mai aggiornato** dalla calibrazione: è una costante fisica dell'ottica del sensore.

### Output seriale (ogni 200ms)

```
POS  fused(+123.4,  -56.7)  raw(+130.2,  -60.1) mm
FLOW cnt/frame: fx=+3 fy=-1  |  dist=150 mm  qual=80  |  frames=4 skip=0
IMU  roll=+1.2  pitch=-0.5  yaw=+45.3  |  gyro(+0.1,-0.2,+0.0) dps
---
```

In **debug mode** (`d`), si aggiungono:

```
DBG  scala=0.00282  focale_fisica=568.0  |  accel(+0.01,-0.02,+1.00) g  temp=26.5C
DBG  gyro_acc: x=+0.00012 y=-0.00003 rad  |  h_corr=149.8 mm
```

In **trace mode** (`t`), si stampa ogni singolo frame flow:

```
T fx=+3 fy=-1 | gcx=+0.5 gcy=-0.2 | fxc=+3.5 fyc=-1.2 | dx=+1.48 dy=-0.51 | pos(+123.4,-56.7)
```

---

## Problematiche Note e Rumore

### 1. Deadzone Yaw della libreria `MPU6050_light`

**Problema**: La libreria `MPU6050_light` applica internamente una deadzone sul giroscopio per il calcolo dello yaw. Questa deadzone è troppo aggressiva e filtra via rotazioni lente ma reali, causando una perdita di angolo durante manovre graduali.

**Soluzione**: Lo yaw viene **integrato manualmente** nel driver (`IMU_Driver::update()`) con una deadzone custom di `0.2 deg/s`, molto più blanda di quella della libreria. Roll e pitch continuano a usare il filtro complementare della libreria.

### 2. Drift dello yaw (assenza di magnetometro)

**Problema**: L'MPU6050 è un sensore a 6-DoF (accel + gyro), senza magnetometro. Lo yaw integrato dal giroscopio è soggetto a **drift cumulativo** nel tempo, senza riferimento assoluto al Nord.

**Impatto**: Per test brevi (< 30s) il drift è trascurabile. Per sessioni lunghe, lo yaw può accumulare errori significativi.

**Mitigazione attuale**: Deadzone a `0.2 deg/s` per ridurre il drift da rumore statico. Reset manuale con comando `r`.

### 3. Clone Thinary vs Matek originale

**Problema**: Il modulo Optical Flow utilizzato è un clone Thinary del Matek 3901-L0X. Il clone **non usa il protocollo MSP** standard di Matek, ma un protocollo binario proprietario (CXOF esteso) a 19200 baud.

**Conseguenze**:
- Il baud rate è diverso (19200 vs 115200 del MSP)
- Il formato frame è diverso (11 byte CXOF vs MSP variabile)
- La documentazione del clone è inesistente — il protocollo è stato reverse-engineered

### 4. Qualità ottica e superfici

**Problema**: Il sensore ottico PMW3901 richiede una superficie con **texture** sufficiente. Superfici lucide, uniformi o troppo scure danno qualità 0 e nessun dato utile.

**Mitigazione**: Il firmware scarta frame con qualità `< 30 (MIN_QUALITY)` e distanza `< 10 mm`.

### 5. Accoppiamento assi gyro-flow

**Problema**: Il flusso ottico percepisce come "spostamento" anche le rotazioni del drone. Se il drone ruota ma non trasla, il flow registra comunque counts.

**Soluzione implementata**: Compensazione gyroscopica (Step A della pipeline fused). Vedi [Pipeline di Fusione](#pipeline-di-fusione-sensoriale).

### 6. Errore altezza durante tilt

**Problema**: Quando il drone è inclinato, il sensore ToF non misura la vera altezza dal suolo ma la distanza lungo il suo asse (maggiore).

**Soluzione implementata**: Correzione coseno dell'altezza (Step B della pipeline fused).

### 7. Rate mismatch IMU vs Flow

**Problema**: L'IMU aggiorna a ~100 Hz, l'optical flow a ~20 Hz. I dati giroscopici tra un frame flow e l'altro devono essere accumulati, non campionati all'istante del frame.

**Soluzione implementata**: Accumulatori `gyro_x_acc_rad` e `gyro_y_acc_rad` che integrano il contributo del giroscopio tra i frame flow e vengono azzerati dopo ogni frame processato.

---

## Soluzioni Implementate — Riepilogo

| Problema                       | Soluzione                                           | Dove                  |
|-------------------------------|-----------------------------------------------------|-----------------------|
| Deadzone yaw troppo aggressiva | Integrazione yaw manuale con deadzone custom (0.2°/s) | `IMU_Driver.cpp`     |
| Drift yaw (no magnetometro)   | Deadzone + reset manuale                             | `IMU_Driver.cpp`, `main.cpp` |
| Protocollo clone non documentato | Parser CXOF custom reverse-engineered               | `OpticalFlow.cpp`    |
| Rotazione spuria nel flow      | Compensazione giroscopica (gyro → counts equivalenti) | `main.cpp` (Step A)  |
| Errore ToF durante tilt        | Correzione coseno h × cos(roll) × cos(pitch)        | `main.cpp` (Step B)  |
| Rate mismatch IMU/flow         | Accumulatori gyro tra frame flow                     | `main.cpp`           |
| Frame flow rumorosi            | Filtro qualità (MIN_QUALITY=30) + distanza minima    | `main.cpp`           |
| Sincronizzazione UART persa    | Re-sync su header 0xFE anche da checksum             | `OpticalFlow.cpp`    |
| Scala flow da calibrare        | Procedura calibrazione interattiva (comando `m`)     | `main.cpp`           |
| Filtro complementare troppo lento | Coefficiente gyro 0.995 (più peso al gyro)        | `IMU_Driver.cpp`     |

---

## Build e Deploy (PlatformIO)

### `platformio.ini`

```ini
[env:seeed_xiao_esp32s3]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
monitor_speed = 115200

lib_deps =
    rfetick/MPU6050_light @ ^1.1.0

build_flags =
    -D CORE_DEBUG_LEVEL=0
    -I include
```

### Comandi

```bash
# Build
pio run

# Upload
pio run --target upload

# Serial Monitor
pio device monitor
```

### Quick Start

1. Apri la cartella `drone-firmware` in VS Code con PlatformIO
2. Collega la Seeed XIAO ESP32-S3 via USB-C
3. Premi **Upload** (→ in basso nella barra PlatformIO)
4. Apri il **Serial Monitor** (🔌 in basso)
5. Invia `r` per resettare, `d` per debug, `t` per trace

---

## TODO e Roadmap

- [ ] **PID Controller**: Implementare loop di stabilizzazione roll/pitch/yaw
- [ ] **ESC Driver**: Driver per ESC brushless (PWM/DSHOT)
- [ ] **ESP-NOW Communication**: Protocollo di comunicazione inter-drone
- [ ] **WiFi Telemetry**: Streaming dati verso ground station
- [ ] **Magnetometro esterno**: Aggiungere un magnetometro (es. QMC5883L) per yaw assoluto
- [ ] **Altitude Hold**: PID su distanza ToF per mantenimento quota
- [ ] **Complementary Filter avanzato**: Fusione IMU completa con magnetometro (Madgwick/Mahony)
- [ ] **Logging su SD/SPIFFS**: Salvataggio dati sensori per analisi offline
- [ ] **OTA Updates**: Aggiornamento firmware via WiFi
