#include "IMU_Driver.h"

IMU_Driver::IMU_Driver(TwoWire &w) : _wire(w) {
    mpu = new MPU6050(_wire);
}

bool IMU_Driver::begin(uint8_t sda, uint8_t scl) {
    _wire.begin(sda, scl);
    _wire.setTimeOut(10); // 10ms timeout I2C per evitare hang
    byte status = mpu->begin();
    
    if (status != 0) {
        Serial.print(F("IMU FAIL! Error code: "));
        Serial.println(status);
        return false;
    }
    
    // Aumenta il peso del gyro nel filtro complementare (default 0.98)
    // 0.995 = roll/pitch quasi solo da gyro, stabile durante traslazioni rapide
    mpu->setFilterGyroCoef(0.995f);

    Serial.println(F("IMU Found! Calibrating in 3 seconds..."));
    Serial.println(F(">>> KEEP THE BOARD STILL AND FLAT <<<"));
    delay(3000); // Give user time to let go

    Serial.print(F("Calibrating... "));
    mpu->calcOffsets(true, true); // Gyro and Accel offsets
    Serial.println(F("Done."));
    
    // Print offsets for debug
    Serial.print(F("Offsets -> AccX:")); Serial.print(mpu->getAccXoffset());
    Serial.print(F(" AccY:")); Serial.print(mpu->getAccYoffset());
    Serial.print(F(" AccZ:")); Serial.print(mpu->getAccZoffset());
    Serial.print(F(" GyrX:")); Serial.print(mpu->getGyroXoffset());
    Serial.print(F(" GyrY:")); Serial.print(mpu->getGyroYoffset());
    Serial.print(F(" GyrZ:")); Serial.println(mpu->getGyroZoffset());

    _last_time = micros(); // Start timer
    return true;
}

void IMU_Driver::update() {
    mpu->update();
    
    // --- MANUAL YAW INTEGRATION ---
    unsigned long now = micros();
    float dt = (now - _last_time) / 1000000.0; // Convert micros to seconds
    _last_time = now;
    
    // Get raw gyro Z (degrees per second)
    float gz = mpu->getGyroZ();
    
    // Deadzone (Optional): If rotation is VERY small (<0.2 dps), ignore to stop drift
    if (abs(gz) > 0.2) {
        _my_yaw += gz * dt;
    }
}

void IMU_Driver::getData(SensorData &data) {
    // Fill our global structure
    data.ax = mpu->getAccX();
    data.ay = mpu->getAccY();
    data.az = mpu->getAccZ();
    
    data.gx = mpu->getGyroX();
    data.gy = mpu->getGyroY();
    data.gz = mpu->getGyroZ();
    
    data.roll = mpu->getAngleX();
    data.pitch = mpu->getAngleY();
    data.yaw = _my_yaw; // Use our manually integrated yaw!
    
    data.temp = mpu->getTemp();
}

void IMU_Driver::resetYaw() {
    _my_yaw = 0.0; // Hard reset to zero
    _yaw_offset = 0.0; // Clear offset logic (not used anymore)
}

void IMU_Driver::calibrate() {
    Serial.println(F(">>> TIENI FERMO E IN PIANO! Calibrazione in 2s... <<<"));
    delay(2000);
    Serial.print(F("Calibrazione... "));
    mpu->calcOffsets(true, true);
    _my_yaw = 0.0;
    _yaw_offset = 0;
    _last_time = micros(); // Reset timer per evitare dt enorme
    Serial.println(F("OK!"));
}

