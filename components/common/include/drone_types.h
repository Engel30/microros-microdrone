#pragma once

#include <stdint.h>

// Dati IMU raw dal MPU6050
typedef struct {
    float accel_x, accel_y, accel_z;   // g
    float gyro_x, gyro_y, gyro_z;      // deg/s
    int64_t timestamp_us;
} imu_data_t;

// Dati dal sensore optical flow (protocollo CXOF)
typedef struct {
    int16_t flow_x, flow_y;            // pixel delta
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
