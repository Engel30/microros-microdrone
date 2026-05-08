#pragma once

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

// Dati IMU raw dal MPU6050
typedef struct {
    float accel_x, accel_y, accel_z;   // g
    float gyro_x, gyro_y, gyro_z;      // deg/s
    int64_t timestamp_us;
} imu_data_t;

// Dati dal sensore optical flow (protocollo CXOF)
// Body frame: X=avanti, Y=destra
typedef struct {
    float vel_x, vel_y;                // m/s nel body frame
    float pos_x, pos_y;               // m, posizione integrata (debug)
    int16_t raw_x, raw_y;             // pixel delta raw (debug)
    uint16_t range_mm;                 // distanza ToF in mm
    uint8_t quality;                   // qualita segnale (SQ)
    int64_t timestamp_us;
} flow_data_t;

// Stato fuso (Fase 2+)
typedef struct {
    float pos_x, pos_y, pos_z;         // m
    float vel_x, vel_y, vel_z;         // m/s
    float roll, pitch, yaw;            // rad
    int64_t timestamp_us;
} state_t;

// Comando motori (duty % per ogni motore)
typedef struct {
    float motor[4];                    // 0.0 - 100.0 %
    int64_t timestamp_us;
} motor_cmd_t;

// ============================================================================
// Stato di arm software (sticky). Default DISARMED al boot.
// Scrittore: callback subscriber /drone_1/arm (Core 0).
// Lettori: task_motors (Core 1, 1kHz). Atomic per accesso cross-core safe.
// Vedi components/uros_interface/README.md e main/task_motors.c.
// ============================================================================
extern atomic_bool g_armed;
