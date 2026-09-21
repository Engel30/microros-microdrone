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

---

# 2 — Test motori sul pacco: spin-up in `task_motors`, poi con le eliche il rail 3.3 V non regge

**Branch:** `main` · **Commit:** commit di chiusura (firmware + docs)

## Fatto

**Diagnosi del reset con più motori.** Sintomo iniziale da Foxglove: 1 motore @30% ok, 2 @30% reset; più tardi nella stessa sessione 2 @30% ok, 3 @30% reset. Reset reason `BROWNOUT` su `/drone_1/log`. Alimentazione: pacco 1S2P G30 via BT2.0, celle a 3.65 V, senza fusibile.

**`ros2_ws/tools/motor_steps.py`** — nuovo. Publisher `rclpy` unico a 10 Hz che attraversa una sequenza di duty `"FL,RL,RR,FR:secondi"` senza interruzioni (con `ros2 topic pub` ogni cambio valore costa > 500 ms → watchdog → motori a 0). Zero esplicito a fine sequenza e a Ctrl+C; esce se nessun subscriber aggancia il topic entro 5 s. Documentato in `ros2_ws/README.md`. Nota: il primo lancio di verifica dello script (fatto da Claude per testarne il parsing) ha trovato agent e drone connessi e **ha pubblicato davvero** `[10,10,10,0]` e `[30,30,30,0]` per 3 s; nessun danno, ma da ora lo script si lancia solo su richiesta.

**Spin-up in `task_motors`** — modulo `components/motor_driver/motor_spinup.{h,c}`, C puro senza dipendenze ESP-IDF, con test su host (`components/motor_driver/test/run.sh`, gcc, 5 casi: primo test automatico del firmware). Regola, per ogni motore: da 0 si esce solo con rampa lineare (`MOTOR_SPINUP_RAMP_MS` 150) fino a `MOTOR_SPIN_MIN_PCT` (8%) e sosta (`MOTOR_SPINUP_HOLD_MS` **2000**); sopra, il comando passa intatto. Il vincolo sta nel layer motori e non nel decollo perché è una proprietà dell'attuatore: vale per `cmd_motor_test` oggi, PID domani, failsafe dopodomani. `task_motors` lo applica dopo watchdog e arm gate; l'echo `/drone_1/motors` riporta il duty applicato; log INFO `motors: spin-up mask=0x..` a ogni partenza. **Decisione:** arm resta "motori a 0" (Angelo non vuole eliche che girano all'arm); la sequenza di decollo di Fase 1 (sosta a basso regime, poi inseguimento del riferimento) sarà una conseguenza di questa regola.

File: `components/motor_driver/{motor_spinup.c,include/motor_spinup.h,test/,CMakeLists.txt,README.md}`, `components/common/include/drone_config.h` (3 `#define`), `main/task_motors.c`, `docs/grounding/05-BRINGUP-QUICKSTART.md` (§5.2b spin-up, §5.3 script), `docs/grounding/03-FIRMWARE-ARCHITETTURA.md` (riga `task_motors`).

## Scoperto

**Il pacco e il percorso reggono il regime.** Con `[25,20,25,0]` fisso il 3V3 della XIAO passa da 3.26 V (riposo) a 3.17 V: LDO già in dropout con celle a 3.65 V (ogni mV perso sul percorso arriva dritto all'ESP32), percorso ~100–150 mΩ. Rampa `10→20→30→50→70` su 4 motori con gradini di 3 s: **nessun reset**. Il picco sincrono a 20 kHz dei canali LEDC (fronti allineati) è escluso: al 4×70% è al massimo e non fa nulla.

**Il brownout è da gradino di corrente su motori non a regime.** Modello che spiega tutti i dati: a un gradino di duty la corrente sale con la costante elettrica (L/R ≈ 100 µs) a Σ(D_nuovo − E_i)/R, dove la back-EMF E_i insegue il duty con la costante **meccanica**, lunga a basso duty senza eliche (frizione dominante). Tabella dei gradini:

| Gradino | Esito |
|---|---|
| 0→10, 10→20 su 3; 20→30 su 3 + 0→30 sul 4°; 30→50 e 50→70 su 4 | ok |
| 0→30 su 2 (Foxglove); 0→25,20,25 | ok |
| 0→30 su 3 | BROWNOUT |
| 8→30 su 4 dopo **300 ms** all'8% (primo spin-up, ramp 150 + hold 300) | BROWNOUT |
| 8→30 su 4 dopo **2 s** all'8% (`"8,8,8,8:2" "30,30,30,30:2"`) | ok |
| 8→15→22→30 su 4 (gradini di ~30 punti totali) | ok |
| 0→30 su 2, poi gli altri 2 (Δ 60 + 60) | ok |
| `"30,30,30,30:3"` da fermo con spin-up hold 2000 ms | **ok** |
| **con eliche:** 3 motori 20%, 4 motori 15% (spin-up attivo, celle 3.6 V) | **BROWNOUT** |

Soglia empirica a 3.65 V: Σ Δduty ≈ 80 passa, ≈ 90 no, **ma solo se i motori sono a regime meccanico**. L'ipotesi intermedia "conta solo Σ Δduty" è stata falsificata dal confronto 300 ms vs 2 s a parità di Δ (88). Lo spin-up con hold 300 ms era il fix n.1, fallito; il fix n.2 è stato cambiare solo il parametro (300 → 2000 ms), la logica era giusta.

**Il multimetro non vede il brownout** (media su 200–400 ms, dip di ms, e il reset spegne il carico). Metodo che funziona: carico stazionario sotto soglia + misura differenziale ai capi dei tratti sospetti, oppure la tabella dei gradini con lo script.

**Foxglove Publish one-shot** ora non raggiunge mai il duty richiesto (watchdog 500 ms < spin-up 2.15 s): test veri solo da CLI/script.

**Con le eliche il regime non regge.** Eliche montate (drone vincolato, tutti e 4 i motori girano, anche all'8% dello spin-up): 3 motori a 20% e 4 motori a 15% vanno in `BROWNOUT` subito dopo la fine dello spin-up ("uno sprint di velocità, poi si spegne"). Non è più un transitorio: con l'elica la back-EMF non cancella la corrente e 4 motori a 15% tirano 1.5–2 A **continui**; con celle a 3.6 V (mai ricaricate) il 3V3 parte a 3.26 V, LDO in dropout, 0.75 V di margine totale sulla soglia di 2.51 V, consumati da percorso + dropout + ripple PWM. In hover servono 4–6 A: **il regime di volo è irraggiungibile per costruzione** finché il pin `VUSB` della XIAO sta sul rail motori. Lo spin-up resta utile (transitori, decollo) ma non è la soluzione. Il criterio di accettazione della spec del pacco (§6, "caduta < 0.3 V su `TP_VBAT`") era sbagliato: con l'LDO in dropout 0.3 V non sono tollerabili; criterio corretto **3V3 ≥ 3.0 V sotto carico**.

**Opzioni documentate** in [`docs/specs/2026-09-21-alimentazione-logica-xiao.md`](../specs/2026-09-21-alimentazione-logica-xiao.md): raccomandata **boost 1S → 5 V su `VUSB`** (+ 1000 µF, + riduzione resistenza del percorso); scartate buck-boost sul 3V3, batteria logica separata, disabilitare il brownout, banco di condensatori. Decisione non ancora presa.

**Il "motore che non gira"** segnalato a metà sessione: al test successivo tutti e 4 giravano. Non isolato, da tenere d'occhio.

## Aperto

- **Rail 3.3 V della XIAO sul rail motori: blocker di volo.** Decisione tra le opzioni della spec (raccomandata B: boost su `VUSB`). Verifica sul layout PCB v1.0 di come isolare il pin `VUSB` dalla net `VBAT_SW`.
- **Celle mai ricaricate** (3.6 V): tutti i test di oggi sono a margine minimo. A 4.2 V la stessa prova con eliche misura quanto si è corti.
- **Hold 2000 ms non ottimizzato**: 1 s e 0.5 s non provati (`"8,8,8,8:1" "30,30,30,30:2"`). 2 s per un decollo è accettabile.
- **Margine stretto e dipendente dalla carica**: soglia misurata a 3.65 V con LDO in dropout; a 4.2 V ci sono ~0.5 V in più sul rail. Le celle **non sono state ricaricate** durante la sessione e la tensione non è stata rimisurata dopo i test.
- **Limitatore di gradino collettivo per Fase 1**: lo spin-up copre l'uscita da 0; una variazione collettiva rapida di throttle (> ~20 punti su 4 motori) in volo supera la stessa soglia. Le correzioni differenziali del PID (somma ≈ 0) non la toccano. Da decidere quando c'è il PID; annotato in `components/motor_driver/README.md`.
- **Test di accettazione della spec §6 da rifare con il criterio corretto** (3V3 ≥ 3.0 V sotto carico, con eliche): quello scritto non è più valido.
- **Fusibile** ancora assente.
- Il modulo `motor_driver` in modalità `[2] Motor test` (menu seriale) non passa dallo spin-up: solo `task_motors` lo applica.

## Prossimo

1. Celle a 4.2 V, 4 motori a 15% con eliche, multimetro sul 3V3: conferma della diagnosi e misura del margine.
2. Decidere l'opzione della spec alimentazione logica (boost su `VUSB`), procurare boost + 1000 µF, verificare sul layout come isolare `VUSB`, montare, misurare 3V3 fino a 4 motori 70% con eliche. Fusibile a bassa resistenza nella stessa sessione.
3. Solo dopo: Fase 1 — PID di assetto.
