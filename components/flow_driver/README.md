# flow_driver

Driver per sensore optical flow PMW3901 (clone P3901) + ToF VL53L1X via UART (protocollo CXOF).

## Stato: implementato e calibrato (Fase 0A)

- ToF (range_mm) verificato OK (testato fino a 72cm, rileva ostacoli)
- `FLOW_SCALE_RAD = 1.294e-2 rad/count` — calibrato con test di spostamento 10cm su libro a 10-11cm altezza. Errore medio ~8% (range 86-104%). Il valore è 7.35x quello ArduPilot originale perché il clone P3901 ha scaler interno diverso dal PMW3901 genuino
- Raw counts accumulati tra letture (`acc_raw_x/y`) per evitare perdita di frame intermedi quando più frame CXOF arrivano tra due chiamate a `flow_read()`
- Gyro compensation non implementata nel driver (è responsabilità di sensor_fusion o del chiamante)

## API

- `flow_init()` — Configura UART1 a 19200 baud su D6(TX)/D7(RX), buffer RX 256 byte
- `flow_read(flow_data_t *data)` — Non-bloccante. Parsa frame CXOF, converte in velocità (m/s) e posizione integrata (m) nel body frame (X=avanti, Y=destra). Ritorna `ESP_OK` se un frame valido è pronto, `ESP_ERR_NOT_FOUND` altrimenti. Resetta i contatori raw accumulati dopo la lettura

## Output (flow_data_t)

| Campo | Tipo | Unità | Descrizione |
|-------|------|-------|-------------|
| vel_x, vel_y | float | m/s | Velocità nel body frame |
| pos_x, pos_y | float | m | Posizione integrata (debug, drifta nel tempo) |
| raw_x, raw_y | int16 | count | Pixel delta accumulati dal sensore (tra due letture) |
| range_mm | uint16 | mm | Distanza ToF |
| quality | uint8 | — | Qualità segnale superficie |

## Conversione pixel → velocità

```
spostamento [m] = count * FLOW_SCALE_RAD * altitude [m]
velocità [m/s]  = spostamento / dt
```

A 70cm altezza, 1 count ≈ 0.9cm di spostamento. Pochi counts di rumore hanno impatto significativo → la gyro compensation nel consumatore è essenziale.

## Note calibrazione

- Testato su: libro (testo), foglio a quadretti, parquet
- Il clone P3901 produce pochi raw counts per frame (tipicamente 1-8) rispetto al PMW3901 genuino
- A 10-11cm: tracking affidabile, ~40-70 counts totali per 10cm di spostamento
- A 70cm: tracking funziona ma ogni count pesa molto, necessaria compensazione gyro precisa
- Quality tipica: 100-160 (sufficiente per tracking)

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
