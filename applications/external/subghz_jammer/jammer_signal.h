#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <lib/toolbox/level_duration.h>

/**
 * @brief Jammer signal encoder.
 *
 * This is the heart of the jammer. It produces a never-ending stream of
 * LevelDuration values via jammer_signal_yield(). The async-TX DMA middleware
 * only considers transmission "done" when the callback returns
 * level_duration_is_reset() on an even level-parity. Because this encoder
 * NEVER returns reset, the DMA loop runs forever while the jammer is active,
 * producing continuous transmission.
 *
 * Three modes are supported:
 *  - CARRIER: a fixed-frequency square wave (unmodulated-ish carrier).
 *  - NOISE:   random levels with random durations (broadband noise).
 * Frequency hopping is handled by the radio layer (stop/restart at each hop);
 * this encoder only needs to be reset() between hops so its state is clean.
 */

typedef enum {
    JammerSignalModeCarrier, /**< Fixed-frequency square-wave carrier */
    JammerSignalModeNoise,   /**< Random OOK noise */
    JammerSignalModeSweep,   /**< Carrier sweeping across the band */
    JammerSignalModePingPong,/**< Carrier sweeping, reversing at the band ends */
    JammerSignalModePulse,   /**< OOK pulse train at a fixed duty cycle (protocol jamming) */
    JammerSignalModeCount,   /**< Sentinel: number of modes (for cycling) */
} JammerSignalMode;

typedef struct {
    JammerSignalMode mode;
    bool level; /**< Current carrier output level (carrier mode only) */
    uint32_t carrier_period_us; /**< Full period of the square wave (us) */
    uint32_t pulse_ratio;       /**< Duty cycle 0..100. Pulse mode only (Stage 2). */
    uint32_t noise_dur_min_us; /**< Shortest random noise duration (us) */
    uint32_t noise_dur_max_us; /**< Longest random noise duration (us) */
    uint32_t rng_state; /**< PRNG state for non-blocking noise generation in ISR */
} JammerSignal;

/**
 * EU-region (EN 300 220) TX gate decision.
 *
 * The Flipper firmware blocks transmission in the EU ISM band (433.05-434.79 MHz)
 * unless the user opts out. This is the pure decision behind
 * jammer_radio_pre_tx_check: it returns true when the EU gate should BLOCK
 * transmission at @p frequency.
 *
 * @param frequency       transmit frequency, Hz
 * @param bypass_region   bitmask; bit0 set => skip the EU-region gate
 * @return true if the EU-region gate blocks transmission at @p frequency
 */
bool jammer_eu_gate_blocks(uint32_t frequency, uint32_t bypass_region);

/** Allocate a JammerSignal with sane defaults. */
JammerSignal* jammer_signal_alloc(void);

/** Free a JammerSignal. */
void jammer_signal_free(JammerSignal* signal);

/**
 * Yield the next LevelDuration. Never returns a reset duration while the
 * jammer is active, so the async-TX loop never completes.
 * This is the version used by the host-side unit test harness.
 */
LevelDuration jammer_signal_yield(JammerSignal* signal);

/** Set the signal mode (carrier / noise). */
void jammer_signal_set_mode(JammerSignal* signal, JammerSignalMode mode);

/** Reset encoder state so a fresh hop/stream starts cleanly. */
void jammer_signal_reset(JammerSignal* signal);

/**
 * Set the carrier frequency (Hz). Implemented by adjusting the internal
 * square-wave period (carrier_period_us).
 */
void jammer_signal_set_frequency(JammerSignal* signal, uint32_t freq_hz);

/** Compute the hop frequency for hop_index within [band_min, band_max]. */
uint32_t jammer_signal_hop_frequency(
    uint32_t band_min,
    uint32_t band_max,
    uint32_t hop_index,
    uint32_t hop_count);
