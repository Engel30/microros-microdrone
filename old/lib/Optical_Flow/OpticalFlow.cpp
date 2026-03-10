#include "OpticalFlow.h"

OpticalFlow::OpticalFlow(HardwareSerial &serial) : _serial(serial) {
    _idx = 0;
    _distance_mm = -1;
    _flow_x = 0;
    _flow_y = 0;
    _quality = 0;
    _newData = false;
}

void OpticalFlow::begin(uint32_t baud, int8_t rxPin, int8_t txPin) {
    _serial.begin(baud, SERIAL_8N1, rxPin, txPin);
}

void OpticalFlow::update() {
    while (_serial.available()) {
        uint8_t c = _serial.read();

        if (_idx == 0) {
            // Aspetta header 0xFE
            if (c == CXOF_HEADER) _buf[_idx++] = c;
        } else if (_idx == 1) {
            // Verifica protocol ID 0x04
            if (c == CXOF_PROTO_ID) {
                _buf[_idx++] = c;
            } else {
                // Potrebbe essere un nuovo header (0xFE come dato checksum)
                _idx = (c == CXOF_HEADER) ? 1 : 0;
                if (c == CXOF_HEADER) _buf[0] = c;
            }
        } else {
            _buf[_idx++] = c;

            if (_idx == CXOF_FRAME_SIZE) {
                // Verifica footer
                if (_buf[10] == CXOF_FOOTER) {
                    // Verifica checksum (somma byte 2-7)
                    uint8_t ck = 0;
                    for (int i = 2; i <= 7; i++) ck += _buf[i];
                    if (ck == _buf[8]) {
                        processFrame();
                    }
                }
                _idx = 0;
            }
        }
    }
}

void OpticalFlow::processFrame() {
    // Byte 2-3: flow X (int16 little-endian)
    _flow_x = (int16_t)(_buf[2] | (_buf[3] << 8));
    // Byte 4-5: flow Y (int16 little-endian)
    _flow_y = (int16_t)(_buf[4] | (_buf[5] << 8));
    // Byte 6-7: distanza in cm (uint16 little-endian), convertiamo in mm
    _distance_mm = (int32_t)(_buf[6] | (_buf[7] << 8)) * 10;
    // Byte 9: qualita' superficie
    _quality = _buf[9];
    _newData = true;
}

bool OpticalFlow::hasNewData() {
    if (_newData) { _newData = false; return true; }
    return false;
}

int32_t OpticalFlow::getDistance() { return _distance_mm; }
int16_t OpticalFlow::getFlowX() { return _flow_x; }
int16_t OpticalFlow::getFlowY() { return _flow_y; }
uint8_t OpticalFlow::getQuality() { return _quality; }
