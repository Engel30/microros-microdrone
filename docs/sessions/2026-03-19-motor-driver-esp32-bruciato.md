# 2026-03-19 — Fase 0B: motor driver, ESP32 bruciato

**Commit:** `07c16e4` (bruciato esp)

## Fatto

Implementato `motor_driver` (LEDC PWM 20kHz, 10-bit, 4 canali).

## Scoperto — l'incidente

Al primo test motori **l'ESP32 si è bruciato**: cortocircuito 3V3-GND permanente.

Catena causale:
1. I GPIO dell'ESP32 sono **flottanti durante il boot**, prima che il firmware configuri i pin.
2. I gate dei MOSFET seguono il flottante → i MOSFET si accendono in modo casuale.
3. I motori coreless partono a caso, generando spike induttivi.
4. Gli spike distruggono il regolatore 3.3V.

**Mancavano le pull-down 10kΩ esterne sui gate.** Le pull-down interne dell'ESP32 non bastano: si attivano solo *dopo* il boot, cioè dopo la finestra in cui il danno avviene.

## Regola che ne deriva

Pull-down 10kΩ fra gate e source di ogni MOSFET, **obbligatoria**, prima di ricollegare qualsiasi motore. Recepita nel design della PCB v1.0 (vedi `2026-03-20-pcb-custom-design.md`), insieme a una resistenza serie 100Ω su ogni gate e a uno switch arm hardware separato.

## Aperto

Fase 0B bloccata in attesa di un nuovo ESP32. Si sbloccherà solo con la PCB v1.0.
