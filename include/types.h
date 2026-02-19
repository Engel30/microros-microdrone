#ifndef TYPES_H
#define TYPES_H

struct SensorData {
    // IMU Data (MPU6050)
    float ax, ay, az;       // Accelerometro (g)
    float gx, gy, gz;       // Giroscopio (deg/s)
    float pitch, roll, yaw; // Angoli stimati (deg)
    float temp;             // Temperatura chip

    // Optical Flow Data (Matek 3901-L0X)
    int16_t flow_x_raw;     // Spostamento pixel X (raw)
    int16_t flow_y_raw;     // Spostamento pixel Y (raw)
    uint8_t flow_quality;   // Qualità lettura ottica (0-255)

    // Time of Flight (ToF - VL53L0X integrato)
    int32_t range_mm;       // Distanza dal suolo (mm)
};

#endif
