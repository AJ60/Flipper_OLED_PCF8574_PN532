/* Mimic jammer_signal Yield() Pulse-mode logic to verify duty-cycle sign.
   Emits the PRE-toggle level (matching the carrier branch at jammer_signal.c:49). */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "jammer_freq.h"

static uint32_t on_us_calc(uint32_t period, uint32_t ratio) {
    if(period < 2) period = 2;
    if(ratio > 100) ratio = 100;
    uint32_t on = period * ratio / 100;
    if(on < 1) on = 1;
    if(on > period) on = period;
    return on;
}

/* PLAN version: dur = level ? (period - on) : on */
static uint32_t plan_high_duty(uint32_t period, uint32_t ratio) {
    uint32_t on = on_us_calc(period, ratio);
    uint32_t high = 0;
    bool level = false;
    for(int i = 0; i < 1000; i++) {
        uint32_t dur = level ? (period - on) : on;
        if(level) high += dur;
        level = !level;
    }
    return high / 1000;
}

/* CORRECTED version: dur = level ? on : (period - on) */
static uint32_t corr_high_duty(uint32_t period, uint32_t ratio) {
    uint32_t on = on_us_calc(period, ratio);
    uint32_t high = 0;
    bool level = false;
    for(int i = 0; i < 1000; i++) {
        uint32_t dur = level ? on : (period - on);
        if(level) high += dur;
        level = !level;
    }
    return high / 1000;
}

int main(void) {
    uint32_t period = 1000;
    int failures = 0;

    /* The corrected Pulse-mode high-duty must grow monotonically with the ratio
     * (more "on" time -> more high time). The sign-flipped "plan" version is
     * monotonically DECREASING, so this ordering check catches a regression to the
     * wrong duty-cycle sign. */
#define CHECK_MONOTONIC_INCREASING(a, b, c, label) do {                             \
        uint32_t _a = (a), _b = (b), _c = (c);                                      \
        int ok = (_a < _b && _b < _c);                                              \
        printf("  %-22s %u < %u < %u : %s\n", (label), _a, _b, _c, ok ? "PASS" : "FAIL"); \
        if(!ok) failures++;                                                         \
    } while(0)

    printf("ratio=10  plan_high=%u  corr_high=%u  (want ~10)\n",
           plan_high_duty(period, 10), corr_high_duty(period, 10));
    printf("ratio=50  plan_high=%u  corr_high=%u  (want ~50)\n",
           plan_high_duty(period, 50), corr_high_duty(period, 50));
    printf("ratio=90  plan_high=%u  corr_high=%u  (want ~90)\n",
           plan_high_duty(period, 90), corr_high_duty(period, 90));

    /* Corrected version: high-duty increases with ratio. */
    CHECK_MONOTONIC_INCREASING(corr_high_duty(period, 10),
                               corr_high_duty(period, 50),
                               corr_high_duty(period, 90), "corr increasing");
    /* Plan (buggy) version: high-duty decreases with ratio. */
    CHECK_MONOTONIC_INCREASING(plan_high_duty(period, 90),
                               plan_high_duty(period, 50),
                               plan_high_duty(period, 10), "plan decreasing");

    printf("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures,
           failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
