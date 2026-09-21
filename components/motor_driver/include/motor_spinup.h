#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "drone_types.h"

// Vincolo di spin-up dei motori coreless.
//
// Un coreless fermo è una resistenza pura (~1 Ω): un duty alto applicato a
// motore fermo tira 4-5× la corrente di regime finché la back-EMF non cresce,
// e con più motori che partono insieme il rail dell'ESP32-S3 va in brownout
// (sessione 2026-09-21). Regola: da 0 si esce solo con una rampa fino a
// spin_min e una sosta a spin_min; sopra spin_min nessuna limitazione.
// Vale per qualsiasi produttore di comando (cmd_motor_test, PID, failsafe).
//
// Modulo puro, senza dipendenze ESP-IDF: testabile su host (test/).
typedef struct {
    float    spin_min_pct;   // duty di fine spin-up, sopra il quale il comando passa intatto
    int64_t  ramp_us;        // durata rampa 0 → spin_min
    int64_t  hold_us;        // sosta a spin_min prima di liberare il comando
    bool     active[4];      // motore uscito da 0 (spin-up in corso o completato)
    int64_t  t0_us[4];       // istante in cui il motore ha lasciato 0
} motor_spinup_t;

void motor_spinup_init(motor_spinup_t *s, float spin_min_pct,
                       uint32_t ramp_ms, uint32_t hold_ms);

// Applica il vincolo: `req` è il comando richiesto, `out` quello da mandare
// ai motori. Ritorna la bitmask dei motori che iniziano lo spin-up in questa
// chiamata (bit i = motore i), utile per il log.
uint8_t motor_spinup_apply(motor_spinup_t *s, const motor_cmd_t *req,
                           motor_cmd_t *out, int64_t now_us);
