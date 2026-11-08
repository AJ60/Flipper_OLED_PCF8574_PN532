/*
 * Host unit tests for the Sub-Ghz jammer thermal-throttle model
 * (jammer_thermal.c). Pure math, no Flipper/Furi dependency.
 *
 *   gcc -o test_jammer_thermal jammer_thermal.c test_jammer_thermal.c && ./test_jammer_thermal
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "jammer_thermal.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(expr, expected, got, label)                                    \
    do {                                                                     \
        g_checks++;                                                          \
        uint32_t _expected = (uint32_t)(expected);                           \
        uint32_t _got = (uint32_t)(got);                                     \
        if(_got != _expected) {                                              \
            g_failures++;                                                    \
            printf("FAIL %s: expected %" PRIu32 ", got %" PRIu32 "\n", label, _expected, _got); \
        }                                                                      \
    } while(0)

/* Decision table: clean state -> Ok. */
static void test_decide_ok(void) {
    CHECK(jammer_thermal_decide(30u, 50u, 3800u, 300000u, 1000u, false),
        JammerThermalOk, jammer_thermal_decide(30u, 50u, 3800u, 300000u, 1000u, false), "clean -> Ok");
}

/* Watchdog timeout -> ForceStop (highest priority). */
static void test_decide_watchdog(void) {
    CHECK(jammer_thermal_decide(30u, 50u, 3800u, 300000u, 300000u, false),
        JammerThermalForceStop,
        jammer_thermal_decide(30u, 50u, 3800u, 300000u, 300000u, false), "watchdog -> Stop");
    /* watchdog disabled (0) never stops on its own. */
    CHECK(jammer_thermal_decide(30u, 50u, 3800u, 0u, 9999999u, false),
        JammerThermalOk,
        jammer_thermal_decide(30u, 50u, 3800u, 0u, 9999999u, false), "watchdog off -> Ok");
}

/* Critical battery -> ForceStop. */
static void test_decide_vbat_crit(void) {
    CHECK(jammer_thermal_decide(30u, 50u, 3300u, 300000u, 1000u, false),
        JammerThermalForceStop,
        jammer_thermal_decide(30u, 50u, 3300u, 300000u, 1000u, false), "vbat=V_CRIT -> Stop");
    CHECK(jammer_thermal_decide(30u, 50u, 3000u, 300000u, 1000u, false),
        JammerThermalForceStop,
        jammer_thermal_decide(30u, 50u, 3000u, 300000u, 1000u, false), "vbat<V_CRIT -> Stop");
    /* vbat 0 (unknown) -> don't stop. */
    CHECK(jammer_thermal_decide(30u, 50u, 0u, 300000u, 1000u, false),
        JammerThermalOk,
        jammer_thermal_decide(30u, 50u, 0u, 300000u, 1000u, false), "vbat=0 -> Ok");
}

/* Overheat -> ReduceDuty (first time). */
static void test_decide_overheat(void) {
    CHECK(jammer_thermal_decide(61u, 50u, 3800u, 300000u, 1000u, false),
        JammerThermalReduceDuty,
        jammer_thermal_decide(61u, 50u, 3800u, 300000u, 1000u, false), "hot first -> ReduceDuty");
    /* Already reduced duty, still hot -> ReducePower. */
    CHECK(jammer_thermal_decide(61u, 50u, 3800u, 300000u, 1000u, true),
        JammerThermalReducePower,
        jammer_thermal_decide(61u, 50u, 3800u, 300000u, 1000u, true), "hot again -> ReducePower");
    /* Just at the warning edge (== T_WARN) is NOT over the threshold. */
    CHECK(jammer_thermal_decide(60u, 50u, 3800u, 300000u, 1000u, false),
        JammerThermalOk,
        jammer_thermal_decide(60u, 50u, 3800u, 300000u, 1000u, false), "at T_WARN -> Ok");
}

/* Over current -> ReduceDuty (first time). */
static void test_decide_over_current(void) {
    CHECK(jammer_thermal_decide(40u, 101u, 3800u, 300000u, 1000u, false),
        JammerThermalReduceDuty,
        jammer_thermal_decide(40u, 101u, 3800u, 300000u, 1000u, false), "I>I_THERMAL -> ReduceDuty");
    /* Over max current, duty already reduced -> ReducePower. */
    CHECK(jammer_thermal_decide(40u, 151u, 3800u, 300000u, 1000u, true),
        JammerThermalReducePower,
        jammer_thermal_decide(40u, 151u, 3800u, 300000u, 1000u, true), "I>I_MAX (again) -> ReducePower");
    /* Over the hard limit on the first pass: escalation starts at ReduceDuty. */
    CHECK(jammer_thermal_decide(40u, 151u, 3800u, 300000u, 1000u, false),
        JammerThermalReduceDuty,
        jammer_thermal_decide(40u, 151u, 3800u, 300000u, 1000u, false), "I>I_MAX first -> ReduceDuty");
}

/* Low battery voltage (but above critical) -> ReducePower. */
static void test_decide_vbat_low(void) {
    CHECK(jammer_thermal_decide(30u, 50u, 3400u, 300000u, 1000u, false),
        JammerThermalReducePower,
        jammer_thermal_decide(30u, 50u, 3400u, 300000u, 1000u, false), "V_MIN>vbat<V_CRIT -> ReducePower");
    /* At V_MIN (==) is not below, so no action. */
    CHECK(jammer_thermal_decide(30u, 50u, 3500u, 300000u, 1000u, false),
        JammerThermalOk,
        jammer_thermal_decide(30u, 50u, 3500u, 300000u, 1000u, false), "at V_MIN -> Ok");
}

/* Estimator: monotonic in power level and frequency. */
static void test_est_current(void) {
    /* Higher power draws more (monotonic). */
    uint32_t low = jammer_thermal_est_current_ma(433920000u, 10u, false);
    uint32_t mid = jammer_thermal_est_current_ma(433920000u, 50u, false);
    uint32_t high = jammer_thermal_est_current_ma(433920000u, 100u, false);
    if(!(low < mid && mid < high)) {
        printf("FAIL est current not monotonic: %u %u %u\n", low, mid, high);
        g_failures++;
    }
    /* Higher frequency draws more (monotonic). */
    uint32_t f_low = jammer_thermal_est_current_ma(433000000u, 100u, false);
    uint32_t f_high = jammer_thermal_est_current_ma(915000000u, 100u, false);
    if(!(f_low < f_high)) {
        printf("FAIL est current freq not monotonic: %u %u\n", f_low, f_high);
        g_failures++;
    }
    /* External PA draws more than internal at the same setting. */
    uint32_t internal = jammer_thermal_est_current_ma(915000000u, 100u, false);
    uint32_t external = jammer_thermal_est_current_ma(915000000u, 100u, true);
    if(!(external > internal)) {
        printf("FAIL est external<=internal: %u %u\n", internal, external);
        g_failures++;
    }
    /* power_level 0 (auto -> max) draws the most. */
    uint32_t auto_ma = jammer_thermal_est_current_ma(433920000u, 0u, false);
    uint32_t full_ma = jammer_thermal_est_current_ma(433920000u, 100u, false);
    if(auto_ma < full_ma) {
        printf("FAIL est auto<full: %u %u\n", auto_ma, full_ma);
        g_failures++;
    }
    printf("ok   est current monotonic (low=%u mid=%u high=%u ext=%u auto=%u)\n",
        low, mid, high, external, auto_ma);
}

/* Estimator: junction temp rises with current and ambient. */
static void test_est_temp(void) {
    uint32_t t_cold = jammer_thermal_est_temp_c(25u, 50u);
    uint32_t t_warm = jammer_thermal_est_temp_c(25u, 150u);
    uint32_t t_hot = jammer_thermal_est_temp_c(45u, 150u);
    if(!(t_cold < t_warm && t_warm < t_hot)) {
        printf("FAIL est temp not monotonic: %u %u %u\n", t_cold, t_warm, t_hot);
        g_failures++;
    }
    /* ~100 mA adds ~30 C above ambient. */
    CHECK(jammer_thermal_est_temp_c(25u, 100u), 55u, jammer_thermal_est_temp_c(25u, 100u), "temp +30C@100mA");
    /* Zero current -> no rise. */
    CHECK(jammer_thermal_est_temp_c(25u, 0u), 25u, jammer_thermal_est_temp_c(25u, 0u), "temp no load");
    printf("ok   est temp monotonic (cold=%u warm=%u hot=%u)\n", t_cold, t_warm, t_hot);
}

int main(void) {
    test_decide_ok();
    test_decide_watchdog();
    test_decide_vbat_crit();
    test_decide_overheat();
    test_decide_over_current();
    test_decide_vbat_low();
    test_est_current();
    test_est_temp();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if(g_failures == 0) {
        printf("ALL THERMAL TESTS PASSED\n");
        return 0;
    }
    return 1;
}
