# Sessioni di lavoro

Una sessione = un giorno di lavoro. Ogni file registra cosa è stato fatto, **cosa è stato scoperto** (le diagnosi: è la parte che vale), cosa resta aperto e qual è il passo successivo.

Questi file sono **append-only**: non si riscrivono. Lo stato corrente del progetto non sta qui, sta in [`../STATO.md`](../STATO.md).

I file vengono creati da `/chiudi-sessione`.

| Data | Sessione | Esito |
|---|---|---|
| 2026-09-21 | [Pacco da banco 18650 e ripresa del drone; §2 test motori e spin-up](2026-09-21-alimentazione-18650-e-ripresa.md) | Pacco 1S2P G30 costruito; DHCP prenotato, rebuild, flash, Foxglove connesso. §2: senza eliche 4 motori al 70% ok e spin-up in `task_motors` per i transitori; **con eliche brownout già a 15%**: rail 3.3 V sul rail motori, LDO in dropout → spec opzioni (boost su `VUSB`) |
| 2026-09-14 | [Riorganizzazione documentale e sistema di continuità sessioni](2026-09-14-riorganizzazione-documentale.md) | docs/ ristrutturata; STATO.md e /chiudi-sessione; componenti PCB corretti |
| 2026-05-09 | [Test motori: brownout buck-boost + console Foxglove](2026-05-09-test-motori-brownout.md) | Motori girano; brownout del buck-boost sopra il 20% PWM |
| 2026-05-08 | [micro-ROS tethered: publisher, subscriber, arm, diagnostica WiFi](2026-05-08-microros-tethered-arm-e-diagnostica.md) | Telemetria completa su Foxglove; WiFi flapping risolto (era alimentazione) |
| 2026-04-26 | [Consolidamento documentazione](2026-04-26-consolidamento-documentazione.md) | Source of truth unica; pivot STM32+UWB scartato |
| 2026-03-20 | [PCB custom: design](2026-03-20-pcb-custom-design.md) | Design spec PCB v1.0 con pull-down e switch arm |
| 2026-03-19 | [Motor driver, ESP32 bruciato](2026-03-19-motor-driver-esp32-bruciato.md) | Fase 0B bloccata; nasce la regola delle pull-down 10kΩ |
| 2026-03-11 → 03-19 | [Fase 0A: sensori](2026-03-11-fase-0a-sensori.md) | Fase 0A completata |
| 2026-03-10 | [Architettura e design spec](2026-03-10-architettura-e-design-spec.md) | Spec fondativa ESP-IDF + micro-ROS |

Le due timeline originali da cui queste sessioni sono state estratte sono in [`../archive/`](../archive/).
