#include "jammer_radio.h"

#include <furi.h>
#include <furi_hal_subghz.h>
#include <furi_hal_power.h>
#include <lib/subghz/subghz_radio_device_loader.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>

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

    // Auto => defer to the persisted SubGHz global radio selection (loader).
    // This keeps the jammer compatible with the SubGHz app's radio setting.
    if(choice == JammerRadioAuto) {
        s_device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeAuto);
        if(s_device == NULL) {
            // External selected globally but not connected: fall back to internal.
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
        FURI_LOG_I(TAG, "radio selected (auto): %s", subghz_devices_get_name(s_device));
        return true;
    }

    // Explicit choice => select the requested radio directly so the jammer's
    // menu honors the user's selection instead of the global setting.
    bool want_external = (choice == JammerRadioExternalCC1101);
    const char* radio_name = SUBGHZ_DEVICE_CC1101_INT_NAME;
    bool keep_otg = false;

    if(want_external) {
        // Probe the external CC1101 with OTG on, then off (loader pattern).
        bool otg_was_enabled = furi_hal_power_is_otg_enabled();
        if(!otg_was_enabled) {
            furi_hal_power_enable_otg();
            furi_delay_ms(10);
        }
        const SubGhzDevice* ext = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
        bool ext_connected = ext && subghz_devices_is_connect(ext);
        if(!otg_was_enabled) {
            furi_hal_power_disable_otg();
        }
        if(ext_connected) {
            radio_name = SUBGHZ_DEVICE_CC1101_EXT_NAME;
            keep_otg = true;
        } else {
            FURI_LOG_W(TAG, "External CC1101 not connected, using internal");
        }
    }

    if(keep_otg && !furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }

    s_device = subghz_devices_get_by_name(radio_name);
    if(s_device == NULL) {
        if(keep_otg && furi_hal_power_is_otg_enabled()) {
            furi_hal_power_disable_otg();
        }
        subghz_devices_deinit();
        return false;
    }

    subghz_devices_begin(s_device);
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

static void jammer_radio_apply_tx_power(const SubGhzDevice* device);

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
    // Apply the PA table after the preset + frequency change so the TX power
    // matches the user's power_level (the preset only sets its own default entry).
    jammer_radio_apply_tx_power(device);
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

bool jammer_radio_commit_hop(const SubGhzDevice* device, JammerSignal* signal, uint32_t frequency) {
    // Clean hop: tear down the DMA stream, reconfigure, and restart so the new
    // frequency is fully locked before transmission resumes. An in-place
    // frequency-only rewrite would leave the stale DMA stream running.
    if(device == NULL) return false;
    if(!subghz_devices_is_frequency_valid(device, frequency)) {
        FURI_LOG_E(TAG, "hop frequency invalid: %lu", frequency);
        return false;
    }
    if(s_tx_running) {
        subghz_devices_stop_async_tx(device);
        s_tx_running = false;
        furi_hal_power_suppress_charge_exit();
    }
    if(!jammer_radio_begin(device, frequency)) {
        FURI_LOG_E(TAG, "hop begin failed: %lu", frequency);
        return false;
    }
    if(signal != NULL) {
        jammer_radio_start(device, signal);
    }
    return true;
}

static uint32_t s_tx_power_level = 0; // last requested power level (0..100); 0 = auto

// Map the UI power level (0..100) to genuine CC1101 PATABLE dBm values.
// In OOK async mode (FuriHalSubGhzPresetOok650Async, FREND0 PA_POWER = 1):
// - PATABLE[0] MUST be 0x00 so carrier turns completely off for logic '0'.
// - PATABLE[1] sets carrier power for logic '1':
//   0 (Auto) / 100% -> 0xC0 (+12 dBm)
//   75%             -> 0xCD (+7 dBm)
//   50%             -> 0x50 (0 dBm)
//   25%             -> 0x26 (-10 dBm)
//   10%             -> 0x17 (-20 dBm)
static uint8_t jammer_radio_power_to_patable(uint32_t power_level) {
    if(power_level == 0 || power_level >= 100) {
        return 0xC0; // +12 dBm (Auto / 100%)
    } else if(power_level >= 75) {
        return 0xCD; // +7 dBm (75%)
    } else if(power_level >= 50) {
        return 0x50; // 0 dBm (50%)
    } else if(power_level >= 25) {
        return 0x26; // -10 dBm (25%)
    } else {
        return 0x17; // -20 dBm (10%)
    }
}

// Write the CC1101 PA_TABLE directly (bypassing the opaque SubGhzDevice) so the
// TX power matches the user's power_level.
static void jammer_radio_apply_tx_power(const SubGhzDevice* device) {
    if(device == NULL) {
        return;
    }
    uint8_t pa_val = jammer_radio_power_to_patable(s_tx_power_level);
    uint8_t pa[8] = {0x00, pa_val, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int external = radio_device_loader_is_external(device);

    // Route through the device interconnect so both the internal CC1101 and the
    // external cc1101_ext plugin apply the PA table on their own bus.
    subghz_devices_load_patable(device, pa);
    FURI_LOG_D(
        TAG,
        "tx power applied: %lu%% -> PATABLE[1]=0x%02X (external=%d)",
        (unsigned long)s_tx_power_level,
        pa_val,
        external);
}

void jammer_radio_set_tx_power(const SubGhzDevice* device, uint32_t power_level) {
    if(device == NULL) {
        return;
    }
    s_tx_power_level = power_level;
    jammer_radio_apply_tx_power(device);
    FURI_LOG_D(
        TAG,
        "tx power requested: %lu%% -> 0x%02X",
        (unsigned long)power_level,
        jammer_radio_power_to_patable(power_level));
}

bool jammer_radio_pre_tx_check(
    const SubGhzDevice* device,
    uint32_t frequency,
    uint32_t bypass_region) {
    // Validate the frequency against the device's band.
    if(device == NULL) return false;
    if(!subghz_devices_is_frequency_valid(device, frequency)) {
        FURI_LOG_E(TAG, "pre-tx frequency invalid: %lu", frequency);
        return false;
    }
    // EU-region gate (EN 300 220): 433.05..434.79 MHz limited to 100 mEIRP.
    // Require the gate unless the user opted to bypass it.
    if(jammer_eu_gate_blocks(frequency, bypass_region)) {
        FURI_LOG_W(TAG, "EU-region tx gated: %lu (set bypass to override)", frequency);
        return false;
    }
    return true;
}

bool jammer_radio_is_tx_running(void) {
    return s_tx_running;
}
