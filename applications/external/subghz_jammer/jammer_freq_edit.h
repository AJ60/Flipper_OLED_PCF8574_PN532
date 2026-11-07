#pragma once

#include <stddef.h>
#include <stdint.h>

/* Full CC1101 sub-GHz range, in Hz. */
#define FREQ_MIN_HZ (300000000u)   /* 300 MHz */
#define FREQ_MAX_HZ (928000000u)   /* 928 MHz */

/* Selector step. 1 MHz matches the "300,301,302..." examples.
 * For CC1101-native 125 kHz resolution change to 125000u (see Open Questions). */
#define FREQ_STEP_HZ (1000000u)

/* Number of selectable steps, inclusive of both ends (629 at 1 MHz). */
#define FREQ_STEP_COUNT \
    (((FREQ_MAX_HZ - FREQ_MIN_HZ) / FREQ_STEP_HZ) + 1u)

/* Map a user-entered frequency (Hz) to a selector index, clamping into range. */
uint16_t freq_hz_to_index(uint32_t freq_hz);

/* Map a selector index back to a frequency (Hz). */
uint32_t freq_index_to_hz(uint16_t index);

/* Format a frequency (Hz) as "XXX.XX MHz" into buf; returns buf. */
const char* freq_hz_to_text(uint32_t freq_hz, char* buf, size_t buf_size);

/**
 * Given a selectable index and a signed step delta (+1/-1), return the clamped
 * next index in [0, FREQ_STEP_COUNT-1]. Returns -1 if index is out of range or
 * delta is not +1/-1.
 */
int32_t freq_edit_step_index(int32_t index, int32_t delta);
