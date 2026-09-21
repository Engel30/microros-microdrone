# 2026-09-14 — Riorganizzazione documentale e sistema di continuità sessioni

**Branch:** `main` · **Commit:** `b5af579`..`273f631` (7 commit)

Prima sessione dopo 4 mesi di fermo. Nessun lavoro su firmware o hardware: solo documentazione e struttura del repo.

## Fatto

**Struttura.** `docs/` diventa autosufficiente e stratificata per volatilità:

```
docs/
├── STATO.md            unico posto con informazione volatile
├── README.md           indice + percorsi di lettura
├── grounding/01..08    come funziona il sistema, in ordine di lettura
├── sessions/           un file per giornata di lavoro, append-only
├── specs/              decisioni datate, immutabili
├── pcb-custom/  assets/  archive/
└── check-links.sh
```

Root ripulita: `attach-usb.md` (duplicato del contenuto di `docs/grounding/04-SETUP-AMBIENTE.md`) e `temp/` in `docs/archive/` e `docs/assets/`, `3d models/` → `hardware/3d-models/` (via lo spazio dal nome).

**Grounding rinumerati** in ordine di lettura: visione → hardware → firmware → setup → bringup → microros → swarm → frame. 85 riferimenti incrociati riscritti.

**Sessioni.** Le due timeline parallele — ora in `docs/archive/timeline-2026-05.md` e `docs/archive/timeline-2026-04.md` — fuse e spezzate in 7 file, uno per giornata di lavoro. Contenuto copiato fedelmente.

**`/chiudi-sessione`** in `.claude/commands/`: procedura in 6 passi che scrive il file sessione, riscrive `STATO.md`, aggiorna l'indice, esegue il link-checker e propone il commit.

**`docs/check-links.sh`**: verifica che ogni riferimento a file punti a qualcosa che esiste.

**Riscritti** `CLAUDE.md` e `README.md`.

**File toccati:** 46. Nessun file di build, nessun sorgente firmware — `idf.py build` invariato per costruzione.

## Scoperto

**Il problema non era il disordine, era che nessuno possedeva lo stato.** Era scritto in quattro posti (la spec del 2026-04-26, il timeline, `README.md`, `CLAUDE.md`) e tre erano indietro. Il documento designato come source of truth dichiarava "PCB layout in corso, ordine JLCPCB pendente, bring-up motori pendente" mentre il timeline (oggi `docs/archive/timeline-2026-05.md`) documentava che il 9 maggio la board era saldata e i motori giravano.

**Causa radice del marcire:** la regola in `CLAUDE.md` diceva di aggiornare il timeline a fine sessione. Dipendeva dalla memoria dell'agente e nominava un solo file. Ha funzionato per quello che nominava e ha fallito su tutto il resto — infatti il timeline era aggiornato e il source of truth no. Da qui la scelta di un comando esplicito invece di una regola in prosa.

**I documenti di produzione PCB avrebbero fatto ordinare i pezzi sbagliati.** `docs/pcb-custom/easyeda-guida-uso.md` indicava MOSFET SI2302 (LCSC `C10487`) e diodo 1N5819 in package DO-41 THT. Angelo ha confermato che la board v1.0 monta **AO3400A (SOT-23)** e **SS14 (SMA)**: componente sbagliato *e* footprint sbagliato. Corretti 5 file vivi; nella BOM il diodo è passato dalla tabella THT a quella SMD. Non toccati `archive/` e `specs/`, che registrano cosa si sapeva allora.

**`CLAUDE.md` forniva contesto obsoleto a ogni sessione:** albero struttura con una guida di setup a un percorso abbandonato ad aprile e senza gli otto doc di grounding; `task_microros` a 50Hz e `imu_queue` depth 5, fermi ai valori pre-Step 5. I valori reali sono 100Hz e 20, verificati su `components/common/include/drone_config.h`.

**Il link-checker deve guardare i backtick, non solo i link markdown.** In questo repo i rimandi sono quasi tutti in prosa fra backtick, ed è la forma che si rompe in silenzio: nessun renderer markdown la segnala.

**Vicolo cieco del checker.** La prima versione produceva 275 "rotti" su 387: erano quasi tutti falsi positivi — tipi di messaggio ROS (`sensor_msgs/msg/Imu`), namespace di topic (`/drone_N/`), nomi di componenti (`uros_interface/`), librerie esterne (`esp-idf-lib/mpu6050`), ellissi in prosa. Tre giri di affinamento: (1) solo stringhe con estensione nota, (2) che contengano una `/` o abbiano la forma di un doc di grounding, (3) rimuovere i link markdown prima di scansionare i backtick, perché l'etichetta di un link non è un percorso. Un checker che grida al lupo viene ignorato entro due settimane.

**Trappola tecnica della rinumerazione.** `01→02, 02→03, 03→04, 04→01` è un ciclo: una sequenza di `git mv` dentro `docs/` avrebbe sovrascritto file. Risolto spostando direttamente in `docs/grounding/` col nome nuovo — directory diversa, nessuna collisione. Inoltre i grounding scendendo di un livello hanno richiesto un `../` in più sui riferimenti che escono dalla cartella (README di `ros2_ws/`, rimandi a `docs/specs/`): rottura che un `sed` sui soli nomi file non intercetta.

**Le sessioni erano 7, non 13.** Sei voci su otto nel timeline di maggio erano tutte del 2026-05-08: blocchi di lavoro della stessa giornata, non sessioni distinte. Nella timeline vecchia il 2026-03-20 era elencato dopo il 2026-04-26 — ordine cronologico rotto, sistemato in migrazione.

## Aperto

- **Codici LCSC di AO3400A e SS14 marcati `DA VERIFICARE`** in `docs/pcb-custom/easyeda-guida-uso.md` e `pcb-design-spec.md`. Non sono stati indovinati di proposito: scriverne uno a memoria avrebbe riprodotto esattamente il difetto appena corretto. Vanno presi dalla BOM del progetto EasyEDA della v1.0 già prodotta.
- **`feature/microros-tethered`** è interamente dentro `main` ma non è stato cancellato: `git branch -d feature/microros-tethered && git push origin --delete feature/microros-tethered`.
- **`docs/specs/2026-04-26-stato-progetto-e-roadmap.md`** riporta ancora SI2302/1N5819 e uno stato superato. È voluto: è una spec storica. Non ha però l'header "STORICO" che hanno le altre due — si potrebbe aggiungere.
- **Nessuna verifica su hardware.** Non è stato acceso nulla: il drone non è stato alimentato né flashato in questa sessione.

## Prossimo

La riorganizzazione non cambia lo stato tecnico del progetto: i prossimi passi restano quelli del 2026-05-09, ora in `docs/STATO.md`.

1. LiPo 1S 25C 300–600 mAh, ri-test motori sopra il 20% PWM
2. Frame 2.0 per smorzare le vibrazioni verso l'IMU
3. Fase 1 — PID di assetto

Prima di dare tensione: la checklist "alla ripresa, da verificare fisicamente" in `docs/STATO.md`.
