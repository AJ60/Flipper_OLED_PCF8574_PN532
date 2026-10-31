#include "jammer_freq.h"

#define TAG "SubGhzJammerFreq"

uint32_t jammer_freq_sweep_point(uint32_t min, uint32_t max, uint32_t point, uint32_t points) {
    if(points < 1) return min;
    if(max <= min) return min;

    /* Wrap the (unbounded) counter into [0, points). */
    uint32_t idx = point % points;
    /* Equal spacing so point 0 = min and point (points-1) = max. */
    uint32_t step = (max - min) / points;
    return min + step * idx;
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

    /* on_us = period * ratio / 100, with a 1 us minimum (never zero-width). */
    uint32_t on_us = period_us * ratio / 100;
    if(on_us < 1) on_us = 1;

    /* Clamp to the period (guards rounding on tiny periods). */
    if(on_us > period_us) on_us = period_us;

    return on_us;
}
