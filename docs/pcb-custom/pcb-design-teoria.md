# Teoria Base di Design PCB

Guida pratica per chi progetta la prima PCB. Orientata a EasyEDA + JLCPCB.

---

## 1. Come funziona una PCB

Una PCB (Printed Circuit Board) è un sandwich di strati:

```
  Silkscreen (testo bianco, etichette)
  Solder mask (vernice protettiva, il "colore" della PCB)
  Rame top (tracce e pad, dove saldi i componenti)
  Substrato FR4 (fibra di vetro + resina, l'isolante)
  Rame bottom (tracce o copper pour)
  Solder mask bottom
```

I componenti si saldano sui **pad** (piazzole di rame) sul top layer. Le **tracce** (piste di rame) collegano i pad tra loro. I **via** sono fori metallizzati che collegano il top al bottom layer.

---

## 2. Schematic vs Layout

Il processo di design ha due fasi distinte:

### Schematic (schema elettrico)
- Disegni il **circuito logico**: cosa è collegato a cosa
- Non ti preoccupi di dove stanno fisicamente i componenti
- Usi simboli standard (resistenza = zig-zag, condensatore = due linee, ecc.)
- Il risultato è una **netlist**: la lista di tutte le connessioni

### Layout (PCB)
- Prendi la netlist e **posizioni fisicamente** i componenti sulla PCB
- Disegni le **tracce** che realizzano le connessioni
- Ti preoccupi di dimensioni, larghezze, distanze, interferenze
- Il risultato sono i **file Gerber**: il "PDF" della PCB che mandi in fabbrica

**In EasyEDA:** disegni prima lo schematic, poi clicchi "Convert to PCB" e l'editor ti mostra tutti i componenti con le connessioni da realizzare (linee sottili chiamate "ratsnest"). Tu li posizioni e tracci le piste.

---

## 3. Footprint e Package

Ogni componente ha un **footprint** — la sua "impronta" fisica sulla PCB:

- **THT (Through-Hole Technology):** componenti con piedini che passano attraverso fori nella PCB. Più facili da saldare a mano. Esempi: pin header, diodi 1N5819, condensatori elettrolitici.
- **SMD (Surface Mount Device):** componenti saldati sulla superficie senza fori. Più piccoli e leggeri. Esempi: resistenze 0603, MOSFET SOT-23, LED 0603.

### Codici package SMD comuni

| Codice | Dimensioni (mm) | Difficoltà saldatura |
|--------|-----------------|---------------------|
| 0402 | 1.0 × 0.5 | Molto difficile (serve microscopio) |
| 0603 | 1.6 × 0.8 | Fattibile con buon saldatore |
| 0805 | 2.0 × 1.25 | Facile |
| 1206 | 3.2 × 1.6 | Molto facile |
| SOT-23 | 2.9 × 1.3 | Richiede flussante e punta fine |

**Consiglio:** per il primo progetto, 0603 è il compromesso ideale. Non è troppo piccolo (si riesce a saldare a mano con flussante) e non spreca spazio.

---

## 4. Tracce — Larghezza e Corrente

La larghezza della traccia determina quanta corrente può portare senza scaldarsi troppo.

Formula semplificata (IPC-2221, 1 oz rame, 10°C di riscaldamento, layer esterno):

| Corrente | Larghezza minima |
|----------|-----------------|
| 0.5A | 0.25mm |
| 1A | 0.4mm |
| 2A | 0.9mm |
| 3A | 1.5mm |
| 5A | 3.0mm |
| 10A | 8.0mm |

**Regola pratica:**
- Segnali digitali (GPIO, I2C, UART): 0.25-0.3mm
- Alimentazione bassa corrente (3.3V, <200mA): 0.5mm
- Alimentazione media (motori singoli, ~3A): 2mm
- Alimentazione alta (bus batteria, >5A): 3mm+ o copper pour

**Copper pour:** invece di una traccia singola, puoi "riempire" un'area intera di rame collegata a una net. È come avere una traccia infinitamente larga. Usalo per GND e per le rail ad alta corrente.

---

## 5. Via

Un via è un foro metallizzato che collega il rame del top a quello del bottom.

```
    Top layer (traccia)
         │
    ┌────┴────┐
    │  Via    │  ← foro metallizzato (0.3mm drill, 0.6mm pad)
    └────┬────┘
         │
    Bottom layer (ground plane o traccia)
```

### Tipi di via

- **Through-hole via:** attraversa tutta la PCB (il tipo standard, l'unico disponibile su PCB 2-layer economiche)
- **Blind/buried via:** non attraversa tutto (solo per PCB multilayer avanzate, non ti serve)

### Quando usare i via

- **Via a GND:** ogni componente che ha un pin GND deve avere un via vicino che lo collega al ground plane sul bottom. Più è vicino, meglio è.
- **Via stitching:** via aggiuntivi sparsi per collegare meglio il top al bottom GND. Mettine uno ogni ~10mm nelle aree vuote.
- **Via per cambio layer:** se devi far passare una traccia dal top al bottom per superare un ostacolo (evitalo se possibile nel design 2-layer).

---

## 6. Ground Plane

Il ground plane è un'area di rame continua che copre un intero layer della PCB, collegata a GND.

### Perché è fondamentale

1. **Bassa impedenza:** il ritorno di corrente (da ogni componente verso il terminale − della batteria) ha il percorso più corto possibile
2. **Schermatura EMI:** il piano di rame sotto le tracce di segnale riduce le emissioni elettromagnetiche
3. **Dissipazione termica:** il rame distribuisce il calore
4. **Semplicità di routing:** non devi tracciare le connessioni GND — ogni pad GND va collegato al piano con un via

### Regola d'oro

**Non spezzare mai il ground plane.** Se una traccia taglia il piano, la corrente di ritorno deve fare il giro lungo → aumenta l'impedenza → rumore. In un design 2-layer con tutti i componenti sul top e GND sul bottom, questo non succede naturalmente.

---

## 7. Condensatori di Disaccoppiamento

Ogni IC (chip) ha bisogno di un condensatore ceramico da 100nF tra il suo pin VCC e GND, posizionato il più vicino possibile (< 3mm).

### Perché servono

Quando un chip cambia stato (es. un GPIO passa da 0 a 1), assorbe un picco di corrente dalla rail di alimentazione. Questo picco crea un "buco" di tensione (glitch) che può disturbare altri chip sulla stessa rail.

Il condensatore 100nF è una "riserva locale" di energia: fornisce la corrente del picco istantaneamente, senza che debba arrivare dalla batteria (che è lontana).

### Regola pratica

- **100nF ceramico** per ogni chip (disaccoppiamento ad alta frequenza)
- **10-100μF elettrolitico** per la rail principale, vicino alla sorgente (disaccoppiamento a bassa frequenza, "bulk")
- Posiziona prima il condensatore, poi instrada la traccia VCC: il flusso di corrente deve essere Sorgente → condensatore → chip, non Sorgente → chip → condensatore

---

## 8. Regole di Clearance e Spacing

| Parametro | Valore minimo JLCPCB | Consigliato |
|-----------|----------------------|-------------|
| Traccia-traccia | 0.127mm (5 mil) | 0.2mm |
| Traccia-pad | 0.127mm | 0.2mm |
| Traccia-bordo PCB | 0.2mm | 0.5mm |
| Pad-pad (SMD) | dipende dal footprint | segui il datasheet |
| Via drill | 0.3mm | 0.3mm |
| Via pad | 0.6mm | 0.6mm |
| Componente-bordo | 1mm | 2mm |

**In EasyEDA:** imposta queste regole nel Design Rule Manager prima di iniziare il routing. L'editor ti avvisa se violi una regola.

---

## 9. DRC (Design Rule Check)

Prima di generare i Gerber, esegui il DRC in EasyEDA. Controlla automaticamente:

- Tracce troppo vicine tra loro
- Tracce che si sovrappongono
- Net non connesse (hai dimenticato un collegamento)
- Via troppo piccoli
- Pad senza connessione (componente "volante")
- Clearance insufficiente

**Non mandare MAI in produzione una PCB con errori DRC.** Ogni errore è un potenziale cortocircuito o circuito aperto.

---

## 10. File Gerber e Produzione

I **file Gerber** sono il formato standard per la produzione di PCB. Sono come i PDF per la stampa: descrivono ogni layer della PCB in modo che la fabbrica sappia dove mettere rame, fori, solder mask, ecc.

### Generare Gerber in EasyEDA

1. Apri il PCB layout
2. Menu "Fabrication" → "Generate Gerber"
3. EasyEDA genera un file .zip con tutti i layer
4. Carica lo .zip su JLCPCB → "Order Now" → "Add Gerber File"
5. JLCPCB mostra un preview 3D della PCB — **controllalo!**
6. Verifica: dimensioni, fori, numero layer, tutto corretto
7. Scegli i parametri (spessore, colore, finitura) e ordina

**Con EasyEDA + JLCPCB**, il processo è semplificato: puoi ordinare direttamente dall'editor senza esportare Gerber manualmente. Ma è buona pratica controllare il preview.

---

## 11. Errori Comuni del Principiante

| Errore | Conseguenza | Come evitarlo |
|--------|-------------|---------------|
| Tracce power troppo sottili | Si scaldano, resistenza alta, brownout | Calcola la corrente e usa la tabella IPC-2221 |
| GND plane spezzato | Ground loop, rumore sui sensori | Tieni tutto sul top, GND intero sul bottom |
| Condensatore lontano dal chip | Non filtra i picchi di corrente | Posizionalo prima del routing, < 3mm dal pin VCC |
| Footprint sbagliato | Il componente non entra nei pad | Verifica con calibro o datasheet prima di ordinare |
| Rame sotto l'antenna WiFi | Range WiFi ridotto drasticamente | Crea keepout zone |
| Angoli a 90° nelle tracce | Problema minore, ma brutta pratica | Usa 45° o curve |
| Via sottodimensionati | Via si rompono o hanno resistenza alta | Usa via 0.3mm drill / 0.6mm pad (standard) |
| Nessun test point | Debug impossibile senza punti di misura | Aggiungi pad esposti sulle net critiche |
| Polarità invertita | Componente bruciato (LED, elettrolitico, diodo) | Segna polarità su silkscreen, verifica prima di saldare |
| Ordinare solo 1 PCB | Se sbagli, devi riordinare e aspettare | Ordina sempre 5 (costano quasi uguale) |

---

## 12. Workflow Completo

```
1. SCHEMATIC (EasyEDA)
   │  Disegna il circuito logico
   │  Assegna footprint a ogni componente
   │  Verifica ERC (Electrical Rule Check)
   │
   ▼
2. CONVERT TO PCB
   │  EasyEDA genera il layout con ratsnest
   │
   ▼
3. BOARD OUTLINE
   │  Disegna la forma della PCB
   │  Posiziona i fori di montaggio
   │
   ▼
4. PLACEMENT
   │  Posiziona i componenti (segui le linee guida)
   │  Il ratsnest ti mostra dove devono andare le connessioni
   │
   ▼
5. COPPER POUR (GND)
   │  Crea copper pour sul bottom layer → net GND
   │  Aggiungi via a GND vicino a ogni pad GND
   │
   ▼
6. ROUTING
   │  Traccia le piste sul top layer
   │  Inizia dalle tracce power (più larghe)
   │  Poi le tracce segnale
   │
   ▼
7. COPPER POUR (VBAT) — opzionale
   │  Riempi le aree vuote del top con VBAT o GND
   │
   ▼
8. VIA STITCHING
   │  Aggiungi via GND nelle aree vuote
   │
   ▼
9. SILKSCREEN
   │  Etichetta componenti, connettori, polarità
   │  Aggiungi nome progetto e versione
   │
   ▼
10. DRC
    │  Esegui Design Rule Check
    │  Correggi tutti gli errori
    │
    ▼
11. REVIEW
    │  Controlla visivamente ogni net
    │  Verifica footprint con datasheet
    │
    ▼
12. GENERATE GERBER → JLCPCB → ORDINA
```

---

## 13. Risorse Utili

- **EasyEDA Tutorial ufficiale:** integrato nell'editor, segui il "Getting Started"
- **JLCPCB capabilities:** pagina "PCB Capabilities" sul sito JLCPCB per i limiti di produzione
- **IPC-2221 trace width calculator:** cerca "PCB trace width calculator" online — inserisci corrente e ti dà la larghezza
- **Datasheet componenti:** cerca sempre il datasheet per verificare footprint e pinout
- **Phil's Lab (YouTube):** canale eccellente per PCB design, spiega teoria e pratica con KiCad (i concetti si applicano anche a EasyEDA)
