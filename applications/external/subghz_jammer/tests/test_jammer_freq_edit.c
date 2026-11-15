/*
 * Host unit tests for the Sub-Ghz jammer frequency-edit helpers
 * (jammer_freq_edit.c).
 *
 * Compiled and run on a host toolchain (gcc/clang) with NO Flipper/Furi
 * dependency, so the kHz<->Hz conversion and band clamping are validated on
 * the host before they ship on device.
 *
 *   ./test_host.sh tests/test_jammer_freq_edit.c jammer_freq_edit.c
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "jammer_freq_edit.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(expr, expected, label)                                   \
    do {                                                               \
        g_checks++;                                                    \
        uint32_t _expected = (uint32_t)(expected);                     \
        uint32_t _got = (uint32_t)(expr);                             \
        if(_got != _expected) {                                        \
            g_failures++;                                              \
            printf("FAIL %s: expected %" PRIu32 ", got %" PRIu32 "\n", \
                label, _expected, _got);                               \
        }                                                              \
    } while(0)

static void test_khz_to_hz(void) {
    CHECK(freq_khz_to_hz(300000u), 300000000u, "khz_to_hz min");
    CHECK(freq_khz_to_hz(433920u), 433920000u, "khz_to_hz 433");
    CHECK(freq_khz_to_hz(928000u), 928000000u, "khz_to_hz max");
    CHECK(freq_khz_to_hz(1u), 1000u, "khz_to_hz 1");
}

static void test_hz_to_khz(void) {
    CHECK(freq_hz_to_khz(300000000u), 300000u, "hz_to_khz min");
    CHECK(freq_hz_to_khz(433920000u), 433920u, "hz_to_khz 433");
    CHECK(freq_hz_to_khz(928000000u), 928000u, "hz_to_khz max");
    CHECK(freq_hz_to_khz(1500u), 1u, "hz_to_khz sub-khz rounds down");
}

static void test_clamp(void) {
    CHECK(freq_khz_clamp(0u), 300000u, "clamp below min");
    CHECK(freq_khz_clamp(299999u), 300000u, "clamp just below min");
    CHECK(freq_khz_clamp(300000u), 300000u, "clamp == min");
    CHECK(freq_khz_clamp(500000u), 500000u, "clamp mid");
    CHECK(freq_khz_clamp(928000u), 928000u, "clamp == max");
    CHECK(freq_khz_clamp(928001u), 928000u, "clamp just above max");
    CHECK(freq_khz_clamp(999999u), 928000u, "clamp above max");
}

static void test_roundtrip(void) {
    /* kHz -> Hz -> kHz is stable for every in-band kHz. */
    for(uint32_t khz = 300000u; khz <= 928000u; khz += 1000u) {
        uint32_t back = freq_hz_to_khz(freq_khz_to_hz(khz));
        CHECK(back, khz, "roundtrip");
    }
    /* min and max map to canonical kHz. */
    CHECK(freq_hz_to_khz(freq_khz_to_hz(300000u)), 300000u, "roundtrip min");
    CHECK(freq_hz_to_khz(freq_khz_to_hz(928000u)), 928000u, "roundtrip max");
}

int main(void) {
    test_khz_to_hz();
    test_hz_to_khz();
    test_clamp();
    test_roundtrip();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if(g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("ALL TESTS FAILED\n");
    return 1;
}
