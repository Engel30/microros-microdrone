#include "pid_controller.h"

void pid_init(pid_state_t *pid, float kp, float ki, float kd, float out_min, float out_max)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->output_min = out_min;
    pid->output_max = out_max;
}

float pid_update(pid_state_t *pid, float setpoint, float measurement, float dt)
{
    // TODO: implementare PID con anti-windup (Fase 1)
    return 0.0f;
}

void pid_reset(pid_state_t *pid)
{
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}
