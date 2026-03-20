# Guida Pratica EasyEDA Std — Fase Schematic

## 1. Creare il Progetto

1. Apri EasyEDA Std (web: easyeda.com, oppure app desktop)
2. **File → New → Project** — nome: `microdrone-fc-v1`
3. Dentro il progetto: **New → Schematic** — si apre il foglio schema

---

## 2. Importare Componenti dalla Libreria LCSC

EasyEDA ha una libreria integrata con **tutti i componenti venduti da LCSC** (il magazzino di JLCPCB). Ogni componente ha già simbolo schematico + footprint PCB + link per l'acquisto.

### Metodo 1: Ricerca nella libreria (consigliato)

1. Nel pannello di sinistra, clicca **"Library"** (icona libro)
2. Nella barra di ricerca, cerca per **nome** o **codice LCSC**
3. Clicca sul componente → **"Place"** per piazzarlo sullo schema

### Metodo 2: Ricerca per LCSC Part Number

Se conosci il codice LCSC (dalla BOM del design spec), cercalo direttamente:
- Digita il codice (es. `C10487`) nella ricerca libreria
- Trovi il componente esatto con footprint verificato

---

## 3. Componenti da Cercare — Lista Pratica

### Moduli (NON hanno footprint LCSC standard — serve creare/trovare)

| Componente | Come trovarlo in EasyEDA |
|-----------|------------------------|
| **XIAO ESP32-S3** | Cerca "XIAO ESP32S3" nella libreria. Se non c'è, cerca nella sezione "User Contributed" (icona globo). Altrimenti crea un footprint custom con 2 file da 7 pin header femmina (passo 2.54mm), distanza tra file = 17.78mm (7×2.54mm) |
| **GY-521 (MPU6050)** | Non serve un footprint dedicato. Usa un **pin header femmina 1×6** (cerca "Female Header 1x6 2.54mm"). Il modulo si inserisce nei pin. |

### Componenti con codice LCSC (cerca per codice)

| Componente | Cerca | LCSC Code | Note |
|-----------|-------|-----------|------|
| SI2302 MOSFET | `C10487` | C10487 | SOT-23, ne servono 4 |
| Resistenza 10kΩ 0603 | `C25804` | C25804 | Pull-down gate, ne servono 4 |
| Resistenza 100Ω 0603 | `C22775` | C22775 | Serie gate, ne servono 4 |
| Resistenza 100kΩ 0603 | `C25803` | C25803 | Partitore V-Sense, ne servono 2 |
| Resistenza 330Ω 0603 | `C23138` | C23138 | LED, ne serve 1 |
| Condensatore 100nF 0603 | `C14663` | C14663 | Disaccoppiamento, ne servono 4 |
| LED verde 0603 | `C72043` | C72043 | Status LED |
| Diodo 1N5819 | cerca "1N5819" | vari | DO-41 THT, ne servono 4 |
| Condensatore 470μF 6.3V | cerca "470uF 6.3V" | vari | Elettrolitico radiale, ne serve 1 |
| JST-PH 2 pin THT | cerca "JST PH 2P vertical" | vari | Ne servono 6 (4 motori + 2 switch) |
| Pin header M 1×4 | cerca "Pin Header 1x4 2.54" | vari | Per connettore flow |
| Pin header M 1×2 | cerca "Pin Header 1x2 2.54" | vari | Per LED esterno |
| Pin header F 1×6 | cerca "Female Header 1x6 2.54" | vari | Per IMU GY-521 |
| Buzzer passivo 12mm | cerca "passive buzzer 12mm" | vari | THT |

### Connettore BT2.0

Il BT2.0 è un connettore proprietario BetaFPV. NON è nella libreria LCSC standard.

**Opzioni:**
1. **Usa un JST-PH 2 pin** al posto del BT2.0 sulla PCB, e fai un cavo adattatore BT2.0 → JST-PH
2. **Crea un footprint custom** misurando il tuo connettore BT2.0 con il calibro (distanza tra pin, dimensioni corpo)
3. **Cerca nella libreria user-contributed** "BT2.0" o "BetaFPV"

L'opzione 1 è la più semplice per un primo progetto.

---

## 4. Piazzare e Collegare i Componenti

### Piazzare
1. Cerca il componente nella libreria
2. Clicca **"Place"**
3. Clicca sullo schema dove vuoi metterlo
4. Premi **R** per ruotare prima di piazzare
5. Premi **ESC** quando hai finito di piazzare quel componente

### Collegare (Wiring)
1. Premi **W** (o clicca l'icona "Wire" nella toolbar)
2. Clicca su un pin di un componente
3. Trascina fino al pin dell'altro componente
4. Clicca per completare la connessione
5. EasyEDA crea automaticamente una **net** (collegamento elettrico)

### Net Label (FONDAMENTALE per schemi puliti)
Invece di tirare fili lunghissimi attraverso tutto lo schema, usa i **Net Label**:

1. Premi **N** (o menu "Place → Net Label")
2. Digita il nome della net (es. "VBAT", "3V3", "GND", "GPIO1")
3. Piazzalo su un filo
4. Tutti i punti con lo stesso net label sono elettricamente collegati

**Esempio:** metti un label "VBAT" sul pin + del BT2.0 e un altro label "VBAT" sul pin dello switch. Sono collegati senza tirare un filo visibile.

### Power Port (per GND e alimentazione)
Per GND e VCC, usa i **Power Port** (simboli speciali):
1. Menu **"Place → Power Port"**
2. Scegli "GND" → piazzalo su ogni pin GND
3. Tutti i simboli GND sono automaticamente collegati alla stessa net

---

## 5. Organizzare lo Schema

### Suggerimento: dividi in blocchi logici

Organizza lo schema in aree visive separate:

```
┌─────────────┐  ┌──────────────┐  ┌─────────────┐
│ POWER       │  │ XIAO ESP32   │  │ SENSORI     │
│ BT2.0       │  │ (centro)     │  │ IMU         │
│ Switch      │  │              │  │ Flow        │
│ C1 470μF    │  │              │  │ V-Sense     │
└─────────────┘  └──────────────┘  └─────────────┘

┌──────────────────────────────────┐  ┌───────────┐
│ MOTOR DRIVERS (×4)               │  │ UI        │
│ Q1-Q4 + D1-D4 + R_pd + R_s     │  │ Buzzer    │
│                                  │  │ LED       │
└──────────────────────────────────┘  └───────────┘
```

### Copiare circuiti ripetuti
I 4 driver motori sono identici. Disegnane uno, poi:
1. Seleziona tutti i componenti del driver (Ctrl+click o box select)
2. **Ctrl+C** → **Ctrl+V** per duplicare
3. EasyEDA rinumera automaticamente i componenti (Q1→Q2, R1→R2, ecc.)
4. Ripeti 3 volte per avere 4 driver

---

## 6. Il Footprint della XIAO ESP32-S3

La XIAO è il componente più critico perché probabilmente non ha un footprint ufficiale in LCSC.

### Come creare il footprint

La XIAO ha **2 file di 7 pin** (14 pin totali), passo 2.54mm.

**Dimensioni fisiche della XIAO:**
- Lunghezza: 21mm
- Larghezza: 17.5mm
- Distanza tra le due file di pin: ~15.24mm (6 × 2.54mm, misura col calibro!)
- 7 pin per fila, passo 2.54mm

**In EasyEDA:**
1. Cerca "DIP-14" o "Pin Header 2x7 2.54mm" nella libreria
2. Se il footprint ha la distanza corretta tra le file, usalo
3. Altrimenti: **"Library → Footprint → New Footprint"**
   - Piazza 14 pad THT (drill 1.0mm, pad 1.7mm) in 2 file da 7
   - Passo: 2.54mm verticale tra i pin
   - Distanza tra file: misura con calibro sul tuo XIAO
   - Disegna il contorno del modulo sul layer silkscreen
   - Segna il pin 1 con un punto

### Pinout della XIAO (da sinistra a destra, vista dall'alto)

```
         USB-C (questo lato)
    ┌─────────────────────┐
    │ D0  (GPIO1)    5V   │
    │ D1  (GPIO2)    GND  │
    │ D2  (GPIO3)    3V3  │
    │ D3  (GPIO4)    D10  │
    │ D4  (GPIO5)    D9   │
    │ D5  (GPIO6)    D8   │
    │ D6  (GPIO43)   D7   │
    └─────────────────────┘
         Antenna (questo lato)
```

**IMPORTANTE:** verifica questo pinout con la documentazione ufficiale Seeed. L'ordine dei pin potrebbe variare.

---

## 7. ERC (Electrical Rule Check)

Prima di passare al PCB layout, esegui l'ERC:

1. Menu **"Design → Check ERC"** (o icona nella toolbar)
2. EasyEDA controlla:
   - Pin non connessi
   - Cortocircuiti tra net diverse
   - Pin di alimentazione senza sorgente
   - Net con un solo pin (inutili)
3. Correggi tutti gli errori
4. I warning si possono ignorare caso per caso (es. pin NC dell'IMU)

---

## 8. Passare al PCB Layout

Una volta che lo schematic è completo e l'ERC è pulito:

1. Menu **"Design → Convert to PCB"**
2. Si apre l'editor PCB con tutti i componenti ammassati in un angolo
3. Le linee sottili (ratsnest) ti mostrano quali pad devono essere collegati
4. Da qui inizia la fase di layout (vedi la design spec per le regole)
