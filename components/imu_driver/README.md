# imu_driver

Driver per MPU6050 (IMU 6-DoF) via I2C ESP-IDF.

## Stato: stub (da implementare in Fase 0A)

## API

- `imu_init()` — Configura I2C a 400kHz, inizializza registri MPU6050 (DLPF, sample rate, range)
- `imu_read(imu_data_t *data)` — Legge 14 byte dai registri 0x3B-0x48, converte in g e deg/s
- `imu_calibrate(int num_samples)` — Calcola offset medio a drone fermo

## Configurazione MPU6050

| Registro | Valore | Effetto |
|----------|--------|---------|
| SMPLRT_DIV (0x19) | 0x00 | Sample rate 1kHz |
| CONFIG (0x1A) | 0x03 | DLPF ~42Hz |
| GYRO_CONFIG (0x1B) | 0x08 | ±500 deg/s |
| ACCEL_CONFIG (0x1C) | 0x08 | ±4g |
| PWR_MGMT_1 (0x6B) | 0x01 | Clock da gyro X |

## Dipendenze

`common`, `driver` (ESP-IDF I2C)
