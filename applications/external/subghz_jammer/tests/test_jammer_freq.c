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
    const uint32_t step = (max - min) / (points - 1); /* 500000 */

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

static void test_sweep_undershoot(void) {
    /* Non-even span (span=1000000, points=4 -> step=333333): the last point
       undershoots max by 1 Hz without the deferral fix. */
    CHECK(jammer_freq_sweep_point(433000000u, 434000000u, 3, 4), 434000000u,
        jammer_freq_sweep_point(433000000u, 434000000u, 3, 4), "sweep last==max");
    CHECK(jammer_freq_sweep_point(433000000u, 434000000u, 0, 4), 433000000u,
        jammer_freq_sweep_point(433000000u, 434000000u, 0, 4), "sweep first==min");
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

static void test_hop_count_for_span(void) {
    /* span=2MHz, step=250kHz -> 8 intervals + 1 = 9 targets. */
    CHECK(jammer_freq_hop_count_for_span(433000000u, 435000000u, 250000u), 9u,
        jammer_freq_hop_count_for_span(433000000u, 435000000u, 250000u), "hop_count 2M/250k");
    /* span=1MHz, step=250kHz -> 5. */
    CHECK(jammer_freq_hop_count_for_span(433000000u, 434000000u, 250000u), 5u,
        jammer_freq_hop_count_for_span(433000000u, 434000000u, 250000u), "hop_count 1M/250k");
    /* step_hz == 0 -> 1. */
    CHECK(jammer_freq_hop_count_for_span(433000000u, 435000000u, 0u), 1u,
        jammer_freq_hop_count_for_span(433000000u, 435000000u, 0u), "hop_count step=0");
    /* Degenerate band -> 1. */
    CHECK(jammer_freq_hop_count_for_span(433000000u, 433000000u, 250000u), 1u,
        jammer_freq_hop_count_for_span(433000000u, 433000000u, 250000u), "hop_count flat band");
    /* span < step -> 2 (min + one step). */
    CHECK(jammer_freq_hop_count_for_span(433000000u, 433100000u, 250000u), 2u,
        jammer_freq_hop_count_for_span(433000000u, 433100000u, 250000u), "hop_count span<step");
}

static void test_step_target(void) {
    const uint32_t min = 433000000u;
    const uint32_t max = 435000000u;
    const uint32_t step = 250000u;
    CHECK(jammer_freq_step_target(min, step, 0, min, max), min,
        jammer_freq_step_target(min, step, 0, min, max), "step[0]");
    CHECK(jammer_freq_step_target(min, step, 1, min, max), min + step,
        jammer_freq_step_target(min, step, 1, min, max), "step[1]");
    /* idx*step == span -> max exactly. */
    CHECK(jammer_freq_step_target(min, step, 8, min, max), max,
        jammer_freq_step_target(min, step, 8, min, max), "step[8]==max");
    /* idx*step > span -> clamp to max. */
    CHECK(jammer_freq_step_target(min, step, 9, min, max), max,
        jammer_freq_step_target(min, step, 9, min, max), "step[9] clamp");
    /* step_hz == 0 -> base. */
    CHECK(jammer_freq_step_target(min, 0u, 5, min, max), min,
        jammer_freq_step_target(min, 0u, 5, min, max), "step step=0");
    /* base offset applied. */
    CHECK(jammer_freq_step_target(434000000u, step, 2, min, max), 434500000u,
        jammer_freq_step_target(434000000u, step, 2, min, max), "step base offset");
}

static void test_dwell_target(void) {
    const uint32_t min = 433000000u;
    const uint32_t max = 435000000u;
    const uint32_t step = 250000u;
    CHECK(jammer_freq_dwell_target(min, max, step, 0), min,
        jammer_freq_dwell_target(min, max, step, 0), "dwell[0]");
    CHECK(jammer_freq_dwell_target(min, max, step, 2), min + 2u * step,
        jammer_freq_dwell_target(min, max, step, 2), "dwell[2]");
    CHECK(jammer_freq_dwell_target(min, max, step, 1000), max,
        jammer_freq_dwell_target(min, max, step, 1000), "dwell[1000] clamp");
    /* Degenerate band -> min. */
    CHECK(jammer_freq_dwell_target(433000000u, 433000000u, step, 3), 433000000u,
        jammer_freq_dwell_target(433000000u, 433000000u, step, 3), "dwell flat band");
    /* step_hz == 0 -> min. */
    CHECK(jammer_freq_dwell_target(min, max, 0u, 3), min,
        jammer_freq_dwell_target(min, max, 0u, 3), "dwell step=0");
}

static void test_noise_hop_target(void) {
    const uint32_t min = 433000000u;
    const uint32_t max = 435000000u;
    const uint32_t step = 500000u;
    CHECK(jammer_freq_noise_hop_target(min, max, step, 0), min,
        jammer_freq_noise_hop_target(min, max, step, 0), "noise[0]");
    CHECK(jammer_freq_noise_hop_target(min, max, step, 1), min + step,
        jammer_freq_noise_hop_target(min, max, step, 1), "noise[1]");
    /* idx*step == span -> max exactly. */
    CHECK(jammer_freq_noise_hop_target(min, max, step, 4), max,
        jammer_freq_noise_hop_target(min, max, step, 4), "noise[4]==max");
    /* idx*step > span -> clamp to max. */
    CHECK(jammer_freq_noise_hop_target(min, max, step, 5), max,
        jammer_freq_noise_hop_target(min, max, step, 5), "noise[5] clamp");
    /* Degenerate band -> min. */
    CHECK(jammer_freq_noise_hop_target(433000000u, 433000000u, step, 3), 433000000u,
        jammer_freq_noise_hop_target(433000000u, 433000000u, step, 3), "noise flat band");
    /* step_hz == 0 -> min. */
    CHECK(jammer_freq_noise_hop_target(min, max, 0u, 3), min,
        jammer_freq_noise_hop_target(min, max, 0u, 3), "noise step=0");
}

static void test_hop_pingpong(void) {
    const uint32_t points = 4u;
    const uint32_t expect[8] = {0u, 1u, 2u, 3u, 2u, 1u, 0u, 1u};
    for(uint32_t i = 0; i < 8u; i++) {
        CHECK(jammer_freq_hop_pingpong(i, points), expect[i],
            jammer_freq_hop_pingpong(i, points), "hop_pingpong");
    }
    /* points < 2 -> always 0. */
    CHECK(jammer_freq_hop_pingpong(10u, 1u), 0u,
        jammer_freq_hop_pingpong(10u, 1u), "hop_pingpong points=1");
    /* Longer period (count=6 -> period 10). */
    const uint32_t big_points = 6u;
    const uint32_t big_expect[12] = {0u, 1u, 2u, 3u, 4u, 5u, 4u, 3u, 2u, 1u, 0u, 1u};
    for(uint32_t i = 0; i < 12u; i++) {
        CHECK(jammer_freq_hop_pingpong(i, big_points), big_expect[i],
            jammer_freq_hop_pingpong(i, big_points), "hop_pingpong big");
    }
}

static void test_hop_queue_next(void) {
    const uint32_t targets[5] = {100u, 200u, 300u, 400u, 500u};
    /* FIFO ring: cursor wraps at count. */
    CHECK(jammer_freq_hop_queue_next(0u, targets, 5u), 100u,
        jammer_freq_hop_queue_next(0u, targets, 5u), "queue[0]");
    CHECK(jammer_freq_hop_queue_next(1u, targets, 5u), 200u,
        jammer_freq_hop_queue_next(1u, targets, 5u), "queue[1]");
    CHECK(jammer_freq_hop_queue_next(4u, targets, 5u), 500u,
        jammer_freq_hop_queue_next(4u, targets, 5u), "queue[4]");
    CHECK(jammer_freq_hop_queue_next(5u, targets, 5u), 100u,
        jammer_freq_hop_queue_next(5u, targets, 5u), "queue[5] wrap");
    CHECK(jammer_freq_hop_queue_next(7u, targets, 5u), 300u,
        jammer_freq_hop_queue_next(7u, targets, 5u), "queue[7] wrap");
    /* count == 1 -> always targets[0]. */
    CHECK(jammer_freq_hop_queue_next(9u, targets, 1u), 100u,
        jammer_freq_hop_queue_next(9u, targets, 1u), "queue count=1");
    /* count == 0 -> 0 sentinel (no target available). */
    CHECK(jammer_freq_hop_queue_next(0u, targets, 0u), 0u,
        jammer_freq_hop_queue_next(0u, targets, 0u), "queue count=0");
    CHECK(jammer_freq_hop_queue_next(123u, targets, 0u), 0u,
        jammer_freq_hop_queue_next(123u, targets, 0u), "queue count=0 big cursor");
    /* NULL targets -> 0. */
    CHECK(jammer_freq_hop_queue_next(0u, NULL, 3u), 0u,
        jammer_freq_hop_queue_next(0u, NULL, 3u), "queue null targets");
}

int main(void) {
    test_sweep_linear();
    test_sweep_guards();
    test_sweep_undershoot();
    test_pingpong();
    test_hop_count_for_span();
    test_step_target();
    test_dwell_target();
    test_noise_hop_target();
    test_hop_pingpong();
    test_hop_queue_next();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if(g_failures == 0) {
        /* clamp: below range -> min */
        if(jammer_freq_clamp_khz(200000, JAMMER_FREQ_MIN_KHZ, JAMMER_FREQ_MAX_KHZ) != 300000u) {
            printf("FAIL clamp low\n");
            return 1;
        }
        /* clamp: above range -> max */
        if(jammer_freq_clamp_khz(1000000, JAMMER_FREQ_MIN_KHZ, JAMMER_FREQ_MAX_KHZ) != 928000u) {
            printf("FAIL clamp high\n");
            return 1;
        }
        /* clamp: in range -> unchanged */
        if(jammer_freq_clamp_khz(433920, JAMMER_FREQ_MIN_KHZ, JAMMER_FREQ_MAX_KHZ) != 433920u) {
            printf("FAIL clamp mid\n");
            return 1;
        }
        /* conversions */
        if(jammer_freq_khz_to_hz(433920u) != 433920000u) {
            printf("FAIL khz->hz\n");
            return 1;
        }
        if(jammer_freq_hz_to_khz(433920000u) != 433920u) {
            printf("FAIL hz->khz\n");
            return 1;
        }

        printf("ALL TESTS PASSED\n");
        return 0;
    }
    return 1;
}
