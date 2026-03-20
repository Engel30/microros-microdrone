# common

Tipi e configurazioni condivisi da tutti i componenti del progetto.

## File

- **`drone_config.h`** — Configurazione centralizzata:
  - Pin GPIO (motori, I2C, UART, ADC, buzzer, LED)
  - Body frame: macro di correzione assi IMU e Flow (X=avanti, Y=destra, Z=giù, convenzione NED)
  - Parametri IMU: indirizzo I2C, sample rate, deadzone gyro
  - Parametri Flow: `FLOW_SCALE_RAD` (rad/count, calibrato empiricamente su clone P3901: 1.294e-2, 7.35x ArduPilot)
  - Soglie batteria, frequenze task, priorità FreeRTOS, profondità code

- **`drone_types.h`** — Struct condivise per comunicazione inter-task via FreeRTOS queues:
  - `imu_data_t` — accelerometro (g) + giroscopio (deg/s) nel body frame + timestamp
  - `flow_data_t` — velocità (m/s) + posizione integrata (m) nel body frame + raw count + range ToF (mm) + qualità + timestamp
  - `state_t` — stato fuso: posizione, velocità, angoli (Fase 2+)
  - `motor_cmd_t` — duty % per 4 motori + timestamp

## Convenzione body frame

Tutti i driver producono dati nello stesso sistema di riferimento:
- **X** = avanti
- **Y** = destra
- **Z** = giù (gravità = +1g)

Le macro di correzione in `drone_config.h` compensano il montaggio fisico dei sensori.

## Dipendenze

`esp_driver_gpio`, `esp_driver_uart` (per le definizioni GPIO_NUM_* e UART_NUM_*)
