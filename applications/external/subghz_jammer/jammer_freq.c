#include "jammer_freq.h"

#define TAG "SubGhzJammerFreq"

uint32_t jammer_freq_sweep_point(uint32_t min, uint32_t max, uint32_t point, uint32_t points) {
    if(points < 2) return min;
    if(max <= min) return min;

    /* Wrap the (unbounded) counter into [0, points-1]. */
    uint32_t idx = point % points;
    /* Point 0 = min, point (points-1) = max; defer the division so the last
       point lands exactly on max even when span isn't divisible by (points-1).
       64-bit intermediate avoids overflow for wide bands. */
    return min + (uint64_t)(max - min) * idx / (points - 1);
}

uint32_t jammer_freq_pingpong(uint32_t step, uint32_t points) {
    if(points < 2) return 0;

    /* Bounce index in [0, 2*(points-1)). */
    uint32_t period = 2 * (points - 1);
    uint32_t pos = step % period;
    if(pos < points) {
        return pos;
    }
    /* Second half: mirror back toward 0. */
    return period - pos;
}

uint32_t jammer_freq_pulse_on_us(uint32_t period_us, uint32_t ratio) {
    /* Floor the period to 2 us so a 1 us period can still hold a 1 us on-slot. */
    if(period_us < 2) period_us = 2;

    /* Clamp the duty cycle to [0, 100]. */
    if(ratio > 100) ratio = 100;

    /* on_us = period * ratio / 100, with a 1 us minimum (never zero-width).
       64-bit intermediate avoids overflow: period_us * ratio can exceed
       UINT32_MAX when period_us is large (e.g. > 42.9 s with ratio 100). */
    uint32_t on_us = (uint64_t)period_us * ratio / 100;
    if(on_us < 1) on_us = 1;

    /* Clamp to the period (guards rounding on tiny periods). */
    if(on_us > period_us) on_us = period_us;

    return on_us;
}
