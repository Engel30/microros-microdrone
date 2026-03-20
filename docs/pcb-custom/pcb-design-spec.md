# PCB Design Specification: Microdrone Flight Controller v1.0

**Data:** 2026-03-20
**Autore:** Angelo + Claude
**EDA:** EasyEDA → JLCPCB
**Stato:** DRAFT — in revisione

---

## 1. Panoramica

Carrier board a 2 layer per il microdrone 75mm. Ospita i moduli XIAO ESP32-S3 e GY-521 (MPU6050) su pin header, con circuiti driver per 4 motori brushed, protezioni, monitoraggio batteria, e connettori per periferiche esterne.

**Priorità di design:** dimensioni minime e peso minimo.

### Requisiti chiave

| Requisito | Valore |
|-----------|--------|
| Tipo | Carrier board per moduli intercambiabili |
| Layer | 2 (top: componenti + segnali, bottom: ground plane) |
| Componenti | Solo lato top |
| Forma | Rettangolare, frame 3D print su misura della PCB |
| Target dimensioni | ≤45×35mm (da verificare dopo placement) |
| Target peso PCB | ≤3g (FR4 1.0mm) |

---

## 2. Specifiche PCB per produzione

| Parametro | Valore | Note |
|-----------|--------|------|
| Layer | 2 | Top + Bottom |
| Spessore | 1.0mm | Più leggero del 1.6mm standard (−40% peso) |
| Rame | 1 oz (35μm) | Sufficiente per 3A su tracce da 2mm |
| Finitura superficiale | HASL lead-free | Standard JLCPCB, senza sovrapprezzo |
| Solder mask | Entrambi i lati | Colore a scelta |
| Silkscreen | Solo top | Etichette componenti e connettori |
| Min trace width | 0.25mm (segnali), 2mm (power motori) | Vedi sezione 8 |
| Min clearance | 0.2mm | Standard JLCPCB |
| Min via drill | 0.3mm | Pad 0.6mm |
| Fori montaggio | 4× M2 (2.2mm drill) agli angoli | Per fissaggio al frame |
| Board outline | Arrotondare gli angoli (r=1mm) | Evita stress meccanico |

---

## 3. Schema Elettrico Completo

### 3.1 Alimentazione

Tre rail di alimentazione distinte:

| Rail | Tensione | Sorgente | Consumatori | Corrente max |
|------|----------|----------|-------------|-------------|
| VBAT | 3.0-4.2V | BT2.0 → SW1 | XIAO VUSB, ramo motori, V-Sense | ~13A picco |
| VBAT_MOTORS | 3.0-4.2V | VBAT → SW2 | 4× drain MOSFET (via flyback) | ~12A picco |
| 3V3 | 3.3V | XIAO LDO interno | IMU, Flow, buzzer, LED, ADC ref | ~100mA |

**Percorso alimentazione:**

```
LiPo 1S ──[BT2.0 F]──► VBAT ──[JST-PH: SW1 Power]──┬──► XIAO pin VUSB
                                                       │         │
                                                  C1 470μF    LDO interno
                                                       │         │
                                                      GND    3V3 Rail ──► sensori
                                                       │
                                                 [JST-PH: SW2 Arm]
                                                       │
                                                  VBAT_MOTORS ──► motori
```

**Regole:**
- C1 (470μF 6.3V elettrolitico): il più vicino possibile al BT2.0
- SW1 interrompe TUTTA l'alimentazione
- SW2 interrompe SOLO l'alimentazione ai motori (arm/disarm hardware)
- NON collegare mai USB e batteria contemporaneamente

### 3.2 Driver Motori (×4)

Ogni motore ha un circuito identico:

```
                VBAT_MOTORS
                     │
                 ┌───┴───┐
                 │ MOTORE │
                 │  8520  │
                 └───┬───┘
                     │
              D (1N5819)──── catodo a VBAT_MOTORS
              │      │       anodo a Drain
              │ Drain SI2302
              │ Gate ───[R 100Ω]─── GPIOx (ESP32)
              │  │
              │ [R 10kΩ]
              │  │
              Source
              │
             GND
```

**Componenti per motore:**

| Componente | Valore | Package | Funzione |
|-----------|--------|---------|----------|
| Q (MOSFET) | SI2302 | SOT-23 | Low-side switch |
| D (flyback) | 1N5819 | DO-41 (THT) | Protezione spike induttivi |
| R_pulldown | 10kΩ | 0603 SMD | Pull-down gate (OBBLIGATORIA) |
| R_series | 100Ω | 0603 SMD | Limita corrente spike verso GPIO |

**Mappatura motori:**

| Motore | Posizione | GPIO | Pin XIAO | Canale LEDC | Connettore |
|--------|-----------|------|----------|-------------|------------|
| M1 | Front-Left | GPIO_NUM_1 | D0 | CH0 | J_M1 (JST-PH 2) |
| M2 | Rear-Left | GPIO_NUM_2 | D1 | CH1 | J_M2 (JST-PH 2) |
| M3 | Rear-Right | GPIO_NUM_3 | D2 | CH2 | J_M3 (JST-PH 2) |
| M4 | Front-Right | GPIO_NUM_4 | D3 | CH3 | J_M4 (JST-PH 2) |

**Perché ogni componente è necessario:**
- **R_pulldown 10kΩ:** durante il boot (~300ms) i GPIO sono flottanti. Senza pull-down il gate è indeterminato → MOSFET si accende → spike induttivi → distruggono i diodi di clamp interni dell'ESP32 → cortocircuito 3V3-GND permanente. È esattamente quello che ha bruciato il primo ESP32.
- **R_series 100Ω:** limita la corrente di picco durante la commutazione e protegge il GPIO da spike che arrivano dal gate. Rallenta leggermente lo switching ma a 20kHz non è un problema.
- **D flyback 1N5819:** Schottky con bassa Vf (~0.4V). Quando il MOSFET si spegne, la corrente del motore (induttivo) continua a fluire attraverso il diodo invece di generare spike di tensione.

### 3.3 Monitoraggio Batteria (V-Sense)

```
VBAT ──[R1 100kΩ]──┬──[R2 100kΩ]── GND
                    │
                    ├── GPIO_NUM_7 (D8, ADC)
                    │
                   [C_vs 100nF]
                    │
                   GND
```

| Componente | Valore | Package | Funzione |
|-----------|--------|---------|----------|
| R1 | 100kΩ | 0603 SMD | Partitore alto |
| R2 | 100kΩ | 0603 SMD | Partitore basso |
| C_vs | 100nF | 0603 SMD | Filtro anti-aliasing ADC |

- Tensione su D8: VBAT / 2 (range 1.5V–2.1V)
- Consumo partitore: ~42μA a 4.2V (trascurabile)
- C_vs filtra il rumore PWM dei motori dalla lettura ADC

### 3.4 Connettore IMU (GY-521)

Pin header femmina 1×6 (o 1×5 se non usi INT) sulla PCB. Il modulo GY-521 si inserisce sopra.

| Pin header | Collegamento | Note |
|-----------|-------------|------|
| VCC | 3V3 Rail | Il GY-521 ha regolatore onboard, accetta 3.3V diretto |
| GND | GND | |
| SCL | GPIO_NUM_6 (D5) | I2C clock, 400kHz. Pull-up sul modulo GY-521 |
| SDA | GPIO_NUM_5 (D4) | I2C data. Pull-up sul modulo GY-521 |
| XDA | Non collegato | Aux I2C, non usato |
| XCL | Non collegato | Aux I2C, non usato |

- I pull-up I2C (4.7kΩ) sono già sul modulo GY-521, NON aggiungerne sulla PCB
- AD0 del GY-521 va lasciato non collegato (default: indirizzo 0x68)
- INT non collegato (usiamo polling, non interrupt)

**Posizionamento critico:** il GY-521 va il più vicino possibile al centro geometrico della PCB per minimizzare gli errori di leva del giroscopio. Lontano dai motori e dalle tracce di potenza.

### 3.5 Connettore Optical Flow (Matek 3901-L0X)

Pin header maschio 1×4 sulla PCB. Cavo a 4 fili scende sotto il frame.

| Pin | Collegamento | Note |
|-----|-------------|------|
| 1 | 3V3 Rail | TASSATIVO 3.3V — NO 5V, brucia il modulo |
| 2 | GND | |
| 3 | GPIO_NUM_43 (D6, TX1) → RX modulo | ESP32 trasmette, modulo riceve |
| 4 | GPIO_NUM_44 (D7, RX1) ← TX modulo | Modulo trasmette, ESP32 riceve |

- UART1 a 19200 baud, protocollo CXOF
- Il connettore va posizionato al bordo della PCB per far passare il cavo verso il basso

### 3.6 Buzzer

Buzzer passivo 3.3V (12mm) saldato direttamente sulla PCB.

```
GPIO_NUM_8 (D9) ──► Buzzer (+) ── Buzzer (−) ──► GND
```

- Pilotato con PWM per toni variabili
- Il buzzer passivo non ha bisogno di resistenza in serie (è già un carico ad alta impedenza)
- Posizionamento: dove c'è spazio, non è critico

### 3.7 LED Status

Doppia opzione: footprint LED sulla PCB + header 2 pin per LED esterno.

```
                    ┌─── LED onboard (SMD 0603)
GPIO_NUM_9 (D10) ──[R 330Ω]──┤
                    └─── J_LED (pin header 1×2) → LED esterno
```

| Componente | Valore | Package |
|-----------|--------|---------|
| R_led | 330Ω | 0603 SMD |
| LED | Verde/Rosso | 0603 SMD |
| J_LED | Pin header 1×2 | THT 2.54mm |

- Puoi usare il LED onboard, quello esterno, o entrambi in parallelo
- Con 330Ω e 3.3V: ~6mA per LED (luminosità sufficiente)

### 3.8 Condensatori di Disaccoppiamento

| Ref | Valore | Package | Posizione | Funzione |
|-----|--------|---------|-----------|----------|
| C1 | 470μF 6.3V | Elettrolitico radiale Ø8mm | Vicino a BT2.0 | Bulk cap, anti-brownout |
| C2 | 100nF | 0603 SMD | Vicino a pin 3V3 XIAO | Disaccoppiamento rail 3V3 |
| C3 | 100nF | 0603 SMD | Vicino a connettore IMU | Disaccoppiamento IMU |
| C4 | 100nF | 0603 SMD | Vicino a connettore Flow | Disaccoppiamento Flow |
| C_vs | 100nF | 0603 SMD | Vicino a D8 (V-Sense) | Filtro ADC (vedi sez. 3.3) |

I 100nF ceramici vanno posizionati il più vicino possibile al pin VCC del componente che proteggono, con via corta al ground plane.

---

## 4. Connettori — Riepilogo

| Ref | Tipo | Pin | Funzione |
|-----|------|-----|----------|
| J_BAT | BT2.0 femmina | 2 | Connettore batteria |
| J_SW1 | JST-PH 2 | 2 | Switch power (esterno) |
| J_SW2 | JST-PH 2 | 2 | Switch arm motori (esterno) |
| J_M1 | JST-PH 2 | 2 | Motore 1 (Front-Left) |
| J_M2 | JST-PH 2 | 2 | Motore 2 (Rear-Left) |
| J_M3 | JST-PH 2 | 2 | Motore 3 (Rear-Right) |
| J_M4 | JST-PH 2 | 2 | Motore 4 (Front-Right) |
| J_IMU | Pin header F 1×6 | 6 | Modulo GY-521 |
| J_FLOW | Pin header M 1×4 | 4 | Optical Flow (cavo sotto frame) |
| J_LED | Pin header M 1×2 | 2 | LED esterno (opzionale) |
| U_XIAO | Pin header F 2×7 | 14 | Modulo XIAO ESP32-S3 |
| BZ1 | Buzzer passivo 12mm | 2 | Buzzer (saldato su PCB) |

**Totale connettori JST-PH:** 6 (SW1, SW2, M1, M2, M3, M4)

---

## 5. Netlist Completa

Ogni net della PCB con i pin che collega:

| Net | Pin collegati | Larghezza traccia |
|-----|--------------|-------------------|
| VBAT | J_BAT(+), J_SW1(1), C1(+) | 3mm o copper pour |
| VBAT_SW | J_SW1(2), U_XIAO(VUSB), J_SW2(1), R1(1), VBAT_MOTORS | 3mm o copper pour |
| VBAT_MOTORS | J_SW2(2), D1(K), D2(K), D3(K), D4(K), J_M1(+), J_M2(+), J_M3(+), J_M4(+) | 3mm o copper pour |
| 3V3 | U_XIAO(3V3), C2(1), J_IMU(VCC), C3(1), J_FLOW(VCC), C4(1), R_led(1), BZ1(+) | 0.5mm |
| GND | Ground plane (bottom layer) — tutti i GND | Copper pour pieno |
| M1_GATE | R_s1(2), R_pd1(1), Q1(Gate) | 0.3mm |
| M2_GATE | R_s2(2), R_pd2(1), Q2(Gate) | 0.3mm |
| M3_GATE | R_s3(2), R_pd3(1), Q3(Gate) | 0.3mm |
| M4_GATE | R_s4(2), R_pd4(1), Q4(Gate) | 0.3mm |
| M1_DRAIN | Q1(Drain), D1(A), J_M1(−) | 2mm |
| M2_DRAIN | Q2(Drain), D2(A), J_M2(−) | 2mm |
| M3_DRAIN | Q3(Drain), D3(A), J_M3(−) | 2mm |
| M4_DRAIN | Q4(Drain), D4(A), J_M4(−) | 2mm |
| GPIO1 | U_XIAO(D0), R_s1(1) | 0.3mm |
| GPIO2 | U_XIAO(D1), R_s2(1) | 0.3mm |
| GPIO3 | U_XIAO(D2), R_s3(1) | 0.3mm |
| GPIO4 | U_XIAO(D3), R_s4(1) | 0.3mm |
| SDA | U_XIAO(D4), J_IMU(SDA) | 0.3mm |
| SCL | U_XIAO(D5), J_IMU(SCL) | 0.3mm |
| UART_TX | U_XIAO(D6), J_FLOW(RX) | 0.3mm |
| UART_RX | U_XIAO(D7), J_FLOW(TX) | 0.3mm |
| V_SENSE | U_XIAO(D8), R1(2), R2(1), C_vs(1) | 0.3mm |
| BUZZER | U_XIAO(D9), BZ1(+) | 0.3mm |
| LED_OUT | U_XIAO(D10), R_led(1) | 0.3mm |
| LED_A | R_led(2), LED1(A), J_LED(1) | 0.3mm |

---

## 6. BOM Completa per la PCB

### Componenti SMD (lato top)

| Ref | Componente | Valore | Package | Qtà | LCSC Part # (esempio) |
|-----|-----------|--------|---------|-----|----------------------|
| Q1-Q4 | MOSFET N-ch | SI2302 | SOT-23 | 4 | C10487 |
| R_pd1-4 | Resistenza | 10kΩ | 0603 | 4 | C25804 |
| R_s1-4 | Resistenza | 100Ω | 0603 | 4 | C22775 |
| R1, R2 | Resistenza | 100kΩ | 0603 | 2 | C25803 |
| R_led | Resistenza | 330Ω | 0603 | 1 | C23138 |
| C2, C3, C4, C_vs | Condensatore | 100nF 25V | 0603 | 4 | C14663 |
| LED1 | LED | Verde | 0603 | 1 | C72043 |

### Componenti THT (lato top)

| Ref | Componente | Valore | Package | Qtà |
|-----|-----------|--------|---------|-----|
| D1-D4 | Diodo Schottky | 1N5819 | DO-41 | 4 |
| C1 | Condensatore elettrolitico | 470μF 6.3V | Radiale Ø8×11mm | 1 |
| J_BAT | Connettore BT2.0 | Femmina | THT | 1 |
| J_SW1, J_SW2 | Connettore | JST-PH 2 pin | THT | 2 |
| J_M1-M4 | Connettore | JST-PH 2 pin | THT | 4 |
| J_IMU | Pin header femmina | 1×6 2.54mm | THT | 1 |
| J_FLOW | Pin header maschio | 1×4 2.54mm | THT | 1 |
| J_LED | Pin header maschio | 1×2 2.54mm | THT | 1 |
| U_XIAO | Pin header femmina | 2×7 2.54mm | THT | 2 (1×7 per lato) |
| BZ1 | Buzzer passivo | 12mm 3.3V | THT | 1 |

**Totale componenti:** ~35

---

## 7. Strategia Ground Plane

### Principio: bottom layer = ground plane pieno

L'intero bottom layer è un copper pour collegato a GND. Non ci sono tracce sul bottom — solo rame pieno e via di collegamento.

**Perché funziona:**
- Ogni componente sul top ha una via a GND che lo collega direttamente al piano sotto
- La corrente di ritorno di ogni segnale trova il percorso a impedenza più bassa direttamente sotto la traccia del segnale (effetto pelle)
- Non ci sono "giri strani" di corrente — ogni ritorno è locale
- È la soluzione definitiva al problema dei ground loop della perfboard

### Regole per il ground plane

1. **NESSUNA traccia sul bottom layer** — solo copper pour GND
2. **Via a GND** vicino a ogni pad GND di ogni componente (distanza < 2mm)
3. **Via stitching:** via GND ogni ~10mm lungo il perimetro della PCB e nelle aree vuote
4. **Non spezzare il piano:** se devi far passare una traccia sotto (emergenza), assicurati che non tagli il percorso di ritorno tra un MOSFET e il suo condensatore di disaccoppiamento
5. **Thermal relief** sui pad GND del copper pour: pattern a croce per facilitare la saldatura

### Separazione power/signal (tramite placement)

Non serve splittare il ground plane. Basta posizionare i componenti in modo che:
- I **MOSFET + diodi + JST motori** siano su un lato della PCB (zona "sporca")
- **XIAO + IMU + connettore flow** siano sull'altro lato (zona "pulita")
- Le tracce power non passino sotto i sensori

```
┌──────────────────────────────────────────┐
│  [J_M1]  Q1 D1        Q4 D4  [J_M4]     │  ← Zona motori (sporca)
│              ┌─────────┐                 │
│  [J_M2]  Q2 │  XIAO   │ Q3  [J_M3]     │
│              │ ESP32S3 │                 │
│  [J_SW1]    └─────────┘     [J_SW2]     │
│         [C1]   [GY-521]                  │  ← Zona sensori (pulita)
│  [BT2.0]  [BZ1]  [J_FLOW]  [J_LED]     │
└──────────────────────────────────────────┘

Bottom layer: GND plane pieno (nessuna traccia)
```

*Nota: il layout sopra è indicativo. La disposizione finale dipenderà dall'ottimizzazione dello spazio in EasyEDA.*

---

## 8. Regole di Routing

### Larghezze tracce

| Tipo | Larghezza | Corrente max | Usata per |
|------|-----------|-------------|-----------|
| Signal | 0.25-0.3mm | ~0.5A | GPIO, I2C, UART, LED, buzzer, gate MOSFET |
| Power 3V3 | 0.5mm | ~200mA | Rail 3.3V verso sensori |
| Motor drain | 2.0mm | ~3A | Drain MOSFET ↔ diodo ↔ JST motore |
| VBAT bus | 3.0mm o pour | ~13A | BT2.0 ↔ switch ↔ XIAO ↔ motori |
| VBAT_MOTORS | 3.0mm o pour | ~12A | Switch arm ↔ diodi flyback ↔ JST motori |

### Regole generali

1. **Tracce power il più corte e larghe possibile** — l'impedenza è proporzionale alla lunghezza e inversamente alla larghezza
2. **Tracce segnale lontane dalle tracce power** — almeno 1mm di distanza per evitare accoppiamento
3. **No angoli a 90°** — usare 45° o curve. Gli angoli a 90° creano discontinuità di impedenza (minore, ma è buona pratica)
4. **Tracce I2C (SDA/SCL) corte** — il bus I2C a 400kHz è sensibile alla capacità parassita. Tieni il connettore IMU vicino ai pin D4/D5 della XIAO
5. **Tracce UART più tolleranti** — 19200 baud è lento, possono essere più lunghe senza problemi
6. **Via per ogni cambio layer** — ma con il nostro design (tutto sul top, GND sul bottom) non servono via di segnale, solo via a GND
7. **Clearance minima 0.2mm** tra tracce — standard JLCPCB

### Routing delle tracce power (consigli specifici)

Per VBAT e VBAT_MOTORS, dove la corrente è alta:
- Preferisci **copper pour** invece di tracce singole. In EasyEDA puoi creare un copper pour sul top layer connesso alla net VBAT e riempire l'area attorno ai MOSFET e ai connettori motori
- Se usi tracce, falle da **3mm minimo** e il più corte possibile
- Il percorso VBAT: BT2.0 → (corto) → C1 → (corto) → SW1 JST → (corto) → XIAO VUSB + ramo SW2
- Il percorso VBAT_MOTORS: SW2 JST → copper pour → catodi D1-D4 → JST M1-M4

---

## 9. Posizionamento Componenti — Linee Guida

### Priorità di posizionamento (in ordine)

1. **XIAO ESP32-S3** — al centro, è il componente più grande (~21×17mm). Orienta con la porta USB-C accessibile dal bordo per programmazione
2. **GY-521 (IMU)** — vicino al centro geometrico della PCB, lontano dai motori. L'IMU misura accelerazioni e rotazioni: più è vicina al centro, meno errori di leva
3. **BT2.0 + C1** — su un bordo, vicini tra loro. Il condensatore bulk deve essere a <5mm dal connettore batteria
4. **4× MOSFET + diodi** — ai bordi/angoli, vicini ai rispettivi JST motori. Ogni gruppo (Qx + Dx + R_pdx + R_sx) forma un cluster compatto
5. **JST motori** — ai bordi, orientati verso i bracci del frame
6. **JST switch** — su un bordo accessibile
7. **Connettore flow** — su un bordo, il cavo scende sotto
8. **Buzzer** — dove c'è spazio
9. **LED + header** — dove c'è spazio, preferibilmente visibile

### Regole di posizionamento

- **Zona "sporca" (motori):** MOSFET, diodi, JST motori, copper pour VBAT_MOTORS. Questa zona ha correnti alte e switching a 20kHz
- **Zona "pulita" (segnali):** XIAO, GY-521, connettore flow, V-Sense, LED, buzzer. Questa zona deve restare lontana dal rumore
- **Non far passare tracce power sotto l'IMU** — il campo magnetico delle correnti pulsate disturba il magnetometro (se mai lo aggiungerai) e può indurre rumore sull'accelerometro
- **Condensatori 100nF** il più vicino possibile al pin VCC del componente associato (< 3mm)

---

## 10. Antenna WiFi — Zona di Keepout

La XIAO ESP32-S3 ha l'antenna PCB integrata su un lato del modulo (il lato opposto alla porta USB-C).

**OBBLIGATORIO:** nessun rame (tracce, pour, GND plane) deve trovarsi sotto l'area dell'antenna. In EasyEDA:
- Identifica quale lato della XIAO ha l'antenna (è il lato corto senza USB)
- Crea una **keepout zone** sul bottom layer (no copper pour) di almeno 10×5mm sotto l'antenna
- Non mettere componenti sopra o sotto l'antenna
- Orienta la XIAO in modo che l'antenna sia verso un bordo della PCB, non verso il centro

Se non rispetti questa regola, il range WiFi sarà drasticamente ridotto.

---

## 11. Test Point (opzionali ma consigliati)

Aggiungi pad di test esposti (senza solder mask) per debug con multimetro:

| Test point | Net | Scopo |
|-----------|-----|-------|
| TP_VBAT | VBAT_SW | Verificare tensione batteria dopo switch |
| TP_3V3 | 3V3 | Verificare regolatore XIAO |
| TP_VARM | VBAT_MOTORS | Verificare che lo switch arm funzioni |
| TP_GND | GND | Riferimento per misure |

Usa pad rotondi da 1.5mm senza solder mask. Occupano pochissimo spazio.

---

## 12. Checklist Pre-Produzione

Prima di ordinare su JLCPCB, verifica:

- [ ] DRC (Design Rule Check) passa senza errori in EasyEDA
- [ ] Tutti i footprint corrispondono ai componenti reali (misura con calibro!)
- [ ] Larghezza tracce power ≥ 2mm (motori), ≥ 3mm (VBAT bus)
- [ ] Ground plane bottom è continuo (nessun taglio accidentale)
- [ ] Keepout antenna rispettata
- [ ] Via a GND vicino a ogni pad GND componente
- [ ] Condensatori 100nF vicini ai VCC dei sensori
- [ ] Pull-down 10kΩ presenti su tutti e 4 i gate MOSFET
- [ ] Polarità BT2.0, JST, LED, diodi, elettrolitico verificata
- [ ] Fori montaggio M2 non interferiscono con tracce
- [ ] Silkscreen leggibile e non sovrapposta a pad
- [ ] Gerber generati e visualizzati nel preview JLCPCB prima di ordinare

---

## 13. Ordine su JLCPCB

Impostazioni consigliate per l'ordine:

| Parametro | Valore |
|-----------|--------|
| Base material | FR-4 |
| Layers | 2 |
| Dimensions | (dal tuo design) |
| PCB Qty | 5 (minimo) |
| PCB Thickness | 1.0mm |
| PCB Color | A scelta (verde è il più economico) |
| Surface Finish | HASL (lead-free) |
| Copper Weight | 1 oz |
| Specify Layer Sequence | No |
| Remove Order Number | Sì (specify location o remove) |

**Costo stimato:** $2-5 per 5 PCB + ~$5-15 spedizione.

**Assembly (opzionale):** JLCPCB offre assemblaggio SMD. Puoi far saldare i componenti SMD (MOSFET, resistenze, condensatori, LED) in fabbrica. Costa ~$8-15 extra ma ti evita di saldare SOT-23 e 0603 a mano. I componenti THT (diodi, connettori, pin header, buzzer, elettrolitico) li salderai tu.

---

## 14. Note per il Frame 3D Print

La PCB sarà il riferimento per il design del frame:
- I **4 fori M2** agli angoli definiscono i punti di fissaggio
- Prevedere spazio sotto la PCB per il cavo del flow (almeno 5mm)
- Il buzzer sporge sopra la PCB di ~5-6mm
- Il condensatore elettrolitico 470μF sporge di ~11mm (è il componente più alto)
- La XIAO con pin header sporge ~8-10mm sopra la PCB
- Il GY-521 con pin header sporge ~8-10mm sopra la PCB
- La porta USB-C della XIAO deve essere accessibile dal bordo per programmazione

---

## Appendice A: Confronto Perfboard vs PCB Custom

| Aspetto | Perfboard attuale | PCB Custom v1.0 |
|---------|-------------------|-----------------|
| Ground loop | GND condiviso motori/sensori | Ground plane pieno separato |
| Protezione MOSFET | Nessuna pull-down | 10kΩ + 100Ω per gate |
| Peso stimato | ~8-10g (millefori + fili) | ~3g (FR4 1.0mm + componenti SMD) |
| Affidabilità | Fili volanti, saldature fragili | Tracce in rame, saldature a reflow |
| Riproducibilità | Pezzo unico | 5 PCB identiche per $5 |
| Debug | Difficile | Test point dedicati |
| Vibrazioni | Fili si staccano | Tutto solidale |

## Appendice B: Schema di Principio Riassuntivo

```
                            ┌─────────── 3V3 ──────────────┐
                            │                               │
┌──────┐    ┌─────┐   ┌────┴────┐   ┌───────┐        ┌────┴────┐
│LiPo  │───►│ SW1 │──►│  XIAO   │──►│GY-521 │        │ Flow    │
│BT2.0 │    │power│   │ESP32-S3 │   │ IMU   │        │3901-L0X │
└──┬───┘    └─────┘   └────┬────┘   └───────┘        └─────────┘
   │                       │ D0-D3 (GPIO PWM)
   │  ┌──────┐             │
   └──┤ C1   │        ┌────┴──────────────────────────────┐
      │470μF │        │  ×4 Motor Driver                  │
      └──────┘        │  GPIO ─[100Ω]─ Gate ─┬─ SI2302   │
                      │                 [10kΩ]│  │Drain│   │
   ┌─────┐            │                  GND  │  1N5819   │
   │ SW2 │──► VBAT_M ─┤                       │     │     │
   │ arm │            │              VBAT_M ───┴─ Motor   │──► [JST] ──► Motore
   └─────┘            └───────────────────────────────────┘

   [V-Sense]  VBAT──100k──┬──100k──GND     [Buzzer] D9──►BZ
                    D8(ADC)┤                [LED]    D10─330Ω──►LED
                      100nF┤
                          GND
```
