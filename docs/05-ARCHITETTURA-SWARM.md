# Architettura Swarm: ESP-NOW Distribuito

**Status:** Design (Approccio B scelto)

---

## 1. Visione

Passaggio da architettura **centralizzata** (micro-ROS UDP → PC) a **distribuita peer-to-peer** (ESP-NOW drone-to-drone).

**Vantaggio:** No infrastruttura WiFi esterna, latenza 1-2ms vs UDP 50-100ms, autonomia distribuita.

---

## 2. Architettura Scelta: Approccio B (Modulare + Protocollo Strutturato)

**Firmware unico** con ruoli pluggabili (compile-time o runtime):
- Stesso codice su tutti i droni
- DRONE_ROLE + DRONE_ID configurabili

**Protocollo ESP-NOW strutturato:**
- Header fisso (magic, version, type, src_id, dst_id, seq, flags, len)
- Tipi messaggio: CMD_ATTITUDE, CMD_GOTO, TELEMETRY_*, ACK
- ACK esplicito per comandi critici
- Retry automatico

**Bridge drone (gateway):**
- WiFi AP locale (crea rete "MicroDrone-Swarm")
- UDP bridge verso PC (porta 8888)
- Doppia interfaccia: ESP-NOW ↔ WiFi

**PC:** Python dashboard Flask + WebSocket (non più micro-ROS)

---

## 3. 4 Ruoli Droni

| Ruolo | Funzione | Autonomia | Note |
|:------|:---------|:----------|:-----|
| **Explorer** | Mappa autonoma area | Bassa (mappatura) | Trasmette dati raw al PC |
| **Relay** | Estende coverage | Media | PC calcola waypoint, relay ci vola |
| **Rescue** | Missione operativa | Alta | Riceve waypoint, executa autonomo |
| **Bridge** | WiFi AP + gateway | Fisso (tethered) | Crea rete, converte UDP ↔ ESP-NOW |

---

## 4. Flusso Dati

**Comando PC → Droni:**
1. PC invia UDP a bridge (indirizzo 192.168.4.1:8888)
2. Bridge forward via ESP-NOW al drone target
3. Drone riceve, estrae payload, processa comando
4. (Opzionale) Manda ACK indietro

**Telemetria Droni → PC:**
1. Drone pubblica heartbeat + IMU + stato via ESP-NOW
2. Bridge riceve, converte UDP
3. PC riceve, visualizza dashboard
4. Salva CSV log

---

## 5. Protocollo ESP-NOW

### Header Strutturato

```
struct esp_now_header {
  uint8_t magic;       // 0xAE (magic byte)
  uint8_t version;     // Protocol version
  uint8_t msg_type;    // Tipo messaggio (enum)
  uint8_t src_id;      // Source drone ID
  uint8_t dst_id;      // Dest drone ID (0xFF = broadcast)
  uint8_t seq;         // Sequence number (per ACK)
  uint8_t flags;       // bit0=ACK_REQ, bit1=ACK, bit2=RETRY
  uint8_t payload_len; // Lunghezza payload
}; // 8 byte
```

### Tipi Messaggio

| Type | Dir | Payload | ACK? | Rate |
|:-----|:---:|:-------:|:----:|:----:|
| **CMD_ATTITUDE** | PC→Drone | roll, pitch, yaw | ✅ | 50Hz |
| **CMD_GOTO** | PC→Drone | x, y, z waypoint | ✅ | 1Hz |
| **TELEMETRY_IMU** | Drone→PC | ax,ay,az,gx,gy,gz | ❌ | 100Hz |
| **TELEMETRY_STATE** | Drone→PC | roll,pitch,yaw,vx,vy,vz,bat | ❌ | 50Hz |
| **HEARTBEAT** | Drone→PC | status, battery | ❌ | 1Hz |
| **ACK** | Any→Any | seq, status | - | on-demand |

---

## 6. Link Management

**Link Loss Failsafe:**
- Se drone non riceve heartbeat PC > 2s: land autonomo
- Ramp motor down, level attitude

**RSSI Thresholding:**
- Se signal strength < -80dBm: segnala PC per reposition relay
- PC calcola nuova posizione relay basata su mappa + RSSI

**Banda Usage:**
- 3 droni non-bridge: ~7.5 KB/s totale
- ESP-NOW disponibile: ~60-80 KB/s
- **Utilizzo: ~10%** → ampio headroom

---

## 7. Transizione da micro-ROS a ESP-NOW

**Roadmap versioni:**
- **v2 (Fase 0-1):** micro-ROS UDP (attuale)
- **v2.5 (Fase 2):** micro-ROS + inizio ESP-NOW
- **v3 (Fase 3+):** Pure ESP-NOW (rimozione micro-ROS)

**Benefici rimozione micro-ROS:**
- RAM libera: +50-80KB
- Build time: -5 minuti
- Latenza: 1-2ms vs 50-100ms

---

## 8. Demo Scenario

**Setup aula:**
- Laptop (PC) con Python dashboard
- Bridge drone (tethered, crea AP)
- 3 droni in volo (Explorer, Relay, Rescue)
- Area 10×10m indoor

**Validazione:**
- ✅ 4 droni stabili simultaneo
- ✅ Comunicazione P2P ESP-NOW
- ✅ PC vede telemetria real-time
- ✅ Explorer mappa autonoma
- ✅ Relay si riposiziona
- ✅ Rescue completa waypoint sequenza
- ✅ Failsafe: link loss → land autonomo

---

## 9. Protocolli Mesh Alternativi

| Protocollo | Mesh Nativo | Latenza | Multi-hop | Status |
|:-----------|:--------:|:-------:|:---------:|:------:|
| **ESP-NOW** | ❌ | ~1-2ms | ⚠️ Manual | ✅ **Scelto** |
| **ESP-WIFI-MESH** | ✅ | ~10-30ms | ✅ Auto | 📅 Futuro |
| **ESP-MESH-LITE** | ✅ | ~10-30ms | ✅ Auto | 📅 Futuro |
| **BLE Mesh** | ✅ | ~50-100ms | ✅ Auto | ❌ Troppo lento |

---

## 10. Mapping: Explorer → Global Map PC

**Explorer vola autonomo, acquisisce:**
- Optical flow (vx, vy)
- ToF altezza
- IMU assetto

**PC integra dati:**
- Kalman filter su posizione X/Y da flow
- Costruisce occupancy grid
- Rileva ostacoli (futura estensione)

**Relay positioning:** PC usa mappa + RSSI per calcolare waypoint relay ottimale

---

## 11. Prossimi Step

1. Definire protocollo ESP-NOW binario (encoding)
2. Implementare tx/rx ESP-NOW firmware
3. Bridge drone con WiFi AP
4. Python agent PC (UDP gateway)
5. Ruoli drone: explorer, relay, rescue
6. Testing 2-4 droni
7. Demo recording per tesi
