#include "tpms_app_i.h"

#define TAG "TPMS"
#include <flipper_format/flipper_format_i.h>

void tpms_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size) {
    furi_assert(context);
    TPMSApp* app = context;
    furi_string_set(app->txrx->preset->name, preset_name);
    app->txrx->preset->frequency = frequency;
    app->txrx->preset->data = preset_data;
    app->txrx->preset->data_size = preset_data_size;
}

bool tpms_set_preset(TPMSApp* app, const char* preset) {
    if(!strcmp(preset, "FuriHalSubGhzPresetOok270Async")) {
        furi_string_set(app->txrx->preset->name, "AM270");
    } else if(!strcmp(preset, "FuriHalSubGhzPresetOok650Async")) {
        furi_string_set(app->txrx->preset->name, "AM650");
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev238Async")) {
        furi_string_set(app->txrx->preset->name, "FM238");
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev476Async")) {
        furi_string_set(app->txrx->preset->name, "FM476");
    } else if(!strcmp(preset, "FuriHalSubGhzPresetCustom")) {
        furi_string_set(app->txrx->preset->name, "CUSTOM");
    } else {
        FURI_LOG_E(TAG, "Unknown preset");
        return false;
    }
    return true;
}

void tpms_get_frequency_modulation(TPMSApp* app, FuriString* frequency, FuriString* modulation) {
    furi_assert(app);
    if(frequency != NULL) {
        furi_string_printf(
            frequency,
            "%03ld.%02ld",
            app->txrx->preset->frequency / 1000000 % 1000,
            app->txrx->preset->frequency / 10000 % 100);
    }
    if(modulation != NULL) {
        furi_string_printf(modulation, "%.2s", furi_string_get_cstr(app->txrx->preset->name));
    }
}

void tpms_begin(TPMSApp* app, uint8_t* preset_data) {
    furi_assert(app);
    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);
    subghz_devices_load_preset(app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

uint32_t tpms_rx(TPMSApp* app, uint32_t frequency) {
    furi_assert(app);
    if(!subghz_devices_is_frequency_valid(app->txrx->radio_device, frequency)) {
        furi_crash("TPMS: Incorrect RX frequency.");
    }
    furi_assert(
        app->txrx->txrx_state != TPMSTxRxStateRx && app->txrx->txrx_state != TPMSTxRxStateSleep);

    subghz_devices_idle(app->txrx->radio_device);
    uint32_t value = subghz_devices_set_frequency(app->txrx->radio_device, frequency);
    subghz_devices_flush_rx(app->txrx->radio_device);
    subghz_devices_set_rx(app->txrx->radio_device);

    subghz_devices_start_async_rx(
        app->txrx->radio_device, subghz_worker_rx_callback, app->txrx->worker);
    subghz_worker_start(app->txrx->worker);
    app->txrx->txrx_state = TPMSTxRxStateRx;
    return value;
}

void tpms_idle(TPMSApp* app) {
    furi_assert(app);
    furi_assert(app->txrx->txrx_state != TPMSTxRxStateSleep);
    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

void tpms_rx_end(TPMSApp* app) {
    furi_assert(app);
    furi_assert(app->txrx->txrx_state == TPMSTxRxStateRx);
    if(subghz_worker_is_running(app->txrx->worker)) {
        subghz_worker_stop(app->txrx->worker);
        subghz_devices_stop_async_rx(app->txrx->radio_device);
    }
    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

void tpms_sleep(TPMSApp* app) {
    furi_assert(app);
    subghz_devices_sleep(app->txrx->radio_device);
    app->txrx->txrx_state = TPMSTxRxStateSleep;
}

void tpms_hopper_update(TPMSApp* app) {
    furi_assert(app);

    switch(app->txrx->hopper_state) {
    case TPMSHopperStateOFF:
    case TPMSHopperStatePause:
        return;
    case TPMSHopperStateRSSITimeOut:
        if(app->txrx->hopper_timeout != 0) {
            app->txrx->hopper_timeout--;
            return;
        }
        break;
    default:
        break;
    }
    float rssi = -127.0f;
    if(app->txrx->hopper_state != TPMSHopperStateRSSITimeOut) {
        // See RSSI Calculation timings in CC1101 17.3 RSSI
        rssi = subghz_devices_get_rssi(app->txrx->radio_device);

        // Stay if RSSI is high enough
        if(rssi > -90.0f) {
            app->txrx->hopper_timeout = 10;
            app->txrx->hopper_state = TPMSHopperStateRSSITimeOut;
            return;
        }
    } else {
        app->txrx->hopper_state = TPMSHopperStateRunnig;
    }
    // Select next frequency
    if(app->txrx->hopper_idx_frequency <
       subghz_setting_get_hopper_frequency_count(app->setting) - 1) {
        app->txrx->hopper_idx_frequency++;
    } else {
        app->txrx->hopper_idx_frequency = 0;
    }

    if(app->txrx->txrx_state == TPMSTxRxStateRx) {
        tpms_rx_end(app);
    };
    if(app->txrx->txrx_state == TPMSTxRxStateIDLE) {
        subghz_receiver_reset(app->txrx->receiver);
        app->txrx->preset->frequency =
            subghz_setting_get_hopper_frequency(app->setting, app->txrx->hopper_idx_frequency);
        tpms_rx(app, app->txrx->preset->frequency);
    }
}
