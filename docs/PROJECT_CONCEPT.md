# Micro-Sciame di Droni ESP32: Studio di Fattibilità

**Data:** 28 Gennaio 2026
**Obiettivo:** Creare una piattaforma drone low-cost (<30€/unità) per sperimentare con algoritmi di sciame (Swarm Intelligence), Mesh Networking (ESP-NOW) e controllo distribuito.

---

## 1. Architettura Hardware (Sensor Fusion Didattico)
Focus su basso costo e comprensione dei sistemi di controllo.

| Sottosistema | Componente Scelto | Funzione | Note |
| :--- | :--- | :--- | :--- |
| **Brain / FC** | **XIAO ESP32-S3** | Flight Control + WiFi | Dual Core 240MHz. Pinout ottimizzato. |
| **IMU** | **MPU-6050** | Assetto (Gyro/Accel) | Bus I2C. Fondamentale per Angle/Rate mode. |
| **Percezione** | **Modulo Flow + Lidar** | Position Hold (X/Y/Z) | **Bus UART.** Modulo integrato (Matek 3901-L0X clone) per risparmiare PIN. |
| **Power** | **4x MOSFET SI2302** | Driver Motori | Low-Side switch. |
| **Monitoring** | **Partitore + Buzzer** | V-Sense + Allarmi | Lettura tensione LiPo (ADC) e feedback sonoro. |

---

## 2. Propulsione e Telaio (Strategia Ibrida)

### Telaio (3D Print)
*   **Materiale:** PLA (FDM).
*   **Motivi 8520:** Press-fit (foro ~8.3-8.4mm). Frame commerciali 75mm Whoop non compatibili con motori coreless 8520.
*   **Perfboard:** 7x5cm montata sopra il frame. Componenti su pin header femmina (rimovibili e riutilizzabili).
*   **Ottico:** Modulo flow montato sotto al centro con visuale libera verso il basso.

### Motori
*   **Tipo:** Coreless Brushed **8520** (8.5mm x 20mm).
*   **Eliche:** 40mm (standard per frame 75mm).
*   **Spinta:** ~40g/motore -> Totale 160g.
*   **Peso Target Drone:** < 60g.

---

## 3. Strategia di Costruzione Elettronica
Due opzioni per il Flight Controller:
1.  **Custom PCB (Consigliata):** Disegno KiCad su misura che si avvita al frame 75mm.
2.  **Perfboard/Millefori:** Tagliata a croce, montaggio componenti discreti. Sconsigliata per vibrazioni ma fattibile.

---

## 4. Alimentazione e "Il Muro dei 7 Minuti"
I micro-droni brushed hanno un'autonomia fisica limitata (7-10 min) dovuta all'efficienza dei motori.

### Soluzione per lo Sviluppo: Tethering
Per sviluppare algoritmi senza cambiare batterie ogni 5 minuti:
*   **Setup:** Alimentatore da banco (4.0V / 3A) + Cavo lungo e sottile.
*   **Vantaggio:** Tempo di volo infinito per tuning PID e test di sciame in area ristretta.

---

## 5. Riferimenti Software
*   **ESP-Drone (Espressif):** Firmware ufficiale ottimizzato. Supporta PMW3901 e controllo PID stabile.
    *   Repo: `espressif/esp-drone`
*   **Crazyflie (Bitcraze):** Gold standard (STM32). Utile per studiare gli schemi elettrici e la dinamica di volo.
*   **micro-ROS per ESP-IDF:** Componente ufficiale per integrare micro-ROS su ESP32.
    *   Repo: `micro-ROS/micro_ros_espidf_component`

## 6. Decisioni Architetturali (Marzo 2026)
*   **Framework:** Migrazione da Arduino/PlatformIO a **ESP-IDF + FreeRTOS + micro-ROS**.
*   **Comunicazione:** micro-ROS (Micro XRCE-DDS) su UDP WiFi verso agent sul PC.
*   **Visualizzazione:** Foxglove Studio (connessione diretta ai topic ROS2).
*   **Scalabilita sciame:** Namespace ROS2 `/drone_N/`, stesso firmware con ID diverso.
*   **Spec completa:** `docs/specs/2026-03-10-swarm-drone-architecture-design.md`

## 7. Next Steps (Action Plan)
1.  Creare progetto ESP-IDF e struttura componenti.
2.  Portare driver IMU e Flow da Arduino a ESP-IDF.
3.  Integrare micro-ROS, validare topic su Foxglove.
4.  Testare motori via PWM (topic cmd_motor_test).
5.  Implementare PID attitudine e hover stabile.
6.  Scalare a sciame con namespace multipli.
