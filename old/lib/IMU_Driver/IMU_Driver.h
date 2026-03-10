#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

#include <Wire.h>
#include <MPU6050_light.h>
#include "types.h"

class IMU_Driver {
public:
    IMU_Driver(TwoWire &w = Wire);
    bool begin(uint8_t sda, uint8_t scl);
    void update();
    void getData(SensorData &data);
    void resetYaw();     // Azzera solo lo Yaw attuale
    void calibrate();    // Rifà la calibrazione dei giroscopi

private:
    MPU6050 *mpu;
    TwoWire &_wire;
    float _yaw_offset = 0; 
    
    // Manual Integration Variables
    float _my_yaw = 0.0;
    unsigned long _last_time = 0;
};

#endif
