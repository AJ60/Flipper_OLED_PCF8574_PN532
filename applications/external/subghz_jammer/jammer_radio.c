#include "jammer_radio.h"

#include <furi.h>
#include <furi_hal_subghz.h>
#include <furi_hal_power.h>
#include <lib/subghz/subghz_radio_device_loader.h>

#include "jammer_signal.h"

#define TAG "SubGhzJammerRadio"

#ifndef SUBGHZ_DEVICE_CC1101_INT_NAME
#define SUBGHZ_DEVICE_CC1101_INT_NAME "cc1101_int"
#endif

static const SubGhzDevice* s_device = NULL;
static bool s_tx_running = false;

/**
 * The async-TX DMA middleware calls this callback to refill the transmission
 * buffer. It forwards to the jammer signal encoder, whose yield() never
 * returns reset, so the DMA loop never completes while jamming.
 */
static LevelDuration jammer_radio_tx_callback(void* context) {
    JammerSignal* signal = (JammerSignal*)context;
    return jammer_signal_yield(signal);
}

bool jammer_radio_init(JammerRadioChoice choice) {
    subghz_devices_init();

    SubGhzRadioDeviceType type = SubGhzRadioDeviceTypeAuto;
    if(choice == JammerRadioInternal) {
        type = SubGhzRadioDeviceTypeInternal;
    } else if(choice == JammerRadioExternalCC1101) {
        type = SubGhzRadioDeviceTypeExternalCC1101;
    }

    s_device = radio_device_loader_set(NULL, type);
    if(s_device == NULL) {
        // Fallback to internal CC1101 if external was selected or not detected
        s_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        if(s_device != NULL) {
            subghz_devices_begin(s_device);
        }
    }

    if(s_device == NULL) {
        FURI_LOG_E(TAG, "No Sub-GHz radio device available");
        subghz_devices_deinit();
        return false;
    }

    subghz_devices_reset(s_device);
    subghz_devices_idle(s_device);
    s_tx_running = false;

    FURI_LOG_I(TAG, "radio selected: %s", subghz_devices_get_name(s_device));
    return true;
}

const SubGhzDevice* jammer_radio_get_device(void) {
    return s_device;
}

void jammer_radio_deinit(void) {
    if(s_device != NULL) {
        if(s_tx_running) {
            subghz_devices_stop_async_tx(s_device);
            s_tx_running = false;
            furi_hal_power_suppress_charge_exit();
        }
        subghz_devices_idle(s_device);
        subghz_devices_sleep(s_device);
        radio_device_loader_end(s_device);
        s_device = NULL;
    }
    subghz_devices_deinit();
}

bool jammer_radio_begin(const SubGhzDevice* device, uint32_t frequency) {
    if(device == NULL) return false;
    if(!subghz_devices_is_frequency_valid(device, frequency)) {
        FURI_LOG_E(TAG, "frequency invalid: %lu", frequency);
        return false;
    }

    subghz_devices_idle(device);
    subghz_devices_reset(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(device, frequency);
    return true;
}

bool jammer_radio_start(const SubGhzDevice* device, JammerSignal* signal) {
    if(device == NULL || signal == NULL) return false;
    if(s_tx_running) return true;

    furi_hal_power_suppress_charge_enter();
    bool ret = subghz_devices_start_async_tx(device, jammer_radio_tx_callback, signal);
    if(ret) {
        s_tx_running = true;
    } else {
        FURI_LOG_E(TAG, "subghz_devices_start_async_tx failed (regulation or duty cycle)");
        furi_hal_power_suppress_charge_exit();
    }
    return ret;
}

void jammer_radio_stop(const SubGhzDevice* device) {
    if(device == NULL) return;
    if(s_tx_running) {
        subghz_devices_stop_async_tx(device);
        s_tx_running = false;
        furi_hal_power_suppress_charge_exit();
    }
    subghz_devices_idle(device);
}

void jammer_radio_set_frequency(const SubGhzDevice* device, uint32_t frequency) {
    if(device == NULL) return;
    if(subghz_devices_is_frequency_valid(device, frequency)) {
        subghz_devices_set_frequency(device, frequency);
        if(s_tx_running) {
            subghz_devices_set_tx(device);
        }
    }
}

bool jammer_radio_is_tx_running(void) {
    return s_tx_running;
}
