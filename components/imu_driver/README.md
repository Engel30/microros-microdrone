# imu_driver

Driver per MPU6050 (IMU 6-DoF) via I2C ESP-IDF. Usa la libreria `esp-idf-lib/mpu6050` (v2.1.8+).

## Stato: implementato e testato (Fase 0A)

## API

- `imu_init()` — Inizializza sottosistema i2cdev, configura MPU6050 (clock PLL, range, DLPF, sample rate)
- `imu_read(imu_data_t *data)` — Legge accelerometro (g) e giroscopio (deg/s) nel body frame (X=avanti, Y=destra, Z=giù). Applica offset calibrazione
- `imu_calibrate(int num_samples)` — Calcola offset medio a drone fermo nel chip frame (500 campioni consigliati). Compensa gravità su asse Z chip

## Body frame

Output allineato alla convenzione NED: X=avanti, Y=destra, Z=giù.

Il chip MPU6050 è montato con destra=+X_chip, avanti=+Y_chip. La rotazione è applicata in `imu_read()` tramite macro in `drone_config.h`:
- body_X (avanti) = chip_Y
- body_Y (destra) = chip_X
- body_Z (giù) = -chip_Z

## Calibrazione

Internamente usa `imu_read_raw_chip()` per leggere nel chip frame senza offset e senza rotazione. Calcola offset nel chip frame. `imu_read()` sottrae offset nel chip frame e poi applica la rotazione body. I due sistemi non si mischiano.

A riposo (drone in piano): accel ≈ [0, 0, +1] g nel body frame (gravità verso giù = +Z).

## Sequenza di inizializzazione

```
i2cdev_init()                            ← sottosistema I2C (mutex, bus)
mpu6050_init_desc(addr=0x68, SDA=D4, SCL=D5)
mpu6050_init()                           ← reset chip, sveglia
mpu6050_set_clock_source(PLL_X)          ← clock stabile
mpu6050_set_full_scale_accel_range(±4g)
mpu6050_set_full_scale_gyro_range(±500°/s)
mpu6050_set_dlpf_mode(DLPF_3 = 42Hz)
mpu6050_set_rate(0 → 1kHz)
```

## Note hardware

- Il modulo GY-521 ha pull-up I2C da 4.7kΩ integrati
- Il warning "check pull-up resistances" del driver ESP-IDF è normale

## Dipendenze

- `common` — tipi e config
- `driver` — ESP-IDF driver framework
- `mpu6050`, `i2cdev`, `esp_idf_lib_helpers` — libreria esp-idf-lib (managed component)
- `esp_timer` — timestamp
