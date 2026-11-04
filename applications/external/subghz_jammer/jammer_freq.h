#pragma once

#include <stdint.h>

#define JAMMER_FREQ_MIN_KHZ 300000u  /* 300 MHz - CC1101 low edge */
#define JAMMER_FREQ_MAX_KHZ 928000u  /* 928 MHz - CC1101 high edge */

/* kHz <-> Hz conversions and range clamp for arbitrary-frequency entry. */
uint32_t jammer_freq_khz_to_hz(uint32_t freq_khz);
uint32_t jammer_freq_hz_to_khz(uint32_t freq_hz);
uint32_t jammer_freq_clamp_khz(uint32_t freq_khz, uint32_t min_khz, uint32_t max_khz);

/**
 * Pure frequency-scheduling math for the Sub-Ghz jammer.
 *
 * These functions are deliberately free of any Furi/Flipper dependency so they
 * can be compiled and unit-tested on a plain host toolchain (gcc/clang) before
 * they ever run on the device. Every function here is deterministic and integer
 * only.
 */

/**
 * Return the frequency for a linear sweep.
 *
 * The band [min, max] is divided into `points` equal segments; point 0 is the
 * band start, point (points-1) is the band end. Indices wrap around, so a
 * caller can feed a monotonically increasing counter forever.
 *
 * @param min    band lower edge (Hz)
 * @param max    band upper edge (Hz)
 * @param point  current sweep index (>= 0), wraps modulo `points`
 * @param points number of sweep steps in the band (>= 1)
 * @return frequency in Hz for `point`
 */
uint32_t jammer_freq_sweep_point(uint32_t min, uint32_t max, uint32_t point, uint32_t points);

/**
 * Return the sweep position for a ping-pong (triangle) sweep.
 *
 * The position bounces 0,1,...,points-1,points-2,...,1,0,1,... with period
 * 2*(points-1). Useful as an index into a linear band so the sweep reverses
 * direction instead of restarting.
 *
 * @param step   monotonically increasing step counter (>= 0)
 * @param points number of band steps (>= 2)
 * @return position in [0, points-1]
 */
uint32_t jammer_freq_pingpong(uint32_t step, uint32_t points);

/**
 * Return the OOK "on" time (us) for a pulse train.
 *
 * on = period_us * ratio / 100, clamped to [1, period_us]. A duty ratio above
 * 100 is clamped to 100 (full period); a ratio of 0 yields the 1 us minimum so
 * the on-slot is never zero-width. The period is floored to 2 us so a 1 us
 * period can still hold a 1 us on-slot.
 *
 * @param period_us full pulse-train period in microseconds (>= 1)
 * @param ratio     duty cycle in percent (0-100; >100 clamps to 100)
 * @return on-time in microseconds in [1, period_us]
 */
uint32_t jammer_freq_pulse_on_us(uint32_t period_us, uint32_t ratio);

/**
 * Return the frequency for a linear step schedule.
 *
 * target = clamp(base + n * step_hz, base, max). Used for dense sweep/brute
 * and scan schedules where the caller controls the step size in Hz.
 *
 * Guards: step_hz == 0 -> base; max <= min -> base.
 *
 * @param base   starting frequency (Hz)
 * @param step_hz step size in Hz (>= 0)
 * @param n      step index (>= 0)
 * @param min    band lower edge (Hz)
 * @param max    band upper edge (Hz)
 * @return frequency in Hz in [base, max]
 */
uint32_t jammer_freq_step_target(
    uint32_t base,
    uint32_t step_hz,
    uint32_t n,
    uint32_t min,
    uint32_t max);

/**
 * Return the dwell frequency for a dwell-cycle schedule.
 *
 * Sweeps [dwell_min, dwell_max] in dwell_step_hz increments, wrapping the index
 * so a caller can feed a monotonically increasing counter forever.
 *
 * Guards: dwell_step_hz == 0 -> dwell_min; dwell_max <= dwell_min -> dwell_min.
 *
 * @param dwell_min      dwell lower edge (Hz)
 * @param dwell_max      dwell upper edge (Hz)
 * @param dwell_step_hz  step size in Hz (>= 0)
 * @param dwell_index    current dwell index (>= 0), wraps
 * @return frequency in Hz in [dwell_min, dwell_max]
 */
uint32_t jammer_freq_dwell_target(
    uint32_t dwell_min,
    uint32_t dwell_max,
    uint32_t dwell_step_hz,
    uint32_t dwell_index);

/**
 * Return the hop frequency for a noise-burst hop schedule.
 *
 * Sweeps [noise_min, noise_max] in noise_step_hz increments, wrapping the index.
 *
 * Guards: noise_step_hz == 0 -> noise_min; noise_max <= noise_min -> noise_min.
 *
 * @param noise_min      noise lower edge (Hz)
 * @param noise_max      noise upper edge (Hz)
 * @param noise_step_hz  step size in Hz (>= 0)
 * @param hop_index      current hop index (>= 0), wraps
 * @return frequency in Hz in [noise_min, noise_max]
 */
uint32_t jammer_freq_noise_hop_target(
    uint32_t noise_min,
    uint32_t noise_max,
    uint32_t noise_step_hz,
    uint32_t hop_index);

/**
 * Return the number of hop targets needed to span [min, max] at step_hz.
 *
 * count = ceil((max - min) / step_hz) + 1. Guarantees the last hop lands
 * exactly on `max`.
 *
 * Guards: step_hz == 0 -> 1; max <= min -> 1.
 *
 * @param min     band lower edge (Hz)
 * @param max     band upper edge (Hz)
 * @param step_hz step size in Hz (>= 0)
 * @return number of hop targets (>= 1)
 */
uint32_t jammer_freq_hop_count_for_span(uint32_t min, uint32_t max, uint32_t step_hz);

/**
 * Return the hop target index for a ping-pong (triangle) hop schedule.
 *
 * Bounces 0,1,...,count-1,0,1,... so the hop direction reverses at the band
 * ends instead of restarting.
 *
 * @param step   monotonically increasing step counter (>= 0)
 * @param count  number of hop targets (>= 2)
 * @return position in [0, count-1]
 */
uint32_t jammer_freq_hop_pingpong(uint32_t step, uint32_t count);
