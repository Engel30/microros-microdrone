// Test su host del vincolo di spin-up. Compila ed esegui con:
//   cd components/motor_driver && ./test/run.sh
#include <stdio.h>
#include <math.h>
#include "motor_spinup.h"

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { fails++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.01f)

#define SPIN_MIN 8.0f
#define RAMP_MS  150
#define HOLD_MS  300
#define MS(x) ((int64_t)(x) * 1000)

static motor_cmd_t req4(float a, float b, float c, float d)
{
    motor_cmd_t r = { .motor = {a, b, c, d}, .timestamp_us = 42 };
    return r;
}

static void test_zero_stays_zero(void)
{
    motor_spinup_t s; motor_spinup_init(&s, SPIN_MIN, RAMP_MS, HOLD_MS);
    motor_cmd_t out, r = req4(0, 0, 0, 0);
    uint8_t st = motor_spinup_apply(&s, &r, &out, MS(1000));
    CHECK(st == 0);
    for (int i = 0; i < 4; i++) CHECK(out.motor[i] == 0.0f);
    CHECK(out.timestamp_us == 42);
}

static void test_step_0_to_30_goes_through_ramp_and_hold(void)
{
    motor_spinup_t s; motor_spinup_init(&s, SPIN_MIN, RAMP_MS, HOLD_MS);
    motor_cmd_t out, r = req4(30, 30, 30, 30);
    int64_t t0 = MS(1000);

    uint8_t st = motor_spinup_apply(&s, &r, &out, t0);
    CHECK(st == 0x0F);                       // tutti e 4 partono
    CHECK(NEAR(out.motor[0], 0.0f));         // inizio rampa

    motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS / 2));
    CHECK(NEAR(out.motor[0], SPIN_MIN / 2)); // metà rampa

    motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS));
    CHECK(NEAR(out.motor[0], SPIN_MIN));     // fine rampa → sosta

    st = motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS + HOLD_MS - 1));
    CHECK(st == 0);                          // nessuna nuova partenza
    CHECK(NEAR(out.motor[3], SPIN_MIN));     // ancora in sosta

    motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS + HOLD_MS));
    for (int i = 0; i < 4; i++) CHECK(NEAR(out.motor[i], 30.0f)); // liberato

    r = req4(100, 100, 100, 100);            // sopra spin_min: nessun tetto
    motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS + HOLD_MS + 1));
    CHECK(NEAR(out.motor[0], 100.0f));
}

static void test_request_below_spin_min_passes_unchanged(void)
{
    motor_spinup_t s; motor_spinup_init(&s, SPIN_MIN, RAMP_MS, HOLD_MS);
    motor_cmd_t out, r = req4(3, 0, 0, 0);
    int64_t t0 = MS(1000);
    motor_spinup_apply(&s, &r, &out, t0);                  // partenza: cap = 0
    CHECK(NEAR(out.motor[0], 0.0f));
    motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS));   // cap = 8 > 3
    CHECK(NEAR(out.motor[0], 3.0f));
    motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS + HOLD_MS + 50));
    CHECK(NEAR(out.motor[0], 3.0f));
}

static void test_return_to_zero_restarts_spinup(void)
{
    motor_spinup_t s; motor_spinup_init(&s, SPIN_MIN, RAMP_MS, HOLD_MS);
    motor_cmd_t out, r = req4(30, 0, 0, 0), z = req4(0, 0, 0, 0);
    int64_t t0 = MS(1000);
    motor_spinup_apply(&s, &r, &out, t0);
    motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS + HOLD_MS + 10));
    CHECK(NEAR(out.motor[0], 30.0f));

    motor_spinup_apply(&s, &z, &out, t0 + MS(500));        // watchdog/disarm → 0
    CHECK(out.motor[0] == 0.0f);

    uint8_t st = motor_spinup_apply(&s, &r, &out, t0 + MS(2000)); // nuova partenza
    CHECK(st == 0x01);
    CHECK(NEAR(out.motor[0], 0.0f));                        // rampa da capo
    motor_spinup_apply(&s, &r, &out, t0 + MS(2000 + RAMP_MS + 10));
    CHECK(NEAR(out.motor[0], SPIN_MIN));
}

static void test_motors_are_independent(void)
{
    motor_spinup_t s; motor_spinup_init(&s, SPIN_MIN, RAMP_MS, HOLD_MS);
    motor_cmd_t out, r = req4(30, 0, 0, 0);
    int64_t t0 = MS(1000);
    motor_spinup_apply(&s, &r, &out, t0);
    motor_spinup_apply(&s, &r, &out, t0 + MS(RAMP_MS + HOLD_MS + 10));
    CHECK(NEAR(out.motor[0], 30.0f));

    r = req4(30, 30, 0, 0);                                 // M1 parte dopo
    uint8_t st = motor_spinup_apply(&s, &r, &out, t0 + MS(1000));
    CHECK(st == 0x02);
    CHECK(NEAR(out.motor[0], 30.0f));                       // M0 non toccato
    CHECK(NEAR(out.motor[1], 0.0f));                        // M1 inizia la rampa
    motor_spinup_apply(&s, &r, &out, t0 + MS(1000 + RAMP_MS + 10));
    CHECK(NEAR(out.motor[1], SPIN_MIN));
}

int main(void)
{
    test_zero_stays_zero();
    test_step_0_to_30_goes_through_ramp_and_hold();
    test_request_below_spin_min_passes_unchanged();
    test_return_to_zero_restarts_spinup();
    test_motors_are_independent();
    if (fails) { printf("%d FAIL\n", fails); return 1; }
    printf("OK: tutti i test spin-up passati\n");
    return 0;
}
