// ============================================================
// TEST POSIZIONE - Flow + IMU (v3 - log dettagliati)
//
// Comandi: 'r'=reset  'd'=debug  'c'=calibra IMU  'm'=misura
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "IMU_Driver.h"
#include "OpticalFlow.h"

static const float DEG2RAD = PI / 180.0f;
static const uint8_t MIN_QUALITY = 30;

// FLOW_SCALE: converte counts -> angolo -> mm. Dipende dall'ottica del clone.
// Calibrare con 'm' e aggiornare questo valore nel sorgente.
static float FLOW_SCALE = 0.00282f;

// FOCAL_COUNTS: usato SOLO per la compensazione gyro (angolo_gyro -> counts equivalenti).
// Deve riflettere l'ottica FISICA del sensore, NON il FLOW_SCALE calibrato.
// Valore fisico ArduPilot CXOF: 1/1.76e-3 = 568. NON aggiornare con la calibrazione.
static const float FOCAL_COUNTS = 568.0f;

// Modalita' trace per diagnostica (attivata con 't', disattivata con 't')
// Stampa ogni frame flow con raw counts, gyro contrib, dx calcolato
static bool traceMode = false;

IMU_Driver imu(Wire);
HardwareSerial FlowSerial(1);
OpticalFlow flow(FlowSerial);
SensorData sd;

// Posizione
float raw_x = 0, raw_y = 0;
float fused_x = 0, fused_y = 0;


// Accumulatori gyro
float gyro_x_acc_rad = 0;
float gyro_y_acc_rad = 0;
unsigned long lastImuMicros = 0;

// Statistiche flow per periodo di stampa
int32_t sum_fx = 0, sum_fy = 0;
int flowFrames = 0;
int skippedFrames = 0;

// Calibrazione
bool measuring = false;
float meas_start_x = 0, meas_start_y = 0;

bool debugMode = false;
unsigned long lastPrintTime = 0;

void resetPosition() {
    raw_x = raw_y = fused_x = fused_y = 0;
    imu.resetYaw();
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    Serial.println(F("============================================="));
    Serial.println(F(" TEST POSIZIONE - Flow + IMU (v3)"));
    Serial.println(F("============================================="));
    Serial.println(F("Comandi:"));
    Serial.println(F("  'r' = reset posizione e yaw"));
    Serial.println(F("  'd' = toggle debug dettagliato"));
    Serial.println(F("  't' = toggle trace frame-per-frame (diagnostica)"));
    Serial.println(F("  'c' = ricalibra IMU (tenere fermo!)"));
    Serial.println(F("  'm' = calibra scala (muovi 100mm dritto)"));
    Serial.println(F("=============================================\n"));

    Serial.print(F("Init IMU... "));
    if (imu.begin(PIN_I2C_SDA, PIN_I2C_SCL)) {
        Serial.println(F("OK"));
    } else {
        Serial.println(F("FAIL"));
    }

    flow.begin(FLOW_UART_BAUD, PIN_FLOW_RX, PIN_FLOW_TX);
    Serial.println(F("Flow sensor OK"));
    Serial.printf("Scala: %.5f rad/count (focale: %.1f counts)\n\n", FLOW_SCALE, FOCAL_COUNTS);

    lastImuMicros = micros();
}

void loop() {
    // --- Comandi ---
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'r') {
            resetPosition();
            Serial.println(F("\n>>> RESET <<<\n"));
        } else if (cmd == 'd') {
            debugMode = !debugMode;
            Serial.printf("\n>>> Debug: %s <<<\n\n", debugMode ? "ON" : "OFF");
        } else if (cmd == 't') {
            traceMode = !traceMode;
            Serial.printf("\n>>> Trace frame-per-frame: %s <<<\n", traceMode ? "ON" : "OFF");
            if (traceMode) Serial.println(F("Colonne: fx_raw fy_raw | gyro_contrib_x gyro_contrib_y | fx_comp fy_comp | dx dy | pos_fused\n"));
        } else if (cmd == 'c') {
            imu.calibrate();
            lastImuMicros = micros();
            gyro_x_acc_rad = 0;
            gyro_y_acc_rad = 0;
        } else if (cmd == 'm') {
            if (!measuring) {
                measuring = true;
                meas_start_x = fused_x;
                meas_start_y = fused_y;
                Serial.println(F("\n>>> CALIBRAZIONE: muovi esattamente 100mm dritto, poi premi 'm' <<<\n"));
            } else {
                measuring = false;
                float dx = fused_x - meas_start_x;
                float dy = fused_y - meas_start_y;
                float dist_misurata = sqrtf(dx * dx + dy * dy);
                Serial.printf("\n>>> Misurato: %.1f mm (dX:%.1f dY:%.1f)  atteso: 100 mm <<<\n",
                    dist_misurata, dx, dy);

                if (dist_misurata > 5) {
                    float correzione = 100.0f / dist_misurata;
                    FLOW_SCALE *= correzione;
                    // FOCAL_COUNTS non si aggiorna: e' una costante fisica del sensore
                    Serial.printf(">>> Correzione: x%.2f  nuova scala: %.5f  (focale fisica: %.1f) <<<\n\n",
                        correzione, FLOW_SCALE, FOCAL_COUNTS);
                    resetPosition();
                } else {
                    Serial.println(F(">>> Troppo piccolo, riprova <<<\n"));
                }
            }
        }
    }

    // --- 1. IMU + accumula gyro ---
    imu.update();
    imu.getData(sd);

    unsigned long now = micros();
    float dt = (now - lastImuMicros) / 1e6f;
    lastImuMicros = now;

    if (fabsf(sd.gx) > 0.2f) gyro_x_acc_rad += sd.gx * DEG2RAD * dt;
    if (fabsf(sd.gy) > 0.2f) gyro_y_acc_rad += sd.gy * DEG2RAD * dt;

    // --- 2. Flow ---
    flow.update();

    if (flow.hasNewData()) {
        int16_t fx_raw = flow.getFlowX();
        int16_t fy_raw = flow.getFlowY();
        int32_t dist   = flow.getDistance();
        uint8_t qual   = flow.getQuality();

        // Statistiche per log
        sum_fx += fx_raw;
        sum_fy += fy_raw;
        flowFrames++;

        bool valid = (qual >= MIN_QUALITY) && (dist > 10);

        if (!valid) {
            skippedFrames++;
        } else {
            float h = (float)dist;

            // RAW
            raw_x += (float)fx_raw * FLOW_SCALE * h;
            raw_y += (float)fy_raw * FLOW_SCALE * h;

            // FUSED
            float fx_comp = (float)fx_raw + gyro_y_acc_rad * FOCAL_COUNTS;
            float fy_comp = (float)fy_raw + gyro_x_acc_rad * FOCAL_COUNTS;

            float cos_r = cosf(sd.roll * DEG2RAD);
            float cos_p = cosf(sd.pitch * DEG2RAD);
            float h_corr = h * cos_r * cos_p;

            float dx_body = fx_comp * FLOW_SCALE * h_corr;
            float dy_body = fy_comp * FLOW_SCALE * h_corr;

            float yaw_rad = sd.yaw * DEG2RAD;
            float cos_y = cosf(yaw_rad);
            float sin_y = sinf(yaw_rad);

            fused_x += dx_body * cos_y - dy_body * sin_y;
            fused_y += dx_body * sin_y + dy_body * cos_y;

            // TRACE: stampa ogni frame per diagnostica (attiva con 't')
            if (traceMode) {
                float gyro_cx = gyro_y_acc_rad * FOCAL_COUNTS;  // contributo gyro in counts
                float gyro_cy = gyro_x_acc_rad * FOCAL_COUNTS;
                Serial.printf("T fx=%+4d fy=%+4d | gcx=%+5.1f gcy=%+5.1f | fxc=%+5.1f fyc=%+5.1f | dx=%+6.2f dy=%+6.2f | pos(%+7.1f,%+7.1f)\n",
                    fx_raw, fy_raw, gyro_cx, gyro_cy,
                    (float)fx_raw + gyro_cx, (float)fy_raw + gyro_cy,
                    dx_body, dy_body, fused_x, fused_y);
            }
        }

        gyro_x_acc_rad = 0;
        gyro_y_acc_rad = 0;
    }

    // --- 3. Stampa ---
    if (millis() - lastPrintTime >= 200) {
        lastPrintTime = millis();

        // Riga 1: Posizione
        Serial.printf("POS  fused(%+7.1f,%+7.1f)  raw(%+7.1f,%+7.1f) mm",
            fused_x, fused_y, raw_x, raw_y);
        if (measuring) Serial.print("  [MISURA]");
        Serial.println();

        // Riga 2: Sensori flow
        Serial.printf("FLOW cnt/frame: fx=%+ld fy=%+ld  |  dist=%ld mm  qual=%d  |  frames=%d skip=%d\n",
            flowFrames > 0 ? sum_fx / flowFrames : 0,
            flowFrames > 0 ? sum_fy / flowFrames : 0,
            (long)flow.getDistance(), flow.getQuality(),
            flowFrames, skippedFrames);

        // Riga 3: IMU
        Serial.printf("IMU  roll=%+5.1f  pitch=%+5.1f  yaw=%+5.1f  |  gyro(%+5.1f,%+5.1f,%+5.1f) dps\n",
            sd.roll, sd.pitch, sd.yaw, sd.gx, sd.gy, sd.gz);

        if (debugMode) {
            // Riga 4: Dettagli extra
            Serial.printf("DBG  scala=%.5f  focale_fisica=%.1f  |  accel(%+4.2f,%+4.2f,%+4.2f) g  temp=%.1fC\n",
                FLOW_SCALE, FOCAL_COUNTS, sd.ax, sd.ay, sd.az, sd.temp);
            Serial.printf("DBG  gyro_acc: x=%+.5f y=%+.5f rad  |  h_corr=%.1f mm\n",
                gyro_x_acc_rad, gyro_y_acc_rad,
                (float)flow.getDistance() * cosf(sd.roll * DEG2RAD) * cosf(sd.pitch * DEG2RAD));
        }

        Serial.println(F("---"));

        // Reset statistiche periodo
        sum_fx = 0;
        sum_fy = 0;
        flowFrames = 0;
        skippedFrames = 0;
    }
}
