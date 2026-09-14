# 2026-03-20 — PCB custom: design

**Commit:** `a6df2a2` (design pcb)

Il giorno dopo l'incidente dell'ESP32 bruciato (vedi `docs/sessions/2026-03-19-motor-driver-esp32-bruciato.md`): la PCB custom nasce come risposta diretta a quell'incidente. Entrambi i commit portano data 2026-03-20; la timeline dell'epoca datava l'incidente al 19.

## Fatto

- Analisi della causa del burning del regolatore 3.3V.
- Design spec PCB completato in `docs/pcb-custom/pcb-design-spec.md`:
  - Carrier board 2-layer, componenti solo sul top, ground plane pieno sul bottom
  - 4 driver motori con **pull-down 10kΩ + serie 100Ω su ogni gate** — la lezione dell'incidente, resa struttura
  - **Switch arm motori separato**, protezione hardware indipendente dal firmware
  - JST-PH per motori e switch, BT2.0 femmina sulla PCB
  - Target: ≤45×35mm, spessore 1.0mm, peso PCB ≤3g
- Guida teoria PCB per principianti: `docs/pcb-custom/pcb-design-teoria.md`
- Guida uso EasyEDA Std: `docs/pcb-custom/easyeda-guida-uso.md`

## Deciso

Passaggio da perfboard a PCB custom per risolvere ground loop e protezioni. EDA scelto: EasyEDA Std → JLCPCB.

## Prossimo

Disegnare lo schematic in EasyEDA, poi il layout, poi ordinare.
