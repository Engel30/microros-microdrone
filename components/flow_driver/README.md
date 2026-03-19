# flow_driver

Driver per sensore optical flow PMW3901 (clone P3901) + ToF VL53L1X via UART (protocollo CXOF).

## Stato: implementato (Fase 0A) — scala da calibrare

ToF (range_mm) verificato OK. Conversione pixel→velocità implementata con costante ArduPilot CXOF (`FLOW_SCALE_RAD = 1.76e-3 rad/count`). Da validare con test di spostamento noto (10cm). Se impreciso, aggiustare `FLOW_SCALE_RAD` in `drone_config.h`.

## API

- `flow_init()` — Configura UART1 a 19200 baud su D6(TX)/D7(RX), buffer RX 256 byte
- `flow_read(flow_data_t *data)` — Non-bloccante. Parsa frame CXOF, converte in velocità (m/s) e posizione integrata (m) nel body frame (X=avanti, Y=destra). Ritorna `ESP_OK` se un frame valido è pronto, `ESP_ERR_NOT_FOUND` altrimenti

## Output (flow_data_t)

| Campo | Tipo | Unità | Descrizione |
|-------|------|-------|-------------|
| vel_x, vel_y | float | m/s | Velocità nel body frame |
| pos_x, pos_y | float | m | Posizione integrata (debug, drifta nel tempo) |
| raw_x, raw_y | int16 | count | Pixel delta raw dal sensore |
| range_mm | uint16 | mm | Distanza ToF |
| quality | uint8 | — | Qualità segnale superficie |

## Conversione pixel → velocità

```
spostamento [m] = count * FLOW_SCALE_RAD * altitude [m]
velocità [m/s]  = spostamento / dt
```

Il PMW3901 ha uno scaler interno (~8-10x): 1 count != 1 pixel fisico. Il fattore `FLOW_SCALE_RAD` (rad/count) è un valore empirico. Fonte: ArduPilot `AP_OpticalFlow_CXOF.cpp`.

## Protocollo CXOF

Frame 11 byte: `[FE 04 Xl Xh Yl Yh Dl Dh CK SQ AA]`

| Byte | Contenuto | Tipo |
|------|-----------|------|
| 0 | 0xFE | Header |
| 1 | 0x04 | Protocol ID |
| 2-3 | Flow X | int16 little-endian (count) |
| 4-5 | Flow Y | int16 little-endian (count) |
| 6-7 | Distanza | uint16 little-endian (cm → convertiti in mm) |
| 8 | Checksum | Somma byte 2-7 |
| 9 | Quality | Qualità segnale superficie |
| 10 | 0xAA | Footer |

## Parser (state machine)

```
Stato 0: cerca header 0xFE
Stato 1: verifica protocol ID 0x04
         → se byte != 0x04 ma == 0xFE: re-sync (torna a stato 1)
Stato 2-10: accumula byte
         → a 11 byte: verifica footer + checksum → process_frame()
```

## Dipendenze

- `common` — tipi e config
- `esp_driver_uart` — UART ESP-IDF
- `esp_timer` — timestamp
