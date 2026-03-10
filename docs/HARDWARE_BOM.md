# Bill of Materials (BOM): ESP32-S3 Aero-Edu Drone

**Filosofia:** Inizio con Moduli su Millefori -> Evoluzione a Custom PCB.
**Target:** Massima sicurezza e stabilità di volo.

---

## 1. Elettronica Principale (Cervello e Sensi)
*Prendi questi moduli completi (Breakout Boards).*

| Componente | Specifiche | Qtà | Note |
| :--- | :--- | :--- | :--- |
| **MCU Board** | **Seeed Studio XIAO ESP32S3** | 1 | Versione standard (NO Sense). |
| **IMU (Assetto)** | **Modulo GY-521 (MPU-6050)** | 1 | Accelerometro + Giroscopio (I2C). **Alimentare a 3.3V** (~5mA). |
| **Optical Flow + ToF** | **Modulo Integrato (tipo Matek 3901-L0X)** | 1 | **UART (MSP Protocol).** Include PMW3901 + VL53L0X. **TASSATIVO 3.3V** (~40mA). NO 5V! |
| **Buzzer** | **Buzzer Attivo 3.3V / 5V** | 1 | Piccolo (diametro 9mm). Fondamentale per allarmi batteria. |

---

## 2. Meccanica e Propulsione
*Il "corpo" del drone.*

| Componente | Specifiche | Qtà | Note |
| :--- | :--- | :--- | :--- |
| **Telaio (Frame)** | **75mm Whoop Frame** | 1 | BetaFPV 75 Pro o simili (plastica). |
| **Motori** | **8520 Coreless Brushed** | 4 | 3.7V (1S). Prendi 1-2 di scorta. |
| **Eliche** | **40mm (4-pale)** | 2 Set | Foro 1.0mm. Prendine molte (si rompono!). |
| **Batteria** | **LiPo 1S 450-550mAh** | 2-3 | Connettore **BT2.0** (Obbligatorio). |
| **Viteria** | **Viti M1.2 x 4mm** | 10+ | Per fissare la scheda al frame. |

---

## 3. Circuito di Potenza e Monitoraggio
*Componenti sfusi da saldare sulla millefori.*

| Componente | Valore/Sigla | Qtà | Scopo |
| :--- | :--- | :--- | :--- |
| **MOSFET** | **SI2302** (SOT-23) | 5-10 | Driver motori. |
| **Diodo** | **1N5819** (THT) | 10 | Flyback protezione motori. |
| **Condensatore** | **470uF / 6.3V** | 1 | Bulk capacitor anti-reset. |
| **Resistenza** | **10k Ohm** | 10 | Pull-down MOSFET e partitore. |
| **Resistenza** | **100k Ohm** | 2 | **V-Sense (Monitoraggio Batteria)** | Creano un partitore (R1+R2) per leggere i 4.2V della batteria su un pin ADC (max 3.3V). |
| **Switch** | **Mini Slide Switch** | 1 | Interruttore On/Off fisico. |
| **Resistenza** | **220 Ohm** | 5 | Protezione Gate MOSFET. |

---

## 4. Connettori e Cablaggio
*Per rendere il drone modulare e riparabile.*

| Componente | Tipo | Note |
| :--- | :--- | :--- |
| **Machine Pin Headers** | Femmina (Tulipano) | Per incastrare i sensori sulla millefori (anti-vibrazione). |
| **Standard Pin Headers** | Maschio | Da saldare sui moduli sensori. |
| **Cavo Batteria** | **BT2.0 (Maschio)** | Pigtail con cavi in silicone da saldare alla scheda. |
| **Millefori** | FR4 Double-Sided | Verde, 5x7cm (da tagliare a circa 4.5x4.5cm). |
| **Fili Silicone** | 30AWG (Segnali) | Per collegare i motori alla scheda (flessibili). |
| **Fili Silicone** | 24AWG (Power) | Per batteria e tethering (regge corrente alta). |

---

## 5. Power Management (Tethering & Ricarica)
*Indispensabile per sviluppare senza stress.*

| Componente | Nome | Qtà | Funzione |
| :--- | :--- | :--- | :--- |
| **Buck Converter** | **Mini-360** (o LM2596) | 1 | Riduce i 5V USB del Powerbank a 4.2V per il drone. Regolabile. |
| **Caricatore USB** | **Modulo TP4056** (USB-C) | 1 | Piccola scheda per ricaricare le LiPo 1S in sicurezza. |
| **Cavo USB vecchio** | - | 1 | Da tagliare per prelevare i 5V per il Buck Converter. |

---

## 6. Strumenti da avere sul banco
* Se non li hai, aggiungili all'ordine:
1. **Flussante (Flux):** Indispensabile per saldare i MOSFET SOT-23.
2. **Stagno (Lead-free o 60/40):** Qualità buona (es. Kester o Cynel).
3. **Pinzette di precisione:** Per maneggiare i MOSFET.
4. **Multimetro:** Per verificare tensione Buck Converter e cortocircuiti.