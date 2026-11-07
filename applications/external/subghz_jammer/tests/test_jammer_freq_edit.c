/*
 * Host unit tests for the Sub-Ghz jammer frequency-edit helpers
 * (jammer_freq_edit.c).
 *
 * Compiled and run on a host toolchain (gcc/clang) with NO Flipper/Furi
 * dependency, so the frequency-index math is validated on the host before it
 * ships on device. check_host.h is not part of this repo, so the CHECK macro
 * is defined locally here, mirroring tests/test_jammer_freq.c.
 *
 *   ./test_host.sh tests/test_jammer_freq_edit.c jammer_freq_edit.c
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "jammer_freq_edit.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(expr, expected, got, label)                                    \
    do {                                                                     \
        g_checks++;                                                          \
        uint32_t _expected = (uint32_t)(expected);                           \
        uint32_t _got = (uint32_t)(got);                                     \
        if(_got != _expected) {                                              \
            g_failures++;                                                    \
            printf("FAIL %s: expected %" PRIu32 ", got %" PRIu32 "\n", label, \
                _expected, _got);                                             \
        }                                                                      \
    } while(0)

/* CHECK for int32_t results (the helper can legitimately return -1). */
#define CHECK_I32(expr, expected, label)                                     \
    do {                                                                     \
        g_checks++;                                                          \
        int32_t _expected = (int32_t)(expected);                             \
        int32_t _got = (int32_t)(expr);                                      \
        if(_got != _expected) {                                              \
            g_failures++;                                                    \
            printf("FAIL %s: expected %" PRId32 ", got %" PRId32 "\n", label, \
                _expected, _got);                                            \
        }                                                                    \
    } while(0)

static void test_freq_hz_to_index(void) {
    /* Below the band -> index 0. */
    CHECK(freq_hz_to_index(299000000u), 0, freq_hz_to_index(299000000u),
        "hz_to_index below min");
    /* Lowest in-band frequency -> index 0. */
    CHECK(freq_hz_to_index(300000000u), 0, freq_hz_to_index(300000000u),
        "hz_to_index == min");
    /* Highest in-band frequency -> last index. */
    CHECK(freq_hz_to_index(928000000u), FREQ_STEP_COUNT - 1,
        freq_hz_to_index(928000000u), "hz_to_index == max");
    /* Above the band -> last index (clamped). */
    CHECK(freq_hz_to_index(1000000000u), FREQ_STEP_COUNT - 1,
        freq_hz_to_index(1000000000u), "hz_to_index above max");
    /* A mid-band frequency maps to a strictly positive, in-range index. */
    uint16_t mid = freq_hz_to_index(433000000u);
    g_checks++;
    if(mid == 0 || mid >= FREQ_STEP_COUNT) {
        g_failures++;
        printf("FAIL hz_to_index mid range: got %u\n", (uint32_t)mid);
    }
}

static void test_freq_index_to_hz(void) {
    /* Index 0 -> minimum frequency. */
    CHECK(freq_index_to_hz(0), FREQ_MIN_HZ, freq_index_to_hz(0),
        "index_to_hz index 0");
    /* Last index -> maximum frequency. */
    CHECK(freq_index_to_hz(FREQ_STEP_COUNT - 1), FREQ_MAX_HZ,
        freq_index_to_hz(FREQ_STEP_COUNT - 1), "index_to_hz last index");
    /* Every index maps back into the inclusive band. */
    for(uint16_t i = 0; i < FREQ_STEP_COUNT; i++) {
        uint32_t hz = freq_index_to_hz(i);
        g_checks++;
        if(hz < FREQ_MIN_HZ || hz > FREQ_MAX_HZ) {
            g_failures++;
            printf("FAIL index_to_hz out of band: i=%u hz=%" PRIu32 "\n",
                (uint32_t)i, hz);
        }
    }
    /* Out-of-range index clamps to the maximum frequency. */
    CHECK(freq_index_to_hz(0xFFFF), FREQ_MAX_HZ, freq_index_to_hz(0xFFFF),
        "index_to_hz overflow clamp");
}

static void test_freq_index_hz_roundtrip(void) {
    /* index -> hz -> index is stable across the whole band. */
    for(uint16_t i = 0; i < FREQ_STEP_COUNT; i++) {
        uint32_t hz = freq_index_to_hz(i);
        uint16_t back = freq_hz_to_index(hz);
        CHECK(back, i, back, "roundtrip");
    }
    /* min and max hz map to their canonical indices. */
    CHECK(freq_hz_to_index(FREQ_MIN_HZ), 0, freq_hz_to_index(FREQ_MIN_HZ),
        "roundtrip min");
    CHECK(freq_hz_to_index(FREQ_MAX_HZ), FREQ_STEP_COUNT - 1,
        freq_hz_to_index(FREQ_MAX_HZ), "roundtrip max");
}

static void test_freq_edit_step_index(void) {
    /* FREQ_STEP_COUNT == 629. */
    /* Up from the middle. */
    CHECK_I32(freq_edit_step_index(100, 1), 101, "step up mid");
    /* Down from the middle. */
    CHECK_I32(freq_edit_step_index(100, -1), 99, "step down mid");
    /* Clamp at the low end. */
    CHECK_I32(freq_edit_step_index(0, -1), 0, "step down clamp low");
    /* Clamp at the high end. */
    CHECK_I32(freq_edit_step_index(628, 1), 628, "step up clamp high");
    /* Out-of-range index -> -1. */
    CHECK_I32(freq_edit_step_index(629, 1), -1, "step out of range hi");
    CHECK_I32(freq_edit_step_index(-1, 1), -1, "step out of range lo");
    /* Invalid delta -> -1. */
    CHECK_I32(freq_edit_step_index(100, 2), -1, "step bad delta +2");
    CHECK_I32(freq_edit_step_index(100, -2), -1, "step bad delta -2");
    CHECK_I32(freq_edit_step_index(100, 0), -1, "step bad delta 0");
}

/* Every step of +1 over the whole band must advance Hz by exactly 1 MHz. */
static void test_freq_edit_step_hz(void) {
    for(int32_t i = 0; i < (int32_t)FREQ_STEP_COUNT - 1; i++) {
        int32_t next = freq_edit_step_index(i, 1);
        g_checks++;
        if(next != i + 1) {
            g_failures++;
            printf("FAIL step_hz index: i=%d next=%d\n", i, next);
            continue;
        }
        uint32_t base_hz = freq_index_to_hz((uint16_t)i);
        uint32_t next_hz = freq_index_to_hz((uint16_t)next);
        if(next_hz != base_hz + FREQ_STEP_HZ) {
            g_failures++;
            printf("FAIL step_hz hz: i=%d base=%u next=%u\n", i, base_hz, next_hz);
        }
    }
}

static void test_freq_hz_to_text(void) {
    char buf[32];
    /* 300.00 MHz. */
    freq_hz_to_text(300000000u, buf, sizeof(buf));
    g_checks++;
    if(strcmp(buf, "300.00 MHz") != 0) {
        g_failures++;
        printf("FAIL hz_to_text 300: got \"%s\"\n", buf);
    }
    /* 433.00 MHz. */
    freq_hz_to_text(433000000u, buf, sizeof(buf));
    g_checks++;
    if(strcmp(buf, "433.00 MHz") != 0) {
        g_failures++;
        printf("FAIL hz_to_text 433: got \"%s\"\n", buf);
    }
    /* 928.00 MHz. */
    freq_hz_to_text(928000000u, buf, sizeof(buf));
    g_checks++;
    if(strcmp(buf, "928.00 MHz") != 0) {
        g_failures++;
        printf("FAIL hz_to_text 928: got \"%s\"\n", buf);
    }
}

int main(void) {
    test_freq_hz_to_index();
    test_freq_index_to_hz();
    test_freq_index_hz_roundtrip();
    test_freq_hz_to_text();
    test_freq_edit_step_index();
    test_freq_edit_step_hz();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if(g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("ALL TESTS FAILED\n");
    return 1;
}
