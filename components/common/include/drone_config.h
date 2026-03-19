#pragma once

// ============================================================================
// Drone Identity
// ============================================================================
#define DRONE_ID            1

// ============================================================================
// Pin Definitions (Seeed XIAO ESP32-S3)
// ============================================================================

// Motors (PWM via LEDC)
#define PIN_MOTOR_FL        GPIO_NUM_1   // D0 - Front-Left
#define PIN_MOTOR_RL        GPIO_NUM_2   // D1 - Rear-Left
#define PIN_MOTOR_RR        GPIO_NUM_3   // D2 - Rear-Right
#define PIN_MOTOR_FR        GPIO_NUM_4   // D3 - Front-Right

// IMU (I2C)
#define PIN_I2C_SDA         GPIO_NUM_5   // D4
#define PIN_I2C_SCL         GPIO_NUM_6   // D5
#define I2C_FREQ_HZ         400000       // 400kHz fast mode

// Optical Flow + ToF (UART)
#define PIN_FLOW_TX         GPIO_NUM_43  // D6
#define PIN_FLOW_RX         GPIO_NUM_44  // D7
#define FLOW_UART_NUM       UART_NUM_1
#define FLOW_BAUD_RATE      19200

// Battery
#define PIN_BATTERY_ADC     GPIO_NUM_7   // D8
#define PIN_BUZZER          GPIO_NUM_8   // D9
#define PIN_LED_STATUS      GPIO_NUM_9   // D10

// ============================================================================
// IMU Configuration (MPU6050)
// ============================================================================
#define MPU6050_ADDR        0x68
#define IMU_SAMPLE_RATE_HZ  1000
#define GYRO_DEADZONE_DPS   0.2f         // Deadzone yaw integration

// ============================================================================
// Battery
// ============================================================================
#define BATTERY_LOW_VOLTAGE     3.3f     // Allarme batteria (V)
#define BATTERY_CRITICAL_VOLTAGE 3.0f    // Atterraggio forzato (V)
#define BATTERY_DIVIDER_RATIO   2.0f     // Partitore 100k/100k

// ============================================================================
// Task Frequencies
// ============================================================================
#define FREQ_IMU_HZ         1000
#define FREQ_FLOW_HZ        20
#define FREQ_MICROROS_HZ    50
#define FREQ_BATTERY_HZ     1
#define FREQ_MOTORS_HZ      1000

// ============================================================================
// FreeRTOS Task Priorities (higher = more important)
// ============================================================================
#define PRIO_TASK_MOTORS        6
#define PRIO_TASK_PID_ATTITUDE  6
#define PRIO_TASK_IMU           5
#define PRIO_TASK_FLOW          4
#define PRIO_TASK_MICROROS      2
#define PRIO_TASK_BATTERY       1

// ============================================================================
// FreeRTOS Queue Depths
// ============================================================================
#define QUEUE_DEPTH_IMU     5
#define QUEUE_DEPTH_FLOW    3
#define QUEUE_DEPTH_STATE   3
#define QUEUE_DEPTH_CMD     1
#define QUEUE_DEPTH_MOTOR   1
