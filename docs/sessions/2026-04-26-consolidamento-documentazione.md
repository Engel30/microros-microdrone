# 2026-04-26 — Consolidamento documentazione

**Commit:** `b13a17d` (allineate doc), `49bad9b` (refactor docs, analisi per tesi)

## Fatto

- Ripresa del progetto in attesa dei nuovi ESP32-S3 (sostituzione di quello bruciato il 2026-03-20).
- Allineamento di tutte le spec in `docs/specs/` sotto una source of truth unica: `docs/specs/2026-04-26-stato-progetto-e-roadmap.md`.
- Header "STORICO" apposto su `2026-03-10-swarm-drone-architecture-design.md` e `2026-04-14-architettura-swarm-brainstorming.md`.
- Allineati `docs/grounding/03-FIRMWARE-ARCHITETTURA.md`, `docs/grounding/01-VISIONE-PROGETTO.md`, `docs/grounding/07-ARCHITETTURA-SWARM.md`, `CLAUDE.md`.

## Deciso

- **micro-ROS resta sul singolo drone fino a fine Fase 3**; ESP-NOW subentra con la fase swarm. Motivo: il tooling ROS2 (rosbag, PlotJuggler, Foxglove) è prezioso durante il tuning del PID, e il flight control è disaccoppiato dal trasporto — la migrazione tocca solo il task di comunicazione.
- **Pivot STM32+UWB scartato**, con eliminazione della relativa spec `2026-04-16-tesi-magistrale-direzione.md`. Il progetto resta su ESP32-S3 + optical flow per il positioning.

## Nota

Questo documento di consolidamento è a sua volta stato superato il 2026-09-14, quando lo stato volatile è passato a `docs/STATO.md`. Vedi `docs/specs/2026-09-14-riorganizzazione-documentale.md`.
