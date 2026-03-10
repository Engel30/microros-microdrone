#ifndef OPTICAL_FLOW_H
#define OPTICAL_FLOW_H

#include <Arduino.h>
#include "types.h"

// Protocollo CXOF esteso (clone PMW3901 + VL53L1X)
// Frame: FE 04 Xl Xh Yl Yh Dl Dh CK SQ AA (11 byte)
#define CXOF_HEADER     0xFE
#define CXOF_PROTO_ID   0x04
#define CXOF_FOOTER     0xAA
#define CXOF_FRAME_SIZE 11

class OpticalFlow {
public:
    OpticalFlow(HardwareSerial &serial);
    void begin(uint32_t baud, int8_t rxPin, int8_t txPin);
    void update();

    int32_t getDistance();  // mm (VL53L1X)
    int16_t getFlowX();
    int16_t getFlowY();
    uint8_t getQuality();
    bool hasNewData();     // true se c'e' un frame nuovo (si resetta dopo la lettura)

private:
    HardwareSerial &_serial;

    uint8_t _buf[CXOF_FRAME_SIZE];
    uint8_t _idx;

    int32_t _distance_mm;
    int16_t _flow_x;
    int16_t _flow_y;
    uint8_t _quality;
    bool _newData;

    void processFrame();
};

#endif
