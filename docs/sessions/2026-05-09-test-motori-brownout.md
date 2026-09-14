# 2026-05-09 — Test motori: brownout buck-boost + console di debug Foxglove

**Branch:** `feature/microros-tethered` · **Commit:** `5296be5`

## Fatto

**Test motori** su PCB v1.0, alimentazione bench 5V 10A → buck-boost AliExpress → 4.1V.

**Console di debug Foxglove `/drone_1/log`** (`rcl_interfaces/Log`):
- Publisher BEST_EFFORT `/drone_1/log` (8/12 slot occupati). Il Log panel di Foxglove lo riconosce nativamente: filtri per livello (DEBUG/INFO/WARN/ERROR/FATAL), name, timeline scrubbabile.
- API `uros_log(level, fmt, ...)` printf-style, non-blocking, drop-on-full. Queue interna 16 item × 192 byte (~3KB RAM). Sicura prima di `uros_init` (no-op). Non chiamabile da ISR.
- Drain in `task_microros` cap 5 msg/ciclo (10ms) → 500/s effettivi. Buffer messaggio pre-allocato, nessuna malloc per publish.
- Hook: reset reason al boot (POWERON/BROWNOUT/PANIC/WDT/EXT/SW), WiFi connect/disconnect+reason, transizioni arm/disarm, edge watchdog motori (running↔expired in stato armed), `cmd_motor_test` malformato.
- `app-colcon.meta`: `RMW_UXRCE_MAX_PUBLISHERS` 8→12 per non saturare.
- Build pulita, 14% flash free.

**File toccati:**
- `app-colcon.meta` — pub limit 12 (richiede rebuild di `libmicroros.a` se si cambia di nuovo)
- `components/uros_interface/include/uros_interface.h` — API `uros_log()`, enum `uros_log_level_t`
- `components/uros_interface/uros_interface.c` — queue + publisher + helper + drain + hook WiFi/arm/cmd
- `main/task_motors.c` — edge detection watchdog + log
- `components/uros_interface/README.md` — tabella topic + sezione "Console di debug"

## Scoperto

**4 motori @ 10% PWM: gira pulito. 2 motori @ 20% PWM: il drone si resetta.**

Diagnosi: il buck-boost ha transient response insufficiente (cap di output piccolo, induttore lento). Al transitorio PWM dei coreless 8520 il rail dell'ESP32 collassa sotto la soglia di brownout.

Soluzione fisica raccomandata: alimentare da **LiPo 1S 300-600 mAh 25C**, che eroga 7-15A di picco senza fiatare. Workaround temporaneo restando sul buck: cap elettrolitico 1000 µF + 100 nF ceramico sull'uscita, vicino ai source dei MOSFET.

**Secondo ostacolo:** pubblicare `cmd_motor_test` da Foxglove richiede rate ≥ 5 Hz per non far scattare il watchdog motori da 500 ms. Il pannello Publish di Foxglove non ha un rate nativo → workaround `ros2 topic pub -r 10`.

## Verificato

- `/drone_1/log` visibile nel Log panel: boot reset reason, eventi WiFi, transizioni arm, edge watchdog motori funzionano tutti come atteso.
- Motori girano correttamente al duty pubblicato finché si resta sotto la soglia di brownout del buck-boost.

## Prossimo

1. **Alimentazione:** procurare una LiPo 1S 25C 300–600 mAh (BT2.0) per alimentare i motori con la corrente nominale di una cella. Bypassa il buck-boost, eliminando i brownout sui transitori PWM.
2. **Frame 2.0:** stampare un frame che avvolga i coreless 8520, smorzando le vibrazioni meccaniche trasmesse alla cella IMU (meno rumore gyro/accel → PID più stabile). Pipeline in `docs/grounding/08-FRAME-DESIGN-GENERATIVO.md`.
3. **Fase 1 — PID attitude:** controllore di assetto (roll/pitch/yaw rate + outer loop angle), tuning in tethered. Topic `/drone_1/cmd_attitude` (`geometry_msgs/Quaternion`) + task `task_pid_attitude` @ 1 kHz su Core 1.
