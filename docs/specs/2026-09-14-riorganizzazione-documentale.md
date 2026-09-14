# Riorganizzazione documentale e sistema di continuità sessioni

**Data:** 2026-09-14
**Autore:** Angelo + Claude
**Stato:** Approvata — da eseguire
**Branch di esecuzione:** `chore/riorganizzazione-docs` (da `main`, dopo merge di `feature/microros-tethered`)

---

## 1. Problema

Il progetto è rimasto fermo dal 2026-05-09 al 2026-09-14 (4 mesi, nessun file toccato, working tree pulito). Alla ripresa, la documentazione non permette di capire dove si è rimasti senza leggere il `git log`:

1. **Il source of truth è scaduto.** `docs/specs/2026-04-26-stato-progetto-e-roadmap.md` dichiara "PCB layout in corso, ordine JLCPCB pendente, bring-up motori pendente". Ma `docs/archive/timeline-2026-05.md` documenta che l'8-9 maggio il PCB v1.0 era saldato, i 4 motori giravano e il brownout del buck-boost era stato diagnosticato. Il documento designato come riferimento unico è indietro di due sessioni.

2. **L'informazione di stato è spalmata su 4 file** (`specs/2026-04-26`, `docs/archive/timeline-2026-05.md`, `README.md`, `CLAUDE.md`), ognuno con una versione diversa della verità. È la causa strutturale del punto 1: nessuno di questi file ha un unico proprietario dello stato.

3. **`README.md` è rotto.** Rimanda a `docs/grounding/04-SETUP-AMBIENTE.md`, `docs/grounding/02-HARDWARE-BOM.md`, `docs/grounding/02-HARDWARE-BOM.md`, `docs/PROJECT_CONTEXT.md`, `docs/PROJECT_CONCEPT.md` — tutti spostati in `docs/archive/` ad aprile. Non cita nessuno degli otto doc `01-08`. La tabella roadmap dichiara "Fase 0A in corso" (completata a marzo).

4. **`CLAUDE.md` è disallineato.** L'albero struttura mostra `docs/grounding/04-SETUP-AMBIENTE.md` (inesistente) e omette i doc `01-08`; la roadmap dice "Fase 0B bloccata, ESP32 bruciato" (superato dal 2026-05-09, commit `5296be5` *"drone funzionante"*).

5. **Dati hardware errati in documenti di produzione.** `docs/pcb-custom/easyeda-guida-uso.md` e `pcb-design-spec.md` indicano MOSFET SI2302 (LCSC `C10487`) e diodo 1N5819 in package DO-41 THT. La board v1.0 monta **AO3400A (SOT-23)** e **SS14 (SMA)** — confermato da Angelo il 2026-09-14. Chi riordinasse la PCB da questi documenti comprerebbe i componenti sbagliati.

6. **Due timeline parallele** (`docs/archive/timeline-2026-05.md`, `docs/archive/timeline.md`) con storie complementari mai unite; la seconda ha anche l'ordine cronologico rotto (2026-03-20 elencato dopo 2026-04-26).

7. **Root disordinata:** `attach-usb.md` (3 righe, duplicate di `04-SETUP-AMBIENTE.md`), `temp/` (4 schematici tracciati in git), `3d models/` (spazio nel nome), `logs/`, `sdkconfig.old`.

**Causa radice del marcire:** `CLAUDE.md` prescrive *"aggiorna `docs/archive/timeline-2026-05.md` a fine sessione"*. È una regola che dipende dalla memoria dell'agente e che copre solo la timeline: infatti la timeline è stata aggiornata e il source of truth no.

## 2. Obiettivi

| # | Obiettivo | Criterio di verifica |
|---|---|---|
| O1 | Un solo file contiene lo stato volatile del progetto | `docs/STATO.md` esiste; nessun altro doc vivo dichiara fase corrente o blockers |
| O2 | Riprendere il progetto dopo mesi richiede la lettura di un solo file | `STATO.md` risponde a "dove siamo" e "prossimi 3 passi" senza rimandi obbligatori |
| O3 | Ogni sessione lascia una traccia autonoma | `docs/sessions/YYYY-MM-DD-slug.md`, uno per giorno di lavoro |
| O4 | L'aggiornamento è una procedura, non un ricordo | `/chiudi-sessione` esegue i passi in modo deterministico |
| O5 | Nessun link morto nel repo | Link-checker a zero errori (output allegato al commit) |
| O6 | I documenti di produzione PCB descrivono la board reale | Zero occorrenze di SI2302/1N5819 nei doc vivi |

### Non obiettivi

- Riscrivere il contenuto tecnico dei doc di grounding (sono corretti e aggiornati a maggio).
- Riscrivere i documenti storici per renderli coerenti col presente: registrano cosa si sapeva allora, e falsificarli distruggerebbe la tracciabilità delle decisioni (valore per la tesi).
- Toccare firmware, `logs/`, `ros2_ws/`, `old/`.

## 3. Struttura finale

```
microros-microdrone/
├── CLAUDE.md                        # riscritto
├── README.md                        # riscritto
├── CMakeLists.txt  sdkconfig.defaults  app-colcon.meta
├── main/  components/  ros2_ws/  old/
├── logs/                            # invariato (dati sperimentali)
├── hardware/3d-models/              # ex "3d models/"
├── .claude/commands/chiudi-sessione.md
└── docs/
    ├── README.md                    # indice + "cosa leggo quando"
    ├── STATO.md                     # ⭐ source of truth vivo
    ├── grounding/                   # 8 doc operativi, rinumerati
    ├── sessions/                    # 1 file per giorno di lavoro + README
    ├── specs/                       # spec datate, immutabili
    ├── pcb-custom/                  # invariato salvo correzioni MOSFET
    ├── assets/                      # ex temp/
    └── archive/                     # ex docs/archive/ + timeline + attach-usb.md
```

### 3.1 Separazione delle responsabilità

Questa è la regola che impedisce il ripetersi del problema:

| Cartella | Risponde a | Volatilità | Si aggiorna |
|---|---|---|---|
| `STATO.md` | *A che punto siamo?* | alta | ogni sessione, via comando |
| `grounding/` | *Come funziona?* | media | quando cambia il sistema |
| `specs/` | *Cosa abbiamo deciso, e quando?* | nulla | mai (immutabili) |
| `sessions/` | *Cosa è successo quel giorno?* | nulla | mai (append-only) |
| `archive/` | *Cosa pensavamo prima?* | nulla | mai |

**Invariante:** l'informazione volatile vive solo in `STATO.md`. Un grounding doc che dichiara "Fase 0B in corso" è un bug.

## 4. `docs/STATO.md`

File vivo, **senza data nel nome**, riscritto integralmente ad ogni chiusura di sessione.

```markdown
# STATO — microros-microdrone

**Aggiornato:** YYYY-MM-DD · **Ultima sessione:** [link] · **Branch:** <branch>

## In una riga
<una frase: dove siamo e qual è il prossimo passo>

## Dove siamo
Fase corrente + cosa funziona (con evidenza: commit, test) + cosa no.

## Blockers attivi
| Blocker | Impatto | Ipotesi di soluzione |

## Prossimi 3 passi
1. / 2. / 3. — concreti e azionabili, in ordine.

## Roadmap fasi
| Fase | Stato | Obiettivo | Criterio di successo |

## Decisioni consolidate
Link alle spec. Non ricopiate.

## Ultime 3 sessioni
Link a docs/sessions/.
```

**Contenuto iniziale** (dedotto da `docs/archive/timeline-2026-05.md` 2026-05-09 e commit `5296be5`, nessun evento fuori repo da maggio — confermato da Angelo):

- Fase 0A ✅; Fase 0B 🟡 motori girano ma alimentazione inadeguata.
- Blocker 1: brownout del buck-boost AliExpress al transitorio PWM (2 motori @20% → reset). Soluzione: LiPo 1S 300-600mAh 25C; workaround 1000µF + 100nF sull'uscita del buck.
- Blocker 2: `cmd_motor_test` richiede rate ≥5Hz per il watchdog 500ms; il pannello Publish di Foxglove non ha rate nativo → `ros2 topic pub -r 10`.
- Prossimi passi: (1) alimentazione da LiPo e ri-test 4 motori @20%+, (2) `pid_controller` + `task_pid_attitude` (Fase 1), (3) tuning su banco con eliche staccate.

## 5. `docs/sessions/`

Formato `YYYY-MM-DD-slug.md`, **un file per giorno di lavoro** (le voci multiple dello stesso giorno diventano sottosezioni in ordine cronologico crescente).

```markdown
# YYYY-MM-DD — <titolo>
**Branch:** <branch> · **Commit:** <sha>

## Fatto
## Scoperto        ← diagnosi e cause, la parte di valore
## Aperto
## Prossimo
```

### 5.1 Migrazione — 7 file

Contenuto **copiato fedelmente** dalle timeline esistenti, senza riscritture interpretative.

| Nuovo file | Origine | Voci |
|---|---|---|
| `2026-05-09-test-motori-brownout.md` | `docs/archive/timeline-2026-05.md` | 1 |
| `2026-05-08-microros-tethered-arm-e-diagnostica.md` | `docs/archive/timeline-2026-05.md` | 7 (Step 5+6, Step 7+8, quickstart, arm software, publisher temp, WiFi retry, WiFi/USB brownout) |
| `2026-04-26-consolidamento-documentazione.md` | `docs/archive/timeline.md` | 1 |
| `2026-03-20-pcb-custom-design.md` | `docs/archive/timeline.md` | 1 |
| `2026-03-19-motor-driver-esp32-bruciato.md` | `docs/archive/timeline.md` | 1 |
| `2026-03-11-fase-0a-sensori.md` | `docs/archive/timeline.md` | 1 (range 03-11 → 03-19) |
| `2026-03-10-architettura-e-design-spec.md` | `docs/archive/timeline.md` | 1 |

Le due timeline originali → `docs/archive/timeline-2026-05.md` e `docs/archive/timeline-2026-04.md`.

`docs/sessions/README.md`: indice cronologico inverso, una riga per sessione (data — titolo — esito).

## 6. Rinumerazione grounding

| Nuovo | Era | Ruolo |
|---|---|---|
| `docs/grounding/01-VISIONE-PROGETTO.md` | `04` | perché esiste |
| `docs/grounding/02-HARDWARE-BOM.md` | `01` | di cosa è fatto |
| `docs/grounding/03-FIRMWARE-ARCHITETTURA.md` | `02` | come è fatto il software |
| `docs/grounding/04-SETUP-AMBIENTE.md` | `03` | come si monta l'ambiente |
| `docs/grounding/05-BRINGUP-QUICKSTART.md` | `08` | come si accende |
| `docs/grounding/06-MICROROS-TETHERED.md` | `07` | come ci si parla |
| `docs/grounding/07-ARCHITETTURA-SWARM.md` | `05` | dove va |
| `docs/grounding/08-FRAME-DESIGN-GENERATIVO.md` | `06` | meccanica |

**Rischio: ciclo di rinomina.** 01→02, 02→03, 03→04, 04→01 è un ciclo: un `git mv` sequenziale sovrascriverebbe file. Mitigazione: rinomina in due fasi passando da nomi temporanei, oppure `git mv` verso `docs/grounding/` con il nome nuovo in un colpo solo (i file partono da `docs/`, la destinazione è un'altra directory → nessuna collisione). Si adotta la seconda.

**Riferimenti da riscrivere: 61** (36 ai doc numerati, 25 a `specs/`/`old/`/`pcb-custom/`), distribuiti su `docs/archive/timeline-2026-05.md`, le due spec, `02-FIRMWARE`, `08-BRINGUP`, `06-FRAME`, `01-HARDWARE`, `components/motor_driver/README.md`, `CLAUDE.md`.

**Trappola dei link relativi:** i grounding doc scendono di un livello (`docs/` → `docs/grounding/`), quindi ogni riferimento relativo che esce dalla cartella ha bisogno di un `../` in più. Casi presenti in `docs/grounding/05-BRINGUP-QUICKSTART.md`: il link al README di `ros2_ws/` e i rimandi a `docs/specs/`. È una rottura che un `sed` sui soli nomi file non intercetta — da qui la necessità del link-checker.

## 7. Correzione componenti SMD

Confermato da Angelo il 2026-09-14: la board v1.0 monta **AO3400A (SOT-23)** e **SS14 (SMA)**.

**Da correggere (documenti vivi):**

| File | Righe | Correzione |
|---|---|---|
| `CLAUDE.md` | 60 | SI2302 → AO3400A; 1N5819 → SS14 |
| `components/motor_driver/README.md` | 32-34 | componente **e** parametri elettrici (Rds_on, Vgs_th) da datasheet AO3400A |
| `docs/pcb-custom/pcb-design-spec.md` | 91,93,107,108,124,287,299,523,525 | schema, tabella componenti, BOM, footprint DO-41 → SMA |
| `docs/pcb-custom/easyeda-guida-uso.md` | 42,49 | part number LCSC e footprint |
| `docs/pcb-custom/pcb-design-teoria.md` | 48 | esempio THT: sostituire 1N5819 con un componente THT realmente presente |

**Da NON correggere (documenti storici):** `docs/archive/*`, `docs/specs/2026-03-10-*`, `docs/specs/2026-04-26-*`. Registrano scelte dell'epoca.

**Aperto:** i codici LCSC di AO3400A e SS14 **non vengono inventati**. Fino a conferma dal progetto EasyEDA reale di Angelo, in `easyeda-guida-uso.md` si scrive `DA VERIFICARE`. Motivazione: un part number sbagliato in quel documento produce un ordine sbagliato, che è esattamente il difetto che questa spec sta correggendo.

## 8. `/chiudi-sessione`

`.claude/commands/chiudi-sessione.md`:

1. Raccoglie il lavoro della sessione (`git log`, `git diff --stat`, file toccati).
2. Scrive `docs/sessions/YYYY-MM-DD-<slug>.md` (Fatto / Scoperto / Aperto / Prossimo). Se il file del giorno esiste, **appende** una sottosezione.
3. Riscrive `docs/STATO.md`: data, ultima sessione, dove siamo, blockers, prossimi 3 passi, roadmap.
4. Aggiorna `docs/sessions/README.md`.
5. Esegue il link-checker.
6. Propone il commit (non lo esegue senza conferma).

`CLAUDE.md` dichiarerà: *"Non aggiornare `STATO.md` a mano: usa `/chiudi-sessione`."* La vecchia regola sul timeline viene rimossa (è quella che ha fallito).

## 9. Riscritture

**`CLAUDE.md`** — correzione dei disallineamenti (albero struttura, roadmap, MOSFET), nuova mappa `docs/`, regola di chiusura sessione, puntatore a `STATO.md` come primo file da leggere.

**`README.md`** — vetrina del progetto per un lettore esterno: cos'è, architettura, quick start, link a `docs/README.md`. Niente stato volatile (va in `STATO.md`), niente albero di dettaglio (va in `docs/README.md`). Roadmap corretta.

**`docs/README.md`** — indice con percorsi di lettura: *"riprendo dopo una pausa" → `STATO.md`*; *"devo accendere il drone" → `grounding/05`*; *"perché questa scelta?" → `specs/`*.

## 10. Verifica

Script `docs/check-links.sh`: estrae i riferimenti a file del repo — sia link markdown `[testo](percorso)` sia percorsi citati fra backtick — e verifica che il target esista, risolvendo sia relativo al file che cita sia relativo alla radice.

La seconda forma è quella che conta davvero qui: in questo repo i rimandi sono quasi tutti in prosa fra backtick, e sono quelli che si rompono in silenzio perché nessun renderer markdown li segnala.

Per non produrre falsi positivi vengono considerati riferimenti a file solo le stringhe che finiscono con un'estensione nota e che contengono una `/` (o hanno la forma di un doc di grounding citato da un fratello). Restano quindi fuori namespace di topic, tipi di messaggio ROS, nomi di componenti e menzioni in prosa di sorgenti o comandi.

Esclusi dalla scansione: `build/`, `managed_components/`, `old/`, i sorgenti vendored di `ros2_ws/` e `docs/archive/` (i documenti archiviati citano la disposizione dei file dell'epoca, comprese spec poi eliminate, e non vanno riscritti). Exit code ≠ 0 se un riferimento è morto.

**Definizione di fatto:**

- [ ] `check-links.sh` → 0 errori, output incollato in chat
- [ ] `grep -riE 'si2302|1n5819'` → zero risultati fuori da `archive/` e `specs/`
- [ ] `grep -rn 'docs/old/\|docs/0[1-8]-\|setup-guide\.md\|HARDWARE_DIAGRAM'` → zero risultati nei doc vivi (percorsi della vecchia disposizione)
- [ ] `git log --follow` su un file spostato mostra la storia completa (prova che si è usato `git mv`)
- [ ] `STATO.md` risponde a "dove siamo" e "prossimi 3 passi" senza aprire altri file
- [ ] `idf.py build` invariato (nessun file di build toccato)

## 11. Ordine di esecuzione

1. `git checkout main && git merge feature/microros-tethered` (fast-forward: `main` fermo a `31ef7f3`, nessun conflitto atteso)
2. `git checkout -b chore/riorganizzazione-docs`
3. Commit A — **spostamenti** (`git mv`, nessuna modifica di contenuto)
4. Commit B — **riscrittura riferimenti** + link-checker
5. Commit C — **nuovi contenuti** (`STATO.md`, `sessions/`, `docs/README.md`)
6. Commit D — **correzione componenti SMD**
7. Commit E — `/chiudi-sessione` + `CLAUDE.md` + `README.md`

Commit separati per spostamento e contenuto: così `git mv` resta riconoscibile come rinomina e la storia dei file sopravvive.

## 12. Rischi

| Rischio | Mitigazione |
|---|---|
| Perdita storia file negli spostamenti | `git mv` sempre; verifica con `git log --follow` |
| Link morti dopo la rinumerazione | Link-checker in `check-links.sh`, eseguito prima di ogni commit |
| Collisione nel ciclo di rinomina 01↔04 | Spostamento diretto in `docs/grounding/` col nome nuovo (directory diversa) |
| Perdita contenuto nello split delle timeline | Copia fedele; originali conservati in `archive/`, non cancellati |
| Part number LCSC inventati | Marcati `DA VERIFICARE` fino a conferma dal progetto EasyEDA |
| `STATO.md` marcisce di nuovo | Procedura in comando esplicito + invariante "stato solo in STATO.md" |
