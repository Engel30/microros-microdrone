#pragma once

#include "esp_err.h"
#include "drone_types.h"

/**
 * Inizializza I2C e MPU6050 con configurazione da drone_config.h:
 * - I2C a 400kHz su PIN_I2C_SDA/SCL
 * - DLPF ~42Hz, sample rate 1kHz, gyro ±500°/s, accel ±4g
 */
esp_err_t imu_init(void);

/**
 * Legge accelerometro e giroscopio dal MPU6050.
 * Riempie la struct con valori in g (accel) e deg/s (gyro) + timestamp.
 */
esp_err_t imu_read(imu_data_t *data);

/**
 * Calibrazione: legge num_samples a drone fermo e calcola offset medio.
 * Sottrae gli offset da tutte le letture successive.
 */
esp_err_t imu_calibrate(int num_samples);
