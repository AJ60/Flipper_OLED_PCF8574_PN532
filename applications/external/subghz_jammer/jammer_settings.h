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
    JammerSignalMode mode; /**< Carrier or noise */
    uint32_t frequency; /**< Operating frequency (Hz) */
    uint32_t band_min; /**< Lowest allowed frequency for this band (Hz) */
    uint32_t band_max; /**< Highest allowed frequency for this band (Hz) */
    uint16_t hop_count; /**< Number of hop targets (hop mode) */
    uint16_t dwell_ms; /**< Time per hop / per transmission (ms) */
    JammerRadioChoice radio; /**< Radio selection */
    bool latch; /**< true = latch-on trigger, false = hold-to-jam */
} JammerSettings;

/** Load settings from storage; falls back to defaults when unavailable. */
void jammer_settings_load(JammerSettings* settings);

/** Persist settings to storage. */
void jammer_settings_save(const JammerSettings* settings);

/** Populate defaults (carrier, 433.92MHz, internal radio, latch-on). */
void jammer_settings_defaults(JammerSettings* settings);
