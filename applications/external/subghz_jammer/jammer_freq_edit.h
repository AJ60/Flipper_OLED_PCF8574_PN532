#pragma once

#include <stdint.h>

typedef struct Jammer Jammer;

/**
 * kHz <-> Hz conversions and band clamp for arbitrary-frequency entry.
 *
 * Pure math with no Furi dependency, host-tested (see
 * tests/test_jammer_freq_edit.c). 1 kHz resolution across the full
 * 300-928 MHz range.
 */
uint32_t freq_khz_to_hz(uint32_t freq_khz);
uint32_t freq_hz_to_khz(uint32_t freq_hz);
uint32_t freq_khz_clamp(uint32_t freq_khz);

/**
 * Frequency editor backed by NumberInput (native numeric keypad).
 *
 * The value is stored in Hz (JammerSettings.frequency) and edited with 1 kHz
 * resolution across the full 300-928 MHz range. OK commits and returns to the
 * menu; Back cancels without changing the value.
 */
void jammer_freq_edit_enter(Jammer* jammer);
