# Brainstorming: Nuova Architettura Swarm

**Data:** 2026-04-14
**Stato:** Approccio B scelto — design dettagliato in corso

---

## Contesto

Il progetto attuale usa un'architettura centralizzata: ogni drone è un nodo micro-ROS che comunica via UDP/WiFi con un agent sul PC. Questa architettura non supporta comunicazione drone-to-drone ed è dipendente da un'infrastruttura WiFi esterna.

Si esplora una ristrutturazione verso un'architettura swarm con comunicazione peer-to-peer.

---

## Requisiti emersi dalla discussione

### Scenario target
- **Flotta eterogenea** con ruoli specializzati:
  - **Explorer:** mappa l'ambiente
  - **Relay/Link:** si auto-posiziona per compensare buchi di segnale
  - **Rescue:** esegue la missione operativa
  - **Bridge:** fisso vicino al PC, gateway ESP-NOW ↔ UDP
- **4 droni** come proof-of-concept (scala didattica/tesi)
- **Nessuna infrastruttura WiFi esterna** — i droni comunicano tra loro via ESP-NOW, il bridge crea AP locale per il PC

### Modello di controllo
- **PC è il cervello (mission planning):** assegna missioni, costruisce la mappa dall'explorer, calcola posizionamento relay (basato su RSSI + mappa), supervisiona e può overridare
- **Droni sono esecutori (mission execution):** ricevono waypoint, volano autonomamente, gestiscono stabilità e obstacle avoidance localmente
- Il relay è un caso speciale: il PC decide *quando e dove* riposizionarlo automaticamente (senza input dell'ingegnere), basandosi su RSSI debole e mappa degli ostacoli. Il relay riceve il waypoint e ci vola da solo.

### Separazione responsabilità

| | PC (cervello) | Drone (esecutore) |
|---|---|---|
| **Cosa fare** | ✅ Assegna missioni, waypoint, zone | ❌ Non decide la strategia |
| **Come farlo** | ❌ Non gestisce il volo | ✅ Path locale, PID, obstacle avoidance |
| **Mappa** | ✅ Costruisce e mantiene | Manda dati raw (explorer) |
| **Relay positioning** | ✅ Calcola posizione ottimale | Riceve waypoint, ci vola |
| **Monitoring** | ✅ Dashboard, override | Manda telemetria |

### Comunicazione
- **ESP-NOW** come protocollo primario drone-to-drone
  - Latenza ~1-2ms, payload 250 byte, no infrastruttura
  - Banda ampiamente sufficiente (~5 KB/s su ~60-80 KB/s disponibili per 3 droni)
  - Non ha mesh nativo — relay multi-hop va implementato manualmente
- **ESP-WIFI-MESH** come possibile evoluzione futura (mesh automatico, il root node fa da AP)
- **Bridge drone** con doppia interfaccia: ESP-NOW verso i droni, UDP verso il PC

### Decisione su micro-ROS
- **Rimosso dall'architettura.** Motivazioni:
  - Overhead RAM (~50-80KB) significativo sull'ESP32
  - Build molto lente con il componente micro-ROS
  - Doppio stack di comunicazione (ESP-NOW + micro-ROS) inutilmente complesso
  - Il gateway ROS2 lato PC sarebbe un contributo troppo piccolo per giustificare la dipendenza
  - Il valore di "nodo ROS2 nativo" si perde quando la comunicazione primaria è ESP-NOW

### Real-time
- **L'ESP32-S3 è soft real-time, sufficiente per questo drone:**
  - PID a 1kHz su Core 1 dedicato (no WiFi/ESP-NOW che stanno su Core 0)
  - Motori brushed con dinamica lenta
  - Drone ~65g indoor con dinamica non aggressiva
  - Jitter occasionale da flash cache trascurabile
- **Non è un problema** per il progetto

---

## Approcci proposti

### A — Pragmatico Minimale
- Firmware unico, ruolo a compile time
- ESP-NOW broadcast, nessun routing/ack
- PC: script Python CLI (~200 righe)
- **Pro:** veloce (2-3 settimane), semplice
- **Contro:** poco contenuto tesi, nessuna visualizzazione

### B — Modulare con Protocollo Strutturato (RACCOMANDATO)
- Firmware unico con moduli ruolo pluggabili
- Protocollo ESP-NOW strutturato con header, tipi messaggio, ACK per comandi critici
- Bridge in AP mode (crea la sua rete, zero infrastruttura)
- PC: Python dashboard con web UI (Flask + WebSocket)
- Componente `comm_protocol/` separato, riusabile per migrazione a ESP-MESH
- **Pro:** contenuto tesi solido (protocollo + architettura + demo), scalabile
- **Contro:** ~1-2 settimane extra rispetto ad A

### C — Autonomia Massima
- Mission executor on-board con FSM
- Coordinamento distribuito via ESP-NOW
- PC opzionale
- Protocollo con routing layer e service discovery
- **Pro:** massimo valore accademico
- **Contro:** 2-3 mesi extra, rischio di non finire, ESP32 potenzialmente al limite

---

## Analisi tecnica: budget banda ESP-NOW

| Dato per drone | Dimensione | Frequenza | Banda |
|---------------|-----------|-----------|-------|
| IMU (gyro+accel+timestamp) | ~28 byte | 50Hz | 1.4 KB/s |
| Attitude (roll/pitch/yaw) | ~16 byte | 50Hz | 0.8 KB/s |
| Position estimate (x,y,z) | ~16 byte | 20Hz | 0.3 KB/s |
| Battery + status | ~8 byte | 1Hz | ~0 |
| **Totale per drone** | | | **~2.5 KB/s** |

3 droni non-bridge → ~7.5 KB/s totali. ESP-NOW pratico ~60-80 KB/s → **utilizzo ~10-12%**. Ampiamente sufficiente.

---

## Protocolli mesh valutati

| Protocollo | Mesh nativo | Richiede AP | Latenza | Multi-hop | Adatto? |
|-----------|------------|-------------|---------|-----------|---------|
| ESP-NOW | No | No | ~1-2ms | Manuale | ✅ Scelto (semplice, bassa latenza) |
| ESP-WIFI-MESH | Sì | Root fa da AP | ~10-30ms | Automatico | ⏳ Futuro (se serve mesh vero) |
| ESP-MESH-LITE | Sì | Root fa da AP | ~10-30ms | Automatico | ⏳ Futuro |
| BLE Mesh | Sì | No | ~50-100ms | Automatico | ❌ Troppo lento |
| 802.15.4/Thread | Sì | No | ~10ms | Automatico | ❌ ESP32-S3 non ha la radio |

---

## Decisione presa: Approccio B

**Approccio B scelto** con le seguenti precisazioni:
- 4 droni (explorer, relay, rescue, bridge) — non 3
- Bridge è separato dal relay (il bridge deve stare vicino al PC, il relay deve potersi muovere)
- Il relay NON ha autonomia sulla decisione di dove posizionarsi — il PC calcola la posizione ottimale basandosi su RSSI + mappa e manda il waypoint al relay
- La mappa vive sul PC, non sui droni (evita complessità di condivisione mappa via ESP-NOW con frammentazione pacchetti)
- ESP-MESH documentato come "future work" nella tesi

---

## Note per la tesi
- Progetto didattico / tesi magistrale in ingegneria
- Focus: dimostrare i concetti, non production-ready
- Valore nella documentazione delle scelte e dei trade-off
- ESP-MESH come "future work" ben motivato
- La rimozione di micro-ROS va motivata con analisi costi/benefici (RAM, complessità, latenza)
