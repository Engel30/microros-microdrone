# Hardware: BOM e Connessioni

**Ultimo aggiornamento:** 2026-04-16

---

## 1. Bill of Materials

| Componente | Specifiche | Qtà |
|:-----------|:-----------|:---:|
| **MCU** | Seeed XIAO ESP32-S3 | 1 |
| **IMU** | GY-521 (MPU-6050) | 1 |
| **Optical Flow + ToF** | Matek 3901-L0X clone | 1 |
| **Motori** | 8520 Coreless 3.7V | 4 |
| **Batteria** | LiPo 1S 450-550mAh, BT2.0 | 2-3 |
| **MOSFET** | AO3400A (SOT-23) low-side | 4 |
| **Diodo Flyback** | SS14 (SMA) | 4 |
| **Condensatore Bulk** | 470µF / 16V | 1 |
| **Resistenze Pull-Down Gate** | 10kΩ | 6 |
| **Resistenze Serie Gate** | 100Ω | 4 |
| **Resistenze Partitore Battery** | 100kΩ | 2 |
| **Resistenza LED** | 330Ω | 1 |
| **Condensatori Decoupling** | 100nF | 4 |
| **Buzzer** | 3.3V passivo 4kHz | 1 |
| **LED Status** | 0603 rosso (KT-0603R) | 1 |
| **Switch On/Off** | SMD slide 3P MST-12D18G2 | 1 |
| **Telaio** | 3D print PLA custom | 1 |
| **Eliche** | 40mm 4-pale, foro 1.0mm | 2 set |
| **Perfboard** | 7×5cm FR4 doppia | 1 |
| **Pin header** | 2.54mm femmina/maschio | 10+ |

---

## 2. Pinout XIAO ESP32-S3

| Pin | GPIO | Funzione | Bus |
|:---:|:----:|:---------|:---:|
| **D0** | GPIO_NUM_1 | Motore 1 | PWM LEDC 20kHz |
| **D1** | GPIO_NUM_2 | Motore 2 | PWM LEDC 20kHz |
| **D2** | GPIO_NUM_3 | Motore 3 | PWM LEDC 20kHz |
| **D3** | GPIO_NUM_4 | Motore 4 | PWM LEDC 20kHz |
| **D4** | GPIO_NUM_5 | SDA (IMU) | I2C 400kHz |
| **D5** | GPIO_NUM_6 | SCL (IMU) | I2C 400kHz |
| **D6** | GPIO_NUM_43 | TX Flow | UART 19200 |
| **D7** | GPIO_NUM_44 | RX Flow | UART 19200 |
| **D8** | GPIO_NUM_7 | V-Sense Battery | ADC |
| **D9** | GPIO_NUM_8 | Buzzer | PWM |
| **D10** | GPIO_NUM_9 | LED Status | GPIO |
| **3V3** | Output | Sensori | LDO max 200mA |
| **VUSB** | Input | Batteria 1S | Power Input 4.2V |

---

## 3. Architettura Circuito

**Alimentazione:**
- LiPo 4.2V (BT2.0) → Switch → VUSB (XIAO)
- Condensatore 470µF filtra transitori
- LDO interno: VUSB → 3.3V (alimenta sensori)

**Motori:**
- VBAT (4.2V) → MOSFET low-side (AO3400A) → 8520
- **CRITICO:** Pull-down 10kΩ su ogni gate (evita floating durante boot)
- Diodo flyback SS14 su ogni motore (protezione spike)

**Sensori:**
- IMU (I2C): D4=SDA, D5=SCL @ 400kHz, indirizzo 0x68
- Flow (UART): D6=TX, D7=RX @ 19200 baud, frame CXOF 11-byte
- Battery (ADC): partitore 100k/100k su D8 (legge ~2.1V = 4.2V reali)

**Control:**
- Buzzer: PWM D9 (4kHz)
- LED: GPIO D10 via R 330Ω

---

## 4. Note Critiche

⚠️ **Pull-down 10kΩ obbligatorio su gate MOSFET**
- GPIO flottanti durante boot → MOSFET accesi casualmente → spike dreno → burning ESP32
- Hardware pull-down garantisce GPIO=0 durante power-on reset

⚠️ **VUSB vs USB:** NON collegare batteria e USB contemporaneamente

⚠️ **3.3V max 200mA:** IMU (~5mA) + Flow (~40mA) + Buzzer (50mA peak) = ~45mA margine OK

**Flow scale:** 1.294e-2 rad/count (fattore 7.35× ArduPilot, calibrato empiricamente per clone P3901)

---

## 5. Riferimenti

- Schematico EasyEDA: `docs/pcb-custom/`
- Firmware: `docs/02-FIRMWARE-ARCHITETTURA.md`
