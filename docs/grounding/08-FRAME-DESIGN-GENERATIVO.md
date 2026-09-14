# Frame Drone — Processo di Design Generativo (Fusion 360)

Documento di riferimento per il redesign del frame del drone tramite **Fusion 360 Generative Design**.
Hardware target di stampa: **Bambulab A1 Mini** (volume 180×180×180mm).

---

## 1. Workflow generale

1. Modellazione delle geometrie di vincolo (Preserve, Obstacle, Design Space)
2. Setup Generative Design (Constraints + Loads)
3. Definizione manufacturing constraints (FDM)
4. Pre-check + Generate (cloud solve)
5. Selezione outcome → Promote to Design → raffinamento
6. Export STL → slicer → stampa

---

## 2. Geometrie di vincolo

### 2.1 Preserve Geometry (zone che DEVONO esistere)
| Elemento | Forma | Note |
|----------|-------|------|
| Mount motori (×4) | Cilindri Ø ≈ 8.5mm | Posizionati simmetricamente (es. ±65, ±65 mm) |
| Mount PCB centrale | Piattaforma ~30×30mm | Per ESP32-S3 XIAO |
| Stand PCB (×4) | Pilastrini con foro vite | Possono restare body separati |
| Passacavi | Cilindri vuoti sugli arm | Opzionali |

### 2.2 Obstacle Geometry (zone vuote)
| Elemento | Forma consigliata | Offset consigliato |
|----------|-------------------|---------------------|
| Disco eliche (×4) | Cilindri Ø60mm × 8mm sopra ogni motore (per pale 55mm + margine) | 2-3 mm |
| Corpo motore | Cilindro Ø8.5mm × 15mm | 0.5-1 mm |
| Batteria (se presente) | Box che racchiude la LiPo | 1 mm |
| PCB intera | Bounding box semplificato (es. 25×30×8mm) — NO sottocomponenti dettagliati | 1-2 mm |

**Importante:** per la PCB usa SEMPRE un bounding box semplificato, non i sottocomponenti reali.
Il solver cloud è molto più veloce e i risultati equivalenti (al solver basta sapere "qui non puoi mettere materiale").

### 2.3 Design Space
- Solido che racchiude tutto il volume disponibile (es. parallelepipedo 200×200×10mm)
- È il "blocco di argilla" da cui il generativo sculperà
- Deve contenere tutte le Preserve Geometry

### 2.4 Starting Shape (opzionale ma consigliata)
- Body "scheletro a X" o "+" che collega i 4 mount motore al mount PCB
- Spessori abbondanti (es. arm 12×6mm, hub centrale 35×35×6mm)
- Deve toccare/includere tutte le Preserve, stare nel Design Space, non toccare Obstacle
- **Run iniziale:** prova *Unrestricted* per esplorare; **run finale:** usa Starting Shape per controllo topologia

---

## 3. Load Cases

### Load Case 1 — Hover (volo stazionario)
| Tipo | Dove | Valore |
|------|------|--------|
| **Fixed** | Faccia superiore mount PCB centrale | — |
| **Force** | Facce superiori dei 4 mount motore | ~0.3 N per mount, direzione +Z |

Formula: `peso_drone(kg) × 9.81 × 1.5 / 4` → per drone ~80g ≈ 0.3 N/mount

### Load Case 2 — Atterraggio duro / impatto
| Tipo | Dove | Valore |
|------|------|--------|
| **Fixed** | Facce inferiori dei 4 mount motore | — |
| **Force** | Faccia superiore mount PCB centrale | ~6 N, direzione -Z |

Formula: `peso_drone(kg) × 9.81 × 8G` → ~6 N per drone 80g

**Regola critica:** Force e Fixed devono essere su **facce diverse** in ogni load case, altrimenti il solver ignora la forza.

### Safety Factor
- Imposta a **2** nel pannello in basso

---

## 4. Manufacturing Constraints (Bambulab A1 Mini)

| Parametro | Valore |
|-----------|--------|
| Method | **Additive** (disabilita Milling!) |
| Orientation | Z-up |
| Overhang Angle | **45°** (anche se A1 può fare di più, riduce supporti) |
| Minimum Thickness | **1.2 mm** (= 3 perimetri × nozzle 0.4mm) |
| Build Volume | **180 × 180 × 180 mm** |
| Material | **PETG** consigliato (assorbe vibrazioni) o ABS Plastic da libreria come proxy |

### Materiale custom (se PLA non in libreria)
- Density: 1240 kg/m³
- Young's Modulus: 3500 MPa
- Yield Strength: 50 MPa
- Poisson's Ratio: 0.36

In alternativa: usa **ABS Plastic** dalla libreria come proxy — proprietà meccaniche simili, valido anche se poi stampi PLA/PETG.

---

## 5. Symmetry Planes

- Per il primo run: **non definire piani di simmetria** (più semplice, evita warning)
- Se vuoi forzare simmetria: verifica che TUTTE le preserve e obstacle siano perfettamente simmetriche rispetto al piano (coordinate esatte ±X, ±Y)

---

## 6. Errori e warning comuni nel Pre-check

| Messaggio | Causa | Fix |
|-----------|-------|-----|
| *All loads acting on fully fixed entities* | Force e Fixed sulla stessa faccia | Spostare Force su faccia diversa |
| *Milling Head Diameter large vs model* | Manufacturing Milling attivo | Disabilitare Milling, lasciare solo Additive |
| *Symmetry planes invalid* | Geometrie non simmetriche rispetto al piano | Rimuovere piano di simmetria o correggere coordinate |
| *Loads must be on preserve geometry* | Forza applicata su faccia del Design Space | Applicare la forza su una faccia di una Preserve |

---

## 7. Cloud Solve

- Il job gira sui server Autodesk → si può **spegnere il PC** dopo aver cliccato Generate
- Tempi tipici: 30 min – qualche ora
- Notifica via email a fine job
- Ogni run consuma **cloud credits** (gratis con licenza educational/personal)

---

## 8. Post-processing

1. Esamina gli outcome generati (più varianti per ogni load case combo)
2. Scegli la variante con miglior **rapporto massa/rigidità**
3. **Promote to Design** → diventa body editabile
4. Aggiungi `Fillet` su spigoli vivi
5. Aggiungi fori viti con `Hole` tool se mancanti
6. Lascia aperture per accedere ai pad MOSFET (pull-down 10kΩ)
7. Verifica peso target: **< 15g** (budget peso totale drone 80-100g)

---

## 9. Export per stampa

- `File → Export → STL` (risoluzione medio-fine)
- Slicer: Cura / PrusaSlicer / Bambu Studio
- Orientazione: flat sul piano (arm orizzontali)
- Infill: ≥ 40%
- Perimetri: ≥ 3 (= 1.2mm parete = min thickness del generativo)
- Materiale consigliato: **PETG** per resistenza a vibrazioni e impatti

---

## 10. Parametri drone di riferimento

- **Wheelbase:** ~130mm (motori 8520, eliche 55mm)
- **Peso target frame:** < 15 g
- **Peso totale drone:** 80-100 g
- **Spessore minimo arm:** 4-5 mm (per evitare vibrazioni)
- **Motori:** 8520 coreless brushed 3.7V 1S
- **Eliche:** 55mm

---

## Riferimenti

- CLAUDE.md (architettura progetto, pinout, hardware)
- docs/01-HARDWARE-BOM.md
- docs/pcb-custom/pcb-design-spec.md
