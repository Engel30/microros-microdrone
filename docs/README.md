# Documentazione — microros-microdrone

## Riprendi da qui

**[`STATO.md`](STATO.md)** — dove siamo, cosa blocca, prossimi tre passi. Se torni sul progetto dopo una pausa, è l'unico file che devi leggere per ripartire.

## Come è organizzata

Ogni cartella ha una responsabilità diversa e una diversa velocità di cambiamento. È la regola che tiene la documentazione onesta: se lo stato del progetto vive in un posto solo, non può esserci un secondo posto che lo contraddice.

| Cartella | Risponde a | Si aggiorna |
|---|---|---|
| [`STATO.md`](STATO.md) | *A che punto siamo?* | ogni sessione, via `/chiudi-sessione` |
| [`grounding/`](grounding/) | *Come funziona?* | quando cambia il sistema |
| [`specs/`](specs/) | *Cosa abbiamo deciso, e quando?* | mai: le spec sono datate e immutabili |
| [`sessions/`](sessions/) | *Cosa è successo quel giorno?* | mai: append-only |
| [`pcb-custom/`](pcb-custom/) | *Com'è fatta la board?* | quando cambia la board |
| [`archive/`](archive/) | *Cosa pensavamo prima?* | mai |
| [`assets/`](assets/) | schematici e immagini | — |

Un documento di `grounding/` che dichiara "Fase 0B in corso" è un bug: quell'informazione appartiene a `STATO.md`.

## Grounding — da leggere in ordine

1. [`01-VISIONE-PROGETTO.md`](grounding/01-VISIONE-PROGETTO.md) — perché il progetto esiste, obiettivi, vincoli
2. [`02-HARDWARE-BOM.md`](grounding/02-HARDWARE-BOM.md) — di cosa è fatto: BOM, pinout, connessioni
3. [`03-FIRMWARE-ARCHITETTURA.md`](grounding/03-FIRMWARE-ARCHITETTURA.md) — split dual-core, task, code inter-task
4. [`04-SETUP-AMBIENTE.md`](grounding/04-SETUP-AMBIENTE.md) — ESP-IDF, WSL2, USB passthrough
5. [`05-BRINGUP-QUICKSTART.md`](grounding/05-BRINGUP-QUICKSTART.md) — come accendere il drone, ogni sessione
6. [`06-MICROROS-TETHERED.md`](grounding/06-MICROROS-TETHERED.md) — topic, QoS, Foxglove, troubleshooting
7. [`07-ARCHITETTURA-SWARM.md`](grounding/07-ARCHITETTURA-SWARM.md) — dove va: ESP-NOW, dopo la Fase 3
8. [`08-FRAME-DESIGN-GENERATIVO.md`](grounding/08-FRAME-DESIGN-GENERATIVO.md) — frame con Fusion 360 Generative Design

## Cosa leggo quando

| Situazione | Documento |
|---|---|
| Riprendo dopo una pausa | [`STATO.md`](STATO.md) |
| Devo accendere il drone adesso | [`grounding/05-BRINGUP-QUICKSTART.md`](grounding/05-BRINGUP-QUICKSTART.md) |
| Un topic non arriva su Foxglove | [`grounding/06-MICROROS-TETHERED.md`](grounding/06-MICROROS-TETHERED.md) |
| Monto l'ambiente da zero | [`grounding/04-SETUP-AMBIENTE.md`](grounding/04-SETUP-AMBIENTE.md) + [`../ros2_ws/README.md`](../ros2_ws/README.md) |
| Devo ordinare o modificare la PCB | [`pcb-custom/pcb-design-spec.md`](pcb-custom/pcb-design-spec.md) |
| "Perché abbiamo scelto questo?" | [`specs/`](specs/) |
| "Come avevamo risolto quel bug?" | [`sessions/`](sessions/) |
| Devo toccare un componente firmware | il `README.md` dentro `components/<nome>/` |

## Manutenzione

- Le sessioni e `STATO.md` si aggiornano con **`/chiudi-sessione`**, non a mano.
- [`check-links.sh`](check-links.sh) verifica che ogni riferimento a file nella documentazione punti a qualcosa che esiste. Da eseguire prima di ogni commit che tocca i doc.

```bash
./docs/check-links.sh
```
