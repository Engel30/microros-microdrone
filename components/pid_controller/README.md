# pid_controller

Controller PID generico riutilizzabile per roll, pitch, yaw e loop esterni.

## Stato: stub (da implementare in Fase 1)

## API

- `pid_init(pid_state_t *pid, kp, ki, kd, out_min, out_max)` — Inizializza PID con guadagni e limiti output
- `pid_update(pid_state_t *pid, setpoint, measurement, dt)` — Calcola output PID
- `pid_reset(pid_state_t *pid)` — Azzera integrale e errore precedente

## Utilizzo previsto

- Fase 1: 3 istanze (roll, pitch, yaw) a 1kHz in task_pid_attitude
- Fase 2: istanze aggiuntive per velocity hold (50Hz)
- Fase 3: istanze per position control (20Hz)

## Dipendenze

`common`
