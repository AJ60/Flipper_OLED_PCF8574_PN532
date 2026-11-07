#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "jammer_signal.h"
#include "jammer_radio.h"

/**
 * Persistent settings for the jammer.
 * Persisted to APP_DATA_PATH on every change so the next run resumes where
 * the user left off.
 */
typedef struct {
    JammerSignalMode mode; /**< Carrier, noise, sweep, ping-pong, carrier, or pulse */
    uint32_t pulse_ratio; /**< Duty cycle 0..100 (percent). Pulse mode only. */
    uint32_t frequency; /**< Operating frequency (Hz) */
    uint32_t band_min; /**< Lowest allowed frequency for this band (Hz) */
    uint32_t band_max; /**< Highest allowed frequency for this band (Hz) */
    uint16_t hop_count; /**< Number of hop targets (hop mode) */
    uint16_t dwell_ms; /**< Time per hop / per transmission (ms) */
    JammerRadioChoice radio; /**< Radio selection */
    bool latch; /**< true = latch-on trigger, false = hold-to-jam */
    /* --- Advanced scheduling (Stage 3) --- */
    uint32_t hop_interval_ms; /**< Dwell per hop / time between hops (ms) */
    uint32_t dwell_hold_ms; /**< Gap between hops in DwellCycle mode (ms) */
    uint32_t sweep_step_hz; /**< Sweep/brute step size in Hz */
    uint32_t noise_step_hz; /**< Noise-burst step size in Hz */
    uint32_t power_level; /**< TX power 0..100 (0 = auto, mapped per radio) */
    uint8_t bypass_region; /**< Bitmask (0..255): bit0 = skip EU-region gate */
    uint32_t max_tx_duration_ms; /**< TX watchdog timeout (ms) */
    uint8_t tx_gate_mode; /**< 0 = continuous, 1 = gate-on-input, 2 = pulse-gate */
} JammerSettings;

/** Load settings from storage; falls back to defaults when unavailable. */
void jammer_settings_load(JammerSettings* settings);

/** Persist settings to storage. */
void jammer_settings_save(const JammerSettings* settings);

/** Populate defaults (carrier, 433.92MHz, internal radio, latch-on). */
void jammer_settings_defaults(JammerSettings* settings);
