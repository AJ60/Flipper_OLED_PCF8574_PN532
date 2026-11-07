#pragma once

#include <stdint.h>
#include <string.h>
#include <lib/subghz/devices/devices.h>
#include "jammer_signal.h"

/** Radio selection for the jammer. */
typedef enum {
    JammerRadioAuto, /**< Let the loader pick internal or external CC1101 */
    JammerRadioInternal, /**< Force the internal CC1101 */
    JammerRadioExternalCC1101, /**< Force an external (e.g. +20dBm) CC1101 */
} JammerRadioChoice;

/**
 * Initialise the radio subsystem and select the radio.
 * Auto-routes RF / OTG and powers the CC1101 (internal or external) via the
 * radio device loader, exactly like the Sub-GHz CLI / bruteforcer.
 * @return true on success.
 */
bool jammer_radio_init(JammerRadioChoice choice);

/** Get the currently selected radio device (for TX control). */
const SubGhzDevice* jammer_radio_get_device(void);

/** Tear down the radio subsystem. */
void jammer_radio_deinit(void);

/**
 * Prepare the radio for transmission: frequency check, begin, reset,
 * load the async TX preset and set the starting frequency.
 * @return true on success.
 */
bool jammer_radio_begin(const SubGhzDevice* device, uint32_t frequency);

/**
 * Start continuous async transmission using the jammer signal encoder.
 * The custom yield callback never returns reset, so TX runs forever until
 * jammer_radio_stop() is called.
 * @return true on success.
 */
bool jammer_radio_start(const SubGhzDevice* device, JammerSignal* signal);

/** Stop transmission and release the radio. */
void jammer_radio_stop(const SubGhzDevice* device);

/**
 * Commit a frequency hop: stop the async TX, reconfigure the radio device at
 * the new frequency, and restart. This is the clean way to hop — unlike
 * jammer_radio_set_frequency (which only rewrites the frequency register
 * without tearing down the DMA stream). Safe to call from the worker thread.
 *
 * @param device   currently selected radio device
 * @param signal   jammer signal (carries the encoder)
 * @param frequency target frequency (Hz)
 * @return true on success
 */
bool jammer_radio_commit_hop(const SubGhzDevice* device, JammerSignal* signal, uint32_t frequency);

/**
 * Set the TX power from the user's 0..100 power_level.
 *
 * power_level == 0 => auto: pick the highest level the radio supports
 * (internal: 31, external: 30). Otherwise map 1..100 linearly onto the radio's
 * supported level range.
 *
 * @param device      currently selected radio device
 * @param power_level 0..100 (0 = auto)
 */
void jammer_radio_set_tx_power(const SubGhzDevice* device, uint32_t power_level);

/**
 * Validate a TX frequency against the band and (optionally) the EU-region
 * gate before transmission.
 *
 * @param device         currently selected radio device
 * @param frequency      frequency to check (Hz)
 * @param bypass_region  bitmask; bit0 skips the EU-region gate
 * @return true if the frequency is allowed
 */
bool jammer_radio_pre_tx_check(
    const SubGhzDevice* device,
    uint32_t frequency,
    uint32_t bypass_region);

/** Check whether async TX is currently running. */
bool jammer_radio_is_tx_running(void);
