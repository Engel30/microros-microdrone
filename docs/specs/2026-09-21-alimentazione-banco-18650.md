# Alimentazione da banco: pacco 1S2P Golisi G30 su scheda UPS 18650

**Data:** 2026-09-21
**Autore:** Angelo + Claude
**Stato:** Approvata — da costruire e collaudare
**Revisione:** 2026-09-21 sera — celle cambiate da Samsung 30Q a Golisi G30 (vedi §3.1)
**Sostituisce:** il buck-boost AliExpress usato nei test del 2026-05-09

---

## 1. Problema

Il [test motori del 2026-05-09](../sessions/2026-05-09-test-motori-brownout.md) ha chiuso con un blocker: alimentando la PCB v1.0 da bench 5 V → buck-boost → 4.1 V, il drone si resetta per `BROWNOUT` sopra il 20% di PWM su due o più motori. Il buck-boost ha transient response insufficiente: al fronte PWM dei coreless 8520 il rail collassa sotto la soglia di brownout dell'ESP32-S3.

La soluzione indicata a maggio era una LiPo 1S 25C 300–600 mAh, che resta la batteria **di volo**. Ma per il bring-up e il tuning PID in tethered serve una sorgente da banco che:

1. eroghi i ~8 A di picco dei 4 motori senza saggare
2. stia nel range 3.0–4.2 V che PCB v1.0 (`VBAT` via BT2.0) e firmware (`BATTERY_LOW 3.3 V`, `BATTERY_CRITICAL 3.0 V`) si aspettano
3. duri ore, non minuti
4. si ricarichi senza attrezzatura dedicata

## 2. Materiale disponibile

- 10 celle 18650, in coppie: 2× Samsung INR18650-30Q, 2× Golisi G30, 2× Avatar "ICV", 2× generiche "2600mAh 3.7V", altre 2 non identificate
- Scheda "UPS 18650" a 2 slot: carica 1S via USB-C, uscita boost regolabile 5/9/12 V marcata 3 A, pad `B+`/`B-` delle celle esposti. **Verificato col multimetro:** i pad `+` dei due slot sono in corto tra loro, idem i `-` → gli slot sono in **parallelo (1S2P)**
- Holder 18650 a 4 slot, cablaggio non verificato

## 3. Decisione

**Pacco 1S2P con le due Golisi G30, montate stabilmente nella scheda UPS.** USB-C per la ricarica. Uscita presa direttamente dai pad `B+`/`B-`, con fusibile in serie, verso il BT2.0 del drone. L'uscita boost della scheda non si usa. L'holder da 4 slot non si usa.

### 3.1 Perché le G30

| Coppia | Chimica | Corrente continua | Tensione a vuoto (21/09) | Esito |
|---|---|---|---|---|
| **Golisi G30** | NMC (etichetta "IMR", rewrap) | 20 A dichiarati, ~15–20 A nei test indipendenti | 3.6 V entrambe | **scelta** |
| Samsung INR18650-30Q | NMC | 15 A (20 A con limite termico) | una 3.1 V, una **0 V** | una scartata, una di riserva |
| Avatar "ICV" | probabile LiCoO₂ | ~5 A | — | esclusa |
| Generiche 2600 mAh | ICR generica / recupero | ~5 A | — | esclusa |

La prima scelta erano le 30Q. Durante la carica di equalizzazione, la 30Q trovata a 0 V **si è scaldata molto**: sintomo di corto interno da dissoluzione del rame (§5, passo 0). Scartata. La 30Q superstite da sola non fa un 2P (serve lo stesso modello), quindi si passa alla coppia G30, trovate entrambe a 3.6 V, cioè a tensione di stoccaggio e già equalizzate.

Con 8 A di picco le G30 in 2P lavorano al 20–25% del rating. Le ICR in 2P arriverebbero a ~10 A al limite, con sag paragonabile al buck-boost.

Tutte le 18650 sono Li-ion (cilindriche, elettrolita liquido). La differenza tra sigle è il catodo, quindi la corrente erogabile; il range di tensione 2.5/3.0–4.2 V è lo stesso per INR e ICR. Nessuna cella IFR (LiFePO₄, 3.2 V) tra quelle disponibili: sarebbe stata incompatibile.

### 3.2 Perché 2P e non una cella sola

Non per le celle: una G30 da sola coprirebbe gli 8 A. **Per i contatti a molla** dell'holder: 50–100 mΩ ciascuno, non progettati per 8 A. Con una cella sola i 8 A passano su due molle in serie → 0.8–1.6 V di caduta → brownout identico a quello del buck-boost. Con due slot in parallelo i percorsi di contatto sono in parallelo e la caduta si dimezza. In più: 6000 mAh di autonomia al banco e 4 A per cella.

Costo del 2P: le due celle vanno **equalizzate prima del primo inserimento** (§5). Dopo, caricandosi sempre insieme, restano gemelle.

### 3.3 Perché il fusibile è obbligatorio

I pad `B+`/`B-` sono a monte della protezione della scheda. In corto (filo sfilato, ponte di stagno, MOSFET in corto — guasto tipico), la corrente è limitata solo dalla resistenza interna: una cella high-drain ha IR ≈ 20–25 mΩ → 40–60 A; in 2P 80–120 A. Un 20 AWG diventa rosso in secondi, la cella sfiata oltre 30–40 A. **Una cella sola non rende sicuro omettere il fusibile**: il rapporto corto/portata del filo è già 4–5×.

Le LiPo 1S da micro-drone non montano fusibili perché hanno IR 50–80 mΩ e capacità piccola. Una 18650 da torcia/trapano è un ordine di grandezza più "cattiva" in corto.

### 3.4 Perché non le alternative

- **Uscita boost 5 V della UPS → buck-boost esistente:** stesso problema di transient response di maggio, con un convertitore in più in cascata.
- **Holder da 4 slot:** cablato quasi certamente in 4S (14.8 V → distrugge la XIAO), da ricablare; le celle andrebbero spostate a ogni ciclo di carica; un 1S4P non è possibile con coppie di modelli diversi (§4.1 sotto). Nessun vantaggio rispetto alla scheda UPS.
- **Celle diverse in parallelo:** si spartiscono la corrente in proporzione inversa alla IR; la più "forte" lavora e invecchia di più, la più piccola viene trascinata a fine scarica. Il degrado di una cella resta nascosto dall'altra.

## 4. Schema

```
Scheda UPS 18650 (1S2P)
 ┌─────────────────────┐
 │ [G30] ──┬── B+ ─────┼──[FUSIBILE 10 A mini]──── 20 AWG ────┐
 │ [G30] ──┘           │                                      │
 │         ┌── B- ─────┼──────────────────────── 20 AWG ────┐ │
 │ [G30] ──┤           │                                    │ │
 │ [G30] ──┘           │                              ┌─────┴─┴─────┐
 │  USB-C ── charger   │                              │ BT2.0 maschio│
 │  OUT 5/9/12V (n.c.) │                              └──────┬───────┘
 └─────────────────────┘                                     │
                                          PCB v1.0: BT2.0 → VBAT → SW1 → XIAO VUSB + motori
```

| Elemento | Specifica | Motivazione |
|---|---|---|
| Fusibile | lama automotive **mini (ATM/APM) 10 A** (rossa), 32 V DC | contatti larghi, pochi mΩ; tiene 100% nominale indefinitamente, apre al 200% in secondi, in ms su un corto da 40–100 A |
| Portafusibile | volante a spina con code 16–18 AWG e cappuccio | in serie su `B+`, saldato, **vicino alla scheda UPS** (le code contano nella lunghezza) |
| Non usare | vetro 5×20 (portafusibili da 6.3 A, clip a decine di mΩ), PTC/polyfuse (interviene in secondi, 30–80 mΩ propri) | aggiungono la caduta che stiamo eliminando |
| Cavi | **20 AWG silicone, 45 cm per conduttore, coppia intrecciata** (realizzato il 21/09) | 45 cm servono da tether per il tuning PID in Fase 1. 20 AWG = 30 mΩ andata+ritorno → 0.15 V a 5 A (hover), 0.24 V a 8 A: è il massimo per un 20 AWG singolo (sotto i ~3.6 V di cella l'LDO della XIAO va in dropout). Se servisse più margine: secondo 20 AWG in parallelo per polo (→ 15 mΩ, 0.12 V a 8 A). Il twist dimezza l'induttanza; i fronti di commutazione li fornisce `C1` sulla PCB |
| Tether | il cavo arriva al drone **dall'alto**, appeso con un'ansa lasca | un cavo che tira dal basso è un disturbo di coppia e di peso sulla taratura PID |
| Connettore | pigtail BT2.0 maschio (riuso di quello sull'uscita del buck-boost) | compatibile con `J_BAT` della PCB v1.0 |
| Scorta | 2–3 fusibili da 10 A | se scatta, si indaga la causa, non si resta fermi |

**Nessuna modifica firmware.** `BATTERY_LOW_VOLTAGE 3.3 V` e `BATTERY_CRITICAL_VOLTAGE 3.0 V` (`drone_config.h`) sono conservative rispetto ai 2.5 V minimi della G30. Il partitore 100k/100k legge 1.5–2.1 V, nel range ADC.

## 5. Procedura di messa in servizio

**Primo inserimento (una volta sola):**
0. **Cella sotto 2.0 V a vuoto: scartare, non caricare.** Sotto ~1.5 V il collettore di rame dell'anodo si dissolve; in ricarica si rideposita come dendriti che perforano il separatore. Sintomo in carica: la cella si scalda molto. È la sequenza che precede sfiato o thermal runaway. Tra 2.0 e 2.5 V si tenta solo a ≤ 0.1C con mano sulla cella; una cella che si scalda si toglie subito, si mette su superficie non infiammabile e si smaltisce (RAEE, terminali nastrati). *Lezione del 2026-09-21: la 30Q a 0 V.*
1. Verificare peso e involucro: ~45–48 g, wrap integro, nessuna ammaccatura
2. Misurare la tensione di entrambe. Devono stare **entro 0.05 V**. Se no: inserire da sola la più scarica nella scheda UPS e caricarla finché si allinea, poi inserire la seconda. Collegare due celle a tensione diversa in parallelo produce una corrente di equalizzazione limitata solo dalle due IR: decine di ampere e scintilla sul contatto
3. Saldare il cablaggio di §4 con le celle **fuori** dalla scheda

**Regole operative:**
- Mai drone acceso con USB-C inserita nella scheda: il charger vedrebbe il carico dei motori
- Mai nulla collegato all'uscita boost
- Pacco collegato al BT2.0 **solo durante il test**, scollegato appena finito
- `SW1` a portata di mano come kill
- **Transitorio finché non c'è il fusibile:** si può lavorare con le tre regole sopra più ispezione dei cavi prima di ogni sessione. Accettabile per un banco, non come stato finale

## 6. Test di accettazione

Eliche staccate, switch arm ON solo durante la misura, multimetro su `TP_VBAT`.

| # | Condizione | Misura | Passa se |
|---|---|---|---|
| 1 | a vuoto, armato, motori a 0 | V su `TP_VBAT` | 3.6–4.2 V |
| 2 | `[50,50,50,50]` @ 10 Hz da CLI | caduta rispetto a #1 | < 0.3 V |
| 3 | `[100,100,100,100]` @ 10 Hz da CLI | caduta rispetto a #1 | < 0.3 V, nessun reset |
| 4 | boot successivo a #3 | `/drone_1/log` | **nessun** `boot reset_reason=BROWNOUT` |

Diagnosi se #2/#3 falliscono: misurare con i puntali **sul metallo delle celle** durante il carico. Se lì la tensione regge e su `TP_VBAT` no, il colpevole sono i contatti a molla della scheda (o il cablaggio), non le celle.

## 7. Cosa resta fuori

- **La batteria di volo.** Il pacco pesa ~110 g con scheda; i 4 coreless 8520 spingono 120–160 g totali su un drone da 60–80 g. In volo va la LiPo 1S 300–600 mAh (10–15 g), che va ancora procurata.
- **Il buck-boost** resta come sorgente di emergenza a bassa potenza (≤ 10% PWM), con il workaround 1000 µF + 100 nF se serve.

## 8. Riferimenti

- Sessione che ha diagnosticato il brownout: [`sessions/2026-05-09-test-motori-brownout.md`](../sessions/2026-05-09-test-motori-brownout.md)
- Rail di alimentazione della PCB v1.0: [`pcb-custom/pcb-design-spec.md`](../pcb-custom/pcb-design-spec.md) §3.1
- Procedura di test motori: [`grounding/05-BRINGUP-QUICKSTART.md`](../grounding/05-BRINGUP-QUICKSTART.md) §5
