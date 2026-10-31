#pragma once

#include <stdint.h>

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
