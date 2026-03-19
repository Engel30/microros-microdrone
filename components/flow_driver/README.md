# flow_driver

Driver per sensore optical flow PMW3901 + ToF VL53L1X via UART (protocollo CXOF).

## Stato: stub (da implementare in Fase 0A)

## API

- `flow_init()` — Configura UART1 a 19200 baud
- `flow_read(flow_data_t *data)` — Parsa frame CXOF, riempie struct con flow_x/y, range_mm, quality

## Protocollo CXOF

Frame 11 byte: `[FE 04 Xl Xh Yl Yh Dl Dh CK SQ AA]`

- `FE` — header
- `04` — lunghezza dati
- `Xl/Xh` — flow X (int16, little-endian, pixel delta)
- `Yl/Yh` — flow Y (int16, little-endian, pixel delta)
- `Dl/Dh` — distanza ToF (uint16, little-endian, in centimetri → convertire in mm)
- `CK` — checksum
- `SQ` — qualità segnale
- `AA` — footer

Il parser è una state machine con gestione re-sync (0xFE può apparire come valore checksum).

## Riferimento

Implementazione Arduino originale: `old/lib/Optical_Flow/OpticalFlow.cpp`

## Dipendenze

`common`, `driver` (ESP-IDF UART)
