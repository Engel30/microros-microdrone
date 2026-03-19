# uros_interface

Bridge tra micro-ROS (ROS2) e le FreeRTOS queues interne del drone.

## Stato: stub (da implementare in Fase 0A)

## API

- `uros_init()` — Configura trasporto UDP WiFi, crea nodo ROS2, publisher e subscriber
- `uros_spin()` — Esegue spin dell'executor micro-ROS (chiamato a 50Hz)

## Topic pubblicati (Fase 0A)

| Topic | Tipo messaggio | Freq |
|-------|---------------|------|
| `/drone_N/imu/raw` | `sensor_msgs/msg/Imu` | 100Hz |
| `/drone_N/flow` | `std_msgs/msg/Int16MultiArray` | 20Hz |
| `/drone_N/range` | `sensor_msgs/msg/Range` | 20Hz |
| `/drone_N/battery` | `std_msgs/msg/Float32` | 1Hz |

## Topic sottoscritti (fasi successive)

| Topic | Tipo messaggio | Fase |
|-------|---------------|------|
| `/drone_N/cmd_motor_test` | `std_msgs/msg/Float32MultiArray` | 0B |
| `/drone_N/cmd_attitude` | `geometry_msgs/msg/Quaternion` | 1 |
| `/drone_N/cmd_position` | `geometry_msgs/msg/PoseStamped` | 3 |

## Configurazione

- Trasporto: UDP su WiFi (LAN)
- Agent: micro-ROS agent sul PC (Docker o nativo)
- Namespace: `/drone_N/` dove N = DRONE_ID da `drone_config.h`
- QoS: BEST_EFFORT per telemetria, RELIABLE per comandi

## Dipendenze

`common`, `micro_ros_espidf_component` (managed component)
