/*
 * Host unit tests for the Sub-Ghz jammer pulse-train math
 * (jammer_freq_pulse_on_us in jammer_freq.c).
 *
 * Compiled and run on a host toolchain (gcc/clang) with NO Flipper/Furi
 * dependency, so the pulse-train duty-cycle math is validated before it runs
 * on device.
 *
 *   gcc -o test_pulse jammer_freq.c test_pulse.c && ./test_pulse
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "jammer_freq.h"

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

/* OOK "on" time (us) for a pulse train: period_us * ratio/100, clamped to
 * [1, period_us]. */
static void test_pulse_on_us(void) {
    /* Nominal duty cycles. */
    CHECK(jammer_freq_pulse_on_us(1000, 50), 500, jammer_freq_pulse_on_us(1000, 50), "50%");
    CHECK(jammer_freq_pulse_on_us(1000, 10), 100, jammer_freq_pulse_on_us(1000, 10), "10%");
    CHECK(jammer_freq_pulse_on_us(1000, 90), 900, jammer_freq_pulse_on_us(1000, 90), "90%");
    CHECK(jammer_freq_pulse_on_us(1000, 100), 1000, jammer_freq_pulse_on_us(1000, 100), "100%");

    /* ratio 0 -> minimum 1 us (never a zero-width on-slot). */
    CHECK(jammer_freq_pulse_on_us(1000, 0), 1, jammer_freq_pulse_on_us(1000, 0), "0% -> min 1us");

    /* ratio > 100 clamps to full period. */
    CHECK(jammer_freq_pulse_on_us(1000, 150), 1000, jammer_freq_pulse_on_us(1000, 150), "clamp 150%");

    /* Tiny period is floored to 2 us; on stays >= 1. */
    CHECK(jammer_freq_pulse_on_us(1, 50), 1, jammer_freq_pulse_on_us(1, 50), "tiny period");
    CHECK(jammer_freq_pulse_on_us(2, 25), 1, jammer_freq_pulse_on_us(2, 25), "2us*25% -> 1us floor");
}

int main(void) {
    test_pulse_on_us();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if(g_failures == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    return 1;
}
