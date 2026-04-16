# Direzione Tesi Magistrale — Discussione 2026-04-16

**Status:** Valutazione preliminare (in attesa di completare primo drone)

---

## Contesto

Progetto partito come hobby su ESP32. Si valuta se evolvere in tesi magistrale con focus meccatronica (hardware + software + swarm).

---

## Problemi dell'approccio ESP32 puro per tesi

- WiFi stack interferisce con task real-time (Core 0 occupato)
- Non industrialmente credibile per hard real-time flight control
- ESP-NOW è protocollo proprietario Espressif → poca letteratura accademica
- Difficile da difendere come contributo originale

---

## Architettura Target (se si procede con tesi)

```
┌─────────────────────────────────┐
│         Custom PCB              │
│                                 │
│  STM32F405/H7                   │
│  ├── PID attitude 1kHz          │
│  ├── IMU (MPU6050/ICM42688)     │
│  ├── Motor PWM                  │
│  └── MAVLink UART               │
│          │ SPI                  │
│  DW3000 (UWB)                   │
│  ├── Ranging drone↔drone <10cm  │
│  ├── Comunicazione swarm        │
│  └── Sync temporale             │
└─────────────────────────────────┘
```

ESP32 opzionale come bridge WiFi→PC per dashboard, non nel critical path.

---

## Perché UWB (DW3000) è la scelta chiave

- Positioning indoor <10cm senza GPS → sostituisce optical flow per positioning
- Ranging drone-to-drone → collision avoidance e formazioni misurabili
- Comunicazione + ranging in un chip solo
- Molta letteratura accademica recente (2020-2026)
- Usato in Crazyflie (piattaforma più citata in swarm robotics)
- Chip accessibile: Qorvo DW3000 ~€10-15

---

## Contributo Originale Possibile

- Custom PCB flight controller (STM32 + UWB)
- Protocollo swarm leggero su UWB con valutazione formale (latenza, PDR, scalabilità)
- Positioning relativo drone-to-drone senza infrastruttura esterna
- Swarm behaviors: formazione, relay, collision avoidance

---

## Optical Flow — Valore Residuo

L'optical flow (PMW3901/P3901) rimane valido per:
- Velocity hold a bassa quota
- Complementare UWB (UWB dà posizione assoluta relativa, flow dà velocity locale)
- Già calibrato e funzionante sul prototipo ESP32

Non va buttato, ma non è il sensore principale per positioning nel contesto tesi.

---

## Roadmap Tesi (stimata, 6-12 mesi)

| Fase | Durata | Attività |
|------|--------|----------|
| 0 | ora | Completare prototipo ESP32, validare che il settore piace |
| 1 | mesi 1-2 | Prototipo su devboard STM32 + modulo DW3000, firmware base |
| 2 | mesi 3-4 | PCB custom, integrazione completa |
| 3 | mesi 5-6 | Swarm 3-4 droni, misurazioni formali |
| 4 | finali | Scrittura tesi |

---

## Prossimo Step

Completare almeno 1 drone ESP32 funzionante (componenti in arrivo) per validare interesse nel settore prima di decidere se procedere con architettura tesi.

---

## Note

- Codice ESP32 esistente non è buttato: logica IMU/flow/PID portabile su STM32 con HAL
- Riferimento open source: Crazyflie 2.x (STM32F405 + nRF51822, schema pubblico)
- Relatore non ancora definito — da valutare quanto questo vincola l'ambizione hardware
