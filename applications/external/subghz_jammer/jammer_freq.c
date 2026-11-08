#include "jammer_freq.h"
#include <stddef.h>

#define TAG "SubGhzJammerFreq"

uint32_t jammer_freq_khz_to_hz(uint32_t freq_khz) {
    /* 64-bit intermediate: freq_khz * 1000 overflows uint32 above ~4.29 GHz.
       The result is truncated to uint32_t on return, so callers still get the
       correct value for any sub-GHz band (CC1101 max ~928 MHz -> 928000000 Hz). */
    return (uint32_t)((uint64_t)freq_khz * 1000u);
}

uint32_t jammer_freq_hz_to_khz(uint32_t freq_hz) {
    return freq_hz / 1000u;
}

uint32_t jammer_freq_clamp_khz(uint32_t freq_khz, uint32_t min_khz, uint32_t max_khz) {
    if(freq_khz < min_khz) return min_khz;
    if(freq_khz > max_khz) return max_khz;
    return freq_khz;
}

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

uint32_t jammer_freq_step_target(
    uint32_t base,
    uint32_t step_hz,
    uint32_t n,
    uint32_t min,
    uint32_t max) {
    if(step_hz == 0 || max <= min) {
        return base;
    }
    /* 64-bit intermediate so base + n*step_hz cannot overflow 32-bit. */
    uint64_t target = (uint64_t)base + (uint64_t)n * (uint64_t)step_hz;
    if(target > max) {
        target = max;
    }
    if(target < min) {
        target = min;
    }
    return (uint32_t)target;
}

uint32_t jammer_freq_dwell_target(
    uint32_t dwell_min,
    uint32_t dwell_max,
    uint32_t dwell_step_hz,
    uint32_t dwell_index) {
    if(dwell_step_hz == 0 || dwell_max <= dwell_min) {
        return dwell_min;
    }
    uint64_t target = (uint64_t)dwell_min + (uint64_t)dwell_index * (uint64_t)dwell_step_hz;
    if(target > dwell_max) {
        target = dwell_max;
    }
    return (uint32_t)target;
}

uint32_t jammer_freq_noise_hop_target(
    uint32_t noise_min,
    uint32_t noise_max,
    uint32_t noise_step_hz,
    uint32_t hop_index) {
    if(noise_step_hz == 0 || noise_max <= noise_min) {
        return noise_min;
    }
    uint64_t target = (uint64_t)noise_min + (uint64_t)hop_index * (uint64_t)noise_step_hz;
    if(target > noise_max) {
        target = noise_max;
    }
    return (uint32_t)target;
}

uint32_t jammer_freq_hop_count_for_span(uint32_t min, uint32_t max, uint32_t step_hz) {
    if(step_hz == 0 || max <= min) {
        return 1;
    }
    /* ceil((max - min) / step_hz) + 1, 64-bit to avoid overflow. */
    uint64_t span = (uint64_t)max - (uint64_t)min;
    uint64_t count = span / (uint64_t)step_hz + 1;
    if((span % (uint64_t)step_hz) != 0) {
        count += 1;
    }
    return (uint32_t)count;
}

uint32_t jammer_freq_hop_queue_next(uint32_t cursor, const uint32_t* targets, uint32_t count) {
    if(count == 0 || targets == NULL) return 0;
    if(count == 1) return targets[0];
    return targets[cursor % count];
}

uint32_t jammer_freq_hop_pingpong(uint32_t step, uint32_t count) {
    if(count < 2) {
        return 0;
    }
    uint32_t period = 2u * (count - 1u);
    uint32_t pos = step % period;
    if(pos >= count) {
        pos = period - pos;
    }
    return pos;
}
