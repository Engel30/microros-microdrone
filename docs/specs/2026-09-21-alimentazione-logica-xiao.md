# Alimentazione della logica: disaccoppiare il rail 3.3 V della XIAO dal rail motori

**Data:** 2026-09-21
**Autore:** Angelo + Claude
**Stato:** Proposta — opzioni documentate, decisione da prendere
**Completa:** [`2026-09-21-alimentazione-banco-18650.md`](2026-09-21-alimentazione-banco-18650.md) (che risolve la sorgente, non il rail logico)

---

## 1. Problema

Sulla PCB v1.0 il pin `VUSB` della XIAO ESP32-S3 sta sul rail motori (`VBAT_SW`: `SW1` → `VUSB` + `SW2` → motori, [`pcb-custom/pcb-design-spec.md`](../pcb-custom/pcb-design-spec.md) §3.1). Il 3.3 V dell'ESP32 lo genera l'LDO interno della XIAO da `VUSB`. Con una cella Li-ion sotto ~3.9 V l'LDO è in **dropout** e il rail 3.3 V segue `VUSB` uno a uno.

Misure del 2026-09-21 ([sessione §2](../sessions/2026-09-21-alimentazione-18650-e-ripresa.md)), celle G30 a 3.6 V:

| Condizione | 3V3 XIAO | Esito |
|---|---|---|
| riposo | 3.26 V | — |
| 3 motori ~23% senza eliche (~0.7 A) | 3.17 V | ok |
| 4 motori 70% senza eliche a regime | non misurato | ok |
| 3 motori 30% da fermo senza eliche | — | `BROWNOUT` |
| 4 motori **15% con eliche** | — | `BROWNOUT` |
| 3 motori **20% con eliche** | — | `BROWNOUT` |

La soglia di brownout dell'ESP32-S3 è 2.51 V (`CONFIG_ESP_BROWNOUT_DET_LVL=7`, già il minimo). Da 3.26 V ci sono **0.75 V** di margine totale, che vengono consumati da: caduta sul percorso (contatti a molla + cavo + BT2.0 + `SW1` ≈ 100–150 mΩ), dropout dell'LDO che cresce con la corrente dell'ESP32 (burst WiFi 300–400 mA), ripple a 20 kHz del PWM. Senza eliche la corrente a regime è trascurabile e solo i transitori superano il margine (coperti dallo spin-up in `task_motors`). Con le eliche 4 motori a 15% tirano 1.5–2 A continui e il margine sparisce a regime. In hover servono 50–60% su 4 motori, 4–6 A: **il regime di volo è irraggiungibile per costruzione**, non per taratura.

Il criterio di accettazione della spec precedente (§6, "caduta su `TP_VBAT` < 0.3 V") era sbagliato: presuppone che 0.3 V siano tollerabili, ma con l'LDO in dropout non lo sono. Il criterio corretto è **3V3 della XIAO ≥ 3.0 V** in ogni condizione di carico, con margine su 2.51 V.

Il problema non è del pacco da banco: una LiPo 1S di volo sotto carico a fine scarica sta a 3.4–3.5 V, e riproduce la stessa condizione. Va risolto sulla PCB.

## 2. Opzioni

### A. Ricaricare le celle a 4.2 V (verifica, non soluzione)

A 4.2 V il rail parte a 3.3 V pieni con ~0.6 V prima del dropout. Costo zero, da fare comunque prima di ogni test. Se 4 motori a 15% con eliche passano a 4.2 V e falliscono a 3.6 V, la diagnosi è confermata e si sa quanto si è corti. **Non è una soluzione:** a metà scarica (3.7 V) si torna in dropout, e ogni test dipenderebbe dallo stato di carica.

### B. Boost 1S → 5 V sul pin `VUSB` — **raccomandata**

Un piccolo boost (MT3608, TPS61023, TPS61088; moduli da 1–2 g) tra `VBAT_SW` e `VUSB`. L'LDO della XIAO vede 5 V, esce dal dropout per sempre e il 3V3 resta a 3.3 V con la cella che sagga fino a 2.5–2.7 V (limite di ingresso del boost, sotto il quale la cella è comunque da fermare).

```
VBAT_SW ──┬──[C 1000 µF]──┬──► BOOST IN    BOOST OUT (5.0 V) ──► XIAO VUSB
          │               │       │                                   │
          │              GND     GND                              LDO interno → 3V3
          └──► SW2 → VBAT_MOTORS
```

| Aspetto | Nota |
|---|---|
| Corrente | ESP32-S3 con WiFi: 150 mA medi, 400–500 mA di picco a 3.3 V → ~0.5 A dalla cella a 3.6 V con η ≈ 85%. Un MT3608 (switch 2 A) o un TPS61023 (max 1 A out) bastano |
| Ingresso | 1000 µF low-ESR sull'ingresso del boost, vicino alla XIAO, per i dip di ms; il boost regola il regime |
| Uscita | regolata a 5.0 V (trimmer sul MT3608, fisso sui TPS); 22 µF ceramico in uscita oltre a quelli del modulo |
| Rumore | il boost commuta a 0.5–1.2 MHz; l'LDO della XIAO lo attenua. Tenerlo lontano dal GY-521 e dal ramo ADC batteria |
| USB | resta la regola **mai USB-C e pacco insieme**: `VUSB` è il VBUS della XIAO senza diodo, e con il boost ci sarebbero due sorgenti a 5 V in parallelo |
| Peso | 1–2 g, entra nel budget del drone |
| Modifica PCB v1.0 | isolare il pin `VUSB` da `VBAT_SW`: se la XIAO è su header femmina, estrarre il pin `VUSB` e cablarlo all'uscita del boost; altrimenti taglio della pista/pour a ridosso del pin. Da verificare sul layout prima di tagliare |
| PCB v2 | boost integrato (TPS61023 in SOT-23-6 + induttore 2.2 µH), rail `5V_LOGIC` separato |

Conseguenza positiva: con il rail logico disaccoppiato, il sag della batteria conta solo per la spinta dei motori, e la soglia empirica "Σ Δduty ≈ 80–90" scompare come vincolo del PID.

### C. Buck-boost 3.3 V direttamente sul pin `3V3`

Un TPS63001/TPS63020 (modulo) da `VBAT_SW` al pin `3V3`, bypassando l'LDO della XIAO. Una conversione sola, efficienza migliore. **Scartata:** il pin `3V3` è l'uscita dell'LDO interno; alimentarla dall'esterno mette l'LDO in back-feed (tollerato da alcuni regolatori, non garantito), e con la USB collegata i due regolatori si contendono il rail. Più fragile di B per un vantaggio marginale.

### D. Batteria separata per la logica

Una LiPo 1S da 100–150 mAh solo per la XIAO, massa comune. Disaccoppiamento totale, zero elettronica. **Utile solo sul banco:** in volo sono due batterie da caricare, 4–5 g in più, e una batteria logica che si scarica prima di quella motori è un failsafe da gestire. Non risolve il dropout: anche la LiPo logica a fine scarica è sotto 3.9 V. Scartata.

### E. Ridurre la resistenza del percorso — **necessaria, non sufficiente**

Fusibile a lama in portafusibile a bassa resistenza, cavo 20 AWG doppio, `SW1` su switch a bassa resistenza o bypassato durante i test al banco, celle con linguette saldate invece dei contatti a molla. Riduce la parte "caduta sul percorso" del problema, ma con la cella a 3.6 V il margine è 0.75 V **anche a resistenza zero**, e il ripple a 20 kHz del PWM più il dropout dell'LDO lo consumano comunque in hover. Va fatta insieme a B, non al posto di B.

### F. Abbassare o disabilitare il brownout detector

La soglia è già al minimo (2.51 V). Disabilitarlo (`CONFIG_ESP_BROWNOUT_DET=n`) non allarga il margine: sotto ~3.0 V il PA WiFi perde potenza e la flash SPI può corrompersi durante una scrittura. Scartata.

### G. Banco di condensatori su `VUSB`

2200 µF e oltre sul solo ramo XIAO, con Schottky verso `VBAT_SW` per non scaricarli nei motori. Il diodo costa 0.3 V di caduta in regime, peggio del problema; senza diodo il condensatore copre solo dip sotto il millisecondo. Scartata.

## 3. Raccomandazione

**B + E**, con A come verifica immediata.

1. Celle a 4.2 V, ripetere 4 motori a 15% con eliche: conferma della diagnosi e misura del margine (3V3 sotto carico con multimetro).
2. Procurare un boost (MT3608 se in cassetto, TPS61023 se si ordina) e 1000 µF; verificare sul layout come isolare `VUSB`.
3. Montare, misurare 3V3 sotto carico fino a 4 motori a 70% con eliche (eliche montate = spinta reale: drone vincolato al banco).
4. Fusibile e portafusibile a bassa resistenza (E) nella stessa sessione.
5. Nuovo criterio di accettazione: **3V3 ≥ 3.0 V** a 4 motori 100% con eliche, nessun `BROWNOUT` al boot dopo.

Lo spin-up in `task_motors` resta: è una protezione a costo zero e diventa la fase di decollo. Con B in opera, la sosta a 8% potrà scendere sotto i 2 s.

## 4. Riferimenti

- Diagnosi e misure: [`sessions/2026-09-21-alimentazione-18650-e-ripresa.md`](../sessions/2026-09-21-alimentazione-18650-e-ripresa.md) §2
- Rail della PCB v1.0: [`pcb-custom/pcb-design-spec.md`](../pcb-custom/pcb-design-spec.md) §3.1 (net `VBAT_SW`)
- Spin-up firmware: `components/motor_driver/README.md`
- Sorgente da banco: [`specs/2026-09-21-alimentazione-banco-18650.md`](2026-09-21-alimentazione-banco-18650.md)
