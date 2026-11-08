/* Host test for the real jammer_signal.c (Furi deps stubbed). Verifies the
   Pulse-mode duty cycle end-to-end and that Carrier mode is unchanged. */
#include "jammer_signal.h"
#include "jammer_freq.h"
#include <stdio.h>
#include <stdlib.h>

static uint32_t duty_pct(uint32_t period, JammerSignalMode mode, uint32_t ratio) {
    JammerSignal* sig = jammer_signal_alloc();
    sig->mode = mode;
    sig->carrier_period_us = period;
    sig->pulse_ratio = ratio;
    uint32_t high = 0, low = 0;
    for(int j = 0; j < 1000; j++) {
        LevelDuration ld = jammer_signal_yield(sig);
        if(level_duration_get_level(ld)) high += level_duration_get_duration(ld);
        else low += level_duration_get_duration(ld);
    }
    jammer_signal_free(sig);
    /* 1000 iterations = 500 periods; high = 500 * on_us */
    uint32_t on_us = high / 500;
    return on_us * 100 / period;
}

/* Task 6.1: the last hop in a hop/sweep/pingpong band must land exactly on
   band_max. The old integer-division math (step = span/(n-1); min + step*idx)
   undershoots band_max by the remainder whenever span isn't evenly divisible. */
static int test_hop_band_max(void) {
    int fails = 0;
    const uint32_t min = 433000000u;
    const uint32_t max = 435000000u;
    const uint32_t count = 7u;
    /* First hop = band_min. */
    if(jammer_signal_hop_frequency(min, max, 0, count) != min) {
        printf("FAIL hop[0] = %u (want %u)\n",
            jammer_signal_hop_frequency(min, max, 0, count), min);
        fails++;
    } else {
        printf("ok   hop[0] = %u\n", jammer_signal_hop_frequency(min, max, 0, count));
    }
    /* Last hop = band_max (the bug: currently undershoots by the remainder). */
    if(jammer_signal_hop_frequency(min, max, count - 1, count) != max) {
        printf("FAIL hop[%u] = %u (want %u)\n", count - 1,
            jammer_signal_hop_frequency(min, max, count - 1, count), max);
        fails++;
    } else {
        printf("ok   hop[%u] = %u\n",
            count - 1, jammer_signal_hop_frequency(min, max, count - 1, count));
    }
    /* Non-even span (span=10, count=4 -> step=3, last hop undershoots by 1). */
    if(jammer_signal_hop_frequency(0u, 10u, 3, 4) != 10u) {
        printf("FAIL hop span2 = %u (want 10)\n",
            jammer_signal_hop_frequency(0u, 10u, 3, 4));
        fails++;
    } else {
        printf("ok   hop span2 = %u\n", jammer_signal_hop_frequency(0u, 10u, 3, 4));
    }
    if(fails) {
        printf("SOME HOP TESTS FAILED\n");
    }
    return fails;
}

/* Task 1.5: hop-queue ring buffer — init, FIFO order, and drop-oldest when full. */
static int test_hop_queue(void) {
    int fails = 0;
    JammerHopQueue q;

    jammer_hop_queue_init(&q);
    if(q.count != 0u || q.head != 0u || q.tail != 0u) {
        printf("FAIL queue init (count=%u head=%u tail=%u)\n", q.count, q.head, q.tail);
        fails++;
    }

    /* Enqueue 17 values (100,110,...,260); the 17th overflows and drops 100. */
    for(uint32_t i = 0u; i < 17u; i++) {
        jammer_hop_queue_enqueue(&q, 100u + i * 10u);
    }
    if(q.count != JAMMER_HOP_QUEUE_LEN) {
        printf("FAIL queue full count=%u (want %u)\n", q.count, JAMMER_HOP_QUEUE_LEN);
        fails++;
    }
    if(q.head != 1u) {
        printf("FAIL queue head=%u (want 1 after wrap)\n", q.head);
        fails++;
    }
    /* FIFO order read from head: 110,120,...,260 (100 dropped). */
    int fifo_ok = 1;
    for(uint32_t i = 0u; i < q.count; i++) {
        uint32_t expected = 110u + i * 10u; /* 110..260 */
        uint32_t got = q.freq[(q.head + i) % JAMMER_HOP_QUEUE_LEN];
        if(got != expected) {
            printf("FAIL fifo[%u]=%u want %u\n", i, got, expected);
            fifo_ok = 0;
            fails++;
        }
    }
    if(fifo_ok) printf("ok   ring FIFO order preserved across wrap\n");

    /* jammer_signal_hop_enqueue routes into signal->hop_q. */
    JammerSignal* sig = jammer_signal_alloc();
    jammer_signal_hop_enqueue(sig, 433920000u);
    if(sig->hop_q.count != 1u || sig->hop_q.current_hz != 433920000u) {
        printf("FAIL signal_hop_enqueue (count=%u cur=%u)\n", sig->hop_q.count, sig->hop_q.current_hz);
        fails++;
    } else {
        printf("ok   signal_hop_enqueue staged target\n");
    }
    jammer_signal_free(sig);

    return fails;
}

int main(void) {
    int fails = 0;
    uint32_t ratios[] = {0, 10, 25, 50, 75, 90, 100};
    for(int i = 0; i < 7; i++) {
        uint32_t pct = duty_pct(1000, JammerSignalModePulse, ratios[i]);
        uint32_t want = ratios[i];
        if((int32_t)pct - (int32_t)want < -1 || (int32_t)pct - (int32_t)want > 1) {
            printf("FAIL pulse ratio=%u: duty=%u%% (want ~%u%%)\n", ratios[i], pct, want);
            fails++;
        } else {
            printf("ok   pulse ratio=%u -> duty=%u%%\n", ratios[i], pct);
        }
    }
    /* Carrier must stay 50% */
    {
        uint32_t pct = duty_pct(1000, JammerSignalModeCarrier, 50);
        if(pct != 50) { printf("FAIL carrier duty=%u%% (want 50%%)\n", pct); fails++; }
        else printf("ok   carrier duty=50%%\n");
    }
    /* Task 6.1: hop/sweep/pingpong must reach band_max exactly. */
    fails += test_hop_band_max();
    /* Task 1.5: hop-queue ring buffer. */
    fails += test_hop_queue();
    printf("\n%s\n", fails ? "SOME TESTS FAILED" : "ALL JAMMER_SIGNAL TESTS PASSED");
    return fails ? 1 : 0;
}
