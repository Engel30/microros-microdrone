# 2026-09-21 — Pacco da banco 18650 e ripresa del drone dopo 4 mesi

**Branch:** `main` · **Commit:** `ecdb85e`..`b07af77` + commit di chiusura

## Fatto

**Repo.** Consolidata la chiusura del 14/09 rimasta non committata (file sessione, indice, `STATO.md`); `.vscode/` in `.gitignore`.

**Alimentazione da banco** — decisione in [`docs/specs/2026-09-21-alimentazione-banco-18650.md`](../specs/2026-09-21-alimentazione-banco-18650.md). Pacco **1S2P di Golisi G30** montato nella scheda UPS 18650 a 2 slot (carica USB-C, slot verificati in parallelo col multimetro), uscita dai pad `B+`/`B-` verso il BT2.0 del drone, cavo **20 AWG 45 cm intrecciato** (realizzato stasera), fusibile lama mini 10 A **ancora da procurare**. `docs/grounding/02-HARDWARE-BOM.md` e `docs/grounding/05-BRINGUP-QUICKSTART.md` aggiornati (voce BOM, §5.5 test di accettazione). La spec è stata rivista tre volte nella stessa serata (30Q → G30; 20 AWG 15 cm → 18 AWG 50 cm → 20 AWG 45 cm): è lo stesso giorno e la decisione non era ancora eseguita, quindi si è riscritta invece di aprirne una seconda.

**Ripresa del drone.** Primo boot sul pacco G30, monitor seriale, diagnosi del "non si connette" (era l'IP dell'agent, non il WiFi), prenotazione DHCP sul router, ricostruzione dell'ambiente di build, rebuild e flash. Fine serata: `uros_init OK`, Foxglove connesso.

**Doc grounding.** `docs/grounding/04-SETUP-AMBIENTE.md` §4/§5/§6 (miniterm senza build, errori `CHECKSUMS.json` e `IDF_TARGET`, gestione di `sdkconfig`, prenotazione DHCP, avvio agent via launch), `docs/grounding/05-BRINGUP-QUICKSTART.md` (pre-flight: mai USB e pacco insieme, IP prenotati; 4 righe in "Errori frequenti"), `docs/grounding/06-MICROROS-TETHERED.md` (topologia, troubleshooting, stato test).

## Scoperto

**Celle.** Delle 10 18650 (coppie: Samsung 30Q, Golisi G30, Avatar ICV, generiche 2600 mAh, 2 ignote), high-drain sono solo 30Q e G30. Prima scelta 30Q: una era a **0 V**; messa in carica si è **scaldata molto** → corto interno da dissoluzione del rame anodico (cella sotto ~1.5 V per mesi). Scartata. La 30Q superstite (3.1 V) resta di riserva ma da sola non fa un 2P. Le G30 erano entrambe a 3.6 V (stoccaggio): usate senza equalizzazione. Regola aggiunta alla spec: **cella sotto 2.0 V a vuoto non si carica**.

**Perché 2P e fusibile.** Il 2P serve per i contatti a molla della scheda (50–100 mΩ l'uno), non per le celle. Il fusibile serve perché i pad `B+`/`B-` sono a monte della protezione della scheda e una cella high-drain in corto dà 40–60 A (80–120 in 2P); una cella sola non lo rende omissibile.

**Cavo.** 20 AWG = 33 mΩ/m per conduttore. A 45 cm: 30 mΩ sul giro, 0.15 V a 5 A, 0.24 V a 8 A — massimo per un 20 AWG singolo; se servisse più margine, secondo 20 AWG in parallelo per polo. Il cavo tether deve arrivare al drone dall'alto.

**USB + pacco.** Il pin `VUSB` della XIAO è il VBUS senza diodo e `VBAT → SW1 → VUSB` è diretto: USB collegata con le celle sul BT2.0 = 5 V dentro le 18650. Con l'alimentatore da banco non faceva danni, ora sì. Regola in pre-flight.

**Il pacco regge.** Boot pulito, nessun `BROWNOUT`, WiFi associato in 3 s (RSSI −48/−61). Test motori sul pacco **non fatto**.

**"Non si connette al WiFi" era falso.** Il seriale mostrava `WiFi connected, IP=192.168.1.17` e il ping dal PC funzionava; la pagina del router lo marcava "non connesso" (lista client inaffidabile con un ESP32 che genera poco traffico). Il drone si fermava a `Agent non risponde` perché il firmware ha `192.168.1.9` compilato dentro, e dopo 4 mesi il lease DHCP del PC era scaduto: `.9` era finito alla **stampante HP**, il PC era `.7`. Non uno scambio, semplice scadenza dei lease. Fix definitivo scelto: prenotazione DHCP sul router (PC `.7`, drone `.15`) + rebuild con `.7`.

**Ambiente di build.** `sdkconfig` era **sparito** e `build/` vuota (mtime 14/09, durante la riorganizzazione docs); `managed_components/esp-idf-lib__*` svuotati (solo sottocartelle vuote), `micro_ros_espidf_component` intatto con `libmicroros.a` di maggio. `idf.py monitor` lanciato in quello stato ha iniziato a configurare per `esp32` (default senza `sdkconfig`) e fallito su `CHECKSUMS.json`: `build/` rimossa, `sdkconfig` ricostruito da `sdkconfig.old` (8/05, target esp32s3, ma SSID `Federico`) correggendo SSID `"WiFi LiboHouse"`, password e IP `.7`; componenti esp-idf-lib cancellati e riscaricati. Build OK al primo colpo, `libmicroros.a` riutilizzata, flash OK.

**Firmware a bordo prima del flash:** quello del 9/05, SSID `"WiFi LiboHouse"` (con lo spazio), agent `.9`. Non modificato nel codice: cambia solo la configurazione.

## Aperto

- **Fusibile mini 10 A + portafusibile non ancora montati.** Regole transitorie della spec §5 in vigore (pacco collegato solo durante il test, `SW1` a portata, cavi ispezionati).
- **Test di accettazione del pacco** (spec §6 / quickstart §5.5) non eseguito: caduta su `TP_VBAT` a 50% e 100% PWM, reset reason al boot successivo. È la verifica che chiude davvero il blocker del brownout.
- **Layout Foxglove** da rifare: `drone_1-tethered.json` in `foxglove-layouts/` non esiste.
- 30Q superstite: prova delle 24 h (tiene la tensione?) prima di considerarla riserva.
- `sdkconfig` è gitignored: se sparisce di nuovo si riparte da `sdkconfig.old`. Vale la pena tenerne una copia fuori dal repo.
- Debiti precedenti invariati: codici LCSC `DA VERIFICARE`, branch `feature/microros-tethered` da cancellare, header STORICO su `specs/2026-04-26`.

## Prossimo

1. Fusibile in linea sul `B+`.
2. Test di accettazione del pacco con i 4 motori (eliche staccate) — se passa, il blocker #1 è chiuso e la Fase 0B è completa.
3. Layout Foxglove salvato; poi Fase 1, PID di assetto.
