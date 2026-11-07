/*
 * Host unit test for the EU-region TX gate decision (jammer_eu_gate_blocks).
 *
 * This is the pure logic behind jammer_radio_pre_tx_check: it must block TX in
 * the EU ISM band (433.05-434.79 MHz, EN 300 220) unless the user opts out via
 * the bypass_region bit. Run with:
 *   ./test_host.sh tests/test_jammer_eu_gate.c jammer_signal.c
 */
#include "jammer_signal.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* --- tiny test harness (mirrors the other jammer host tests) --- */
static int g_failures = 0;
static int g_checks = 0;

static void CHECK(bool expr, bool expected, uint32_t freq, uint32_t bypass) {
    g_checks++;
    if(expr != expected) {
        g_failures++;
        printf(
            "FAIL: jammer_eu_gate_blocks(%lu, 0x%lx) = %s, expected %s\n",
            (unsigned long)freq,
            (unsigned long)bypass,
            expr ? "true" : "false",
            expected ? "true" : "false");
    }
}

int main(void) {
    /* EU ISM band edges and center, gate active (bypass_region == 0). */
    CHECK(jammer_eu_gate_blocks(433050000u, 0u), true, 433050000u, 0u); /* EU min */
    CHECK(jammer_eu_gate_blocks(434790000u, 0u), true, 434790000u, 0u); /* EU max */
    CHECK(jammer_eu_gate_blocks(433860000u, 0u), true, 433860000u, 0u); /* EU 433.86 */

    /* EU band, user opted out (bypass_region bit0 set) => allowed. */
    CHECK(jammer_eu_gate_blocks(433050000u, 1u), false, 433050000u, 1u);
    CHECK(jammer_eu_gate_blocks(434790000u, 1u), false, 434790000u, 1u);
    CHECK(jammer_eu_gate_blocks(433860000u, 1u), false, 433860000u, 1u);

    /* Non-EU bands are never gated by this rule. */
    CHECK(jammer_eu_gate_blocks(868000000u, 0u), false, 868000000u, 0u); /* 868 MHz */
    CHECK(jammer_eu_gate_blocks(868000000u, 1u), false, 868000000u, 1u);
    CHECK(jammer_eu_gate_blocks(150000000u, 0u), false, 150000000u, 0u); /* 150 MHz */

    /* Just outside the EU band edges => allowed. */
    CHECK(jammer_eu_gate_blocks(433049999u, 0u), false, 433049999u, 0u); /* below min */
    CHECK(jammer_eu_gate_blocks(434790001u, 0u), false, 434790001u, 0u); /* above max */

    /* Only bit0 of bypass_region matters; higher bits are ignored. */
    CHECK(jammer_eu_gate_blocks(433860000u, 0x00000100u), true, 433860000u, 0x100u);
    CHECK(jammer_eu_gate_blocks(433860000u, 0x00000003u), false, 433860000u, 0x3u);

    printf("\nEU gate: %d checks, %d failures\n", g_checks, g_failures);
    if(g_failures > 0) {
        printf("RESULT: FAIL\n");
        return 1;
    }
    printf("RESULT: PASS\n");
    return 0;
}
