# 2026-03-10 — Architettura e design spec

**Commit:** `188003c` (New archiecture micro-ROS)

## Fatto

- Scritto il design spec completo: `docs/specs/2026-03-10-swarm-drone-architecture-design.md`.
- Definita l'architettura ESP-IDF + FreeRTOS + micro-ROS.
- Pinout definitivo, task allocation dual-core, sistema di code inter-task.
- Completata la migrazione concettuale da Arduino a ESP-IDF.

## Nota

È la spec fondativa del progetto. Le parti su hardware, sensori, split dual-core, code e fasi 0-3 sono rimaste valide; la parte sulla comunicazione swarm (centralizzata via micro-ROS) è stata superata dal brainstorming del 2026-04-14 in favore di ESP-NOW peer-to-peer.

Il vecchio firmware Arduino resta in `old/` come riferimento per la logica dei sensori.
