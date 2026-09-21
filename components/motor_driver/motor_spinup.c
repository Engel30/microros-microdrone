#include "motor_spinup.h"

void motor_spinup_init(motor_spinup_t *s, float spin_min_pct,
                       uint32_t ramp_ms, uint32_t hold_ms)
{
    s->spin_min_pct = spin_min_pct;
    s->ramp_us = (int64_t)ramp_ms * 1000;
    s->hold_us = (int64_t)hold_ms * 1000;
    for (int i = 0; i < 4; i++) {
        s->active[i] = false;
        s->t0_us[i] = 0;
    }
}

uint8_t motor_spinup_apply(motor_spinup_t *s, const motor_cmd_t *req,
                           motor_cmd_t *out, int64_t now_us)
{
    uint8_t started = 0;
    out->timestamp_us = req->timestamp_us;

    for (int i = 0; i < 4; i++) {
        float r = req->motor[i];

        // Comando a 0 (o disarm/watchdog): motore fermo, la prossima uscita
        // da 0 rifà lo spin-up da capo.
        if (r <= 0.0f) {
            s->active[i] = false;
            out->motor[i] = 0.0f;
            continue;
        }

        if (!s->active[i]) {
            s->active[i] = true;
            s->t0_us[i] = now_us;
            started |= (uint8_t)(1u << i);
        }

        // Tetto al comando in funzione del tempo trascorso dall'uscita da 0.
        // Un comando sotto spin_min passa intatto (min): l'inrush è già basso.
        int64_t el = now_us - s->t0_us[i];
        float cap;
        if (el < s->ramp_us) {
            cap = s->spin_min_pct * (float)el / (float)s->ramp_us;
        } else if (el < s->ramp_us + s->hold_us) {
            cap = s->spin_min_pct;
        } else {
            cap = 100.0f;
        }
        out->motor[i] = (r < cap) ? r : cap;
    }
    return started;
}
