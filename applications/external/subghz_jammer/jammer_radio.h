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
 * Change the transmission frequency. Safe to call while running for
 * frequency hopping.
 */
void jammer_radio_set_frequency(const SubGhzDevice* device, uint32_t frequency);

/** Check whether async TX is currently running. */
bool jammer_radio_is_tx_running(void);
