#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- DEBUG CONFIGURATION ---
// Imposta a true per vedere i dati grezzi su Seriale
const bool ENABLE_DEBUG_PRINTS = true;

// --- PINOUT DEFINITIONS (Seeed XIAO ESP32S3) ---
// I2C (IMU - MPU6050)
#define PIN_I2C_SDA D4
#define PIN_I2C_SCL D5

// UART (Optical Flow + ToF - Matek 3901-L0X)
#define PIN_FLOW_TX D6  // TX dell'ESP -> RX del Modulo
#define PIN_FLOW_RX D7  // RX dell'ESP <- TX del Modulo

// --- SENSOR CONFIGURATION ---
#define IMU_UPDATE_RATE_HZ 100
#define FLOW_UART_BAUD 19200 // Clone PMW3901+VL53L1X, protocollo CXOF esteso

#endif
