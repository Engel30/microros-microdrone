#pragma once

typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
    float output_min, output_max;
} pid_state_t;

void pid_init(pid_state_t *pid, float kp, float ki, float kd, float out_min, float out_max);
float pid_update(pid_state_t *pid, float setpoint, float measurement, float dt);
void pid_reset(pid_state_t *pid);
