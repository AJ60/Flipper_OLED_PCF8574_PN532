/*
 * Host unit tests for the Sub-Ghz jammer pure-math module (jammer_freq.c).
 *
 * Compiled and run on a host toolchain (gcc/clang) with NO Flipper/Furi
 * dependency, so the scheduling math is validated before it runs on device.
 *
 *   gcc -o test_jammer_freq jammer_freq.c test_jammer_freq.c && ./test_jammer_freq
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

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

static void test_sweep_linear(void) {
    const uint32_t min = 433000000u;
    const uint32_t max = 435000000u;
    const uint32_t points = 5u;
    const uint32_t step = (max - min) / points; /* 400000 */

    CHECK(jammer_freq_sweep_point(min, max, 0, points), min,
        jammer_freq_sweep_point(min, max, 0, points), "sweep[0]");
    CHECK(jammer_freq_sweep_point(min, max, 1, points), min + step,
        jammer_freq_sweep_point(min, max, 1, points), "sweep[1]");
    CHECK(jammer_freq_sweep_point(min, max, 2, points), min + 2u * step,
        jammer_freq_sweep_point(min, max, 2, points), "sweep[2]");
    CHECK(jammer_freq_sweep_point(min, max, 3, points), min + 3u * step,
        jammer_freq_sweep_point(min, max, 3, points), "sweep[3]");
    CHECK(jammer_freq_sweep_point(min, max, 4, points), min + 4u * step,
        jammer_freq_sweep_point(min, max, 4, points), "sweep[4] (== max)");
    /* Wraps around. */
    CHECK(jammer_freq_sweep_point(min, max, 5, points), min,
        jammer_freq_sweep_point(min, max, 5, points), "sweep[5] wraps");
    CHECK(jammer_freq_sweep_point(min, max, 6, points), min + step,
        jammer_freq_sweep_point(min, max, 6, points), "sweep[6] wraps");
}

static void test_sweep_guards(void) {
    /* points < 1 -> min. */
    CHECK(jammer_freq_sweep_point(433000000u, 435000000u, 3, 0), 433000000u,
        jammer_freq_sweep_point(433000000u, 435000000u, 3, 0), "sweep points=0");
    /* Degenerate band (max <= min) -> min. */
    CHECK(jammer_freq_sweep_point(433000000u, 433000000u, 3, 5), 433000000u,
        jammer_freq_sweep_point(433000000u, 433000000u, 3, 5), "sweep flat band");
}

static void test_pingpong(void) {
    const uint32_t points = 4u;
    /* Bounce: 0,1,2,3,2,1,0,1,... period = 2*(points-1) = 6. */
    const uint32_t expect[8] = {0u, 1u, 2u, 3u, 2u, 1u, 0u, 1u};
    for(uint32_t i = 0; i < 8u; i++) {
        CHECK(jammer_freq_pingpong(i, points), expect[i],
            jammer_freq_pingpong(i, points), "pingpong");
    }
    /* points < 2 -> always 0. */
    CHECK(jammer_freq_pingpong(10u, 1u), 0u,
        jammer_freq_pingpong(10u, 1u), "pingpong points=1");
}

int main(void) {
    test_sweep_linear();
    test_sweep_guards();
    test_pingpong();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if(g_failures == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    return 1;
}
