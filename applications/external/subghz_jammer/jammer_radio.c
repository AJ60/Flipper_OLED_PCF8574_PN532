#include "jammer_radio.h"

#include <furi.h>
#include <furi_hal_subghz.h>
#include <furi_hal_power.h>
#include <lib/subghz/subghz_radio_device_loader.h>

#include "jammer_signal.h"

#define TAG "SubGhzJammerRadio"

static const SubGhzDevice* s_device = NULL;

/**
 * The async-TX DMA middleware calls this callback to refill the transmission
 * buffer. It simply forwards to the jammer signal encoder, whose yield() never
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
        FURI_LOG_E(TAG, "radio_device_loader_set failed");
        subghz_devices_deinit();
        return false;
    }
    FURI_LOG_D(TAG, "radio selected");
    return true;
}

const SubGhzDevice* jammer_radio_get_device(void) {
    return s_device;
}

void jammer_radio_deinit(void) {
    if(s_device != NULL) {
        radio_device_loader_end(s_device);
        s_device = NULL;
    }
    subghz_devices_deinit();
}

bool jammer_radio_begin(const SubGhzDevice* device, uint32_t frequency) {
    if(!subghz_devices_is_frequency_valid(device, frequency)) {
        FURI_LOG_E(TAG, "frequency invalid: %lu", frequency);
        return false;
    }

    subghz_devices_begin(device);
    subghz_devices_reset(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(device, frequency);
    return true;
}

bool jammer_radio_start(const SubGhzDevice* device, JammerSignal* signal) {
    /* Suppress charging so the CC1101 can draw the current it needs for full
     * power while the USB charger is attached. */
    furi_hal_power_suppress_charge_enter();
    return subghz_devices_start_async_tx(device, jammer_radio_tx_callback, signal);
}

void jammer_radio_stop(const SubGhzDevice* device) {
    subghz_devices_stop_async_tx(device);
    subghz_devices_sleep(device);
    subghz_devices_end(device);
    furi_hal_power_suppress_charge_exit();
}

void jammer_radio_set_frequency(const SubGhzDevice* device, uint32_t frequency) {
    subghz_devices_set_frequency(device, frequency);
}
