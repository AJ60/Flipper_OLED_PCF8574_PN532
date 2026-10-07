#include "subghz_radio_device_loader.h"

#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/subghz_radio_device_global.h>

static void radio_device_loader_power_on(void) {
    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        //CC1101 power-up time
        furi_delay_ms(10);
    }
}

static void radio_device_loader_power_off(void) {
    if(furi_hal_power_is_otg_enabled()) furi_hal_power_disable_otg();
}

static bool radio_device_loader_is_connect_external(const char* name) {
    bool is_connect = false;
    bool is_otg_enabled = furi_hal_power_is_otg_enabled();

    if(!is_otg_enabled) {
        radio_device_loader_power_on();
    }

    const SubGhzDevice* device = subghz_devices_get_by_name(name);
    if(device) {
        is_connect = subghz_devices_is_connect(device);
    }

    if(!is_otg_enabled) {
        radio_device_loader_power_off();
    }
    return is_connect;
}

const SubGhzDevice* radio_device_loader_set(
    const SubGhzDevice* current_radio_device,
    SubGhzRadioDeviceType radio_device_type) {
    const SubGhzDevice* radio_device = NULL;
    (void)radio_device_type;

    // Global selection persisted by main SubGHz Radio Settings
    // (last_subghz.settings RadioDevice: 1=Internal, 2=External).
    // External selected => use EXT or fail loudly (NULL, no silent fallback).
    // Internal selected => use INT without OTG probe.
    uint32_t global = subghz_global_radio_device_load();
    bool want_external = (global == SUBGHZ_GLOBAL_RADIO_DEVICE_EXTERNAL);

    if(want_external) {
        radio_device_loader_power_on();
        if(radio_device_loader_is_connect_external(SUBGHZ_DEVICE_CC1101_EXT_NAME)) {
            radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
            if(radio_device) {
                subghz_devices_begin(radio_device);
                FURI_LOG_I(
                    "RadioDeviceLoader",
                    "External CC1101 detected and initialized (global selection)");
                return radio_device;
            }
        }
        FURI_LOG_E(
            "RadioDeviceLoader",
            "External selected but CC1101 not detected; refusing internal fallback");
        radio_device_loader_power_off();
        return NULL;
    }

    if(current_radio_device != NULL &&
       current_radio_device == subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME)) {
        radio_device_loader_end(current_radio_device);
    }
    radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(radio_device) {
        subghz_devices_begin(radio_device);
        FURI_LOG_I("RadioDeviceLoader", "Internal CC1101 initialized (global selection)");
    }

    return radio_device;
}

bool radio_device_loader_is_external(const SubGhzDevice* radio_device) {
    furi_assert(radio_device);
    return (radio_device != subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME));
}

void radio_device_loader_end(const SubGhzDevice* radio_device) {
    furi_assert(radio_device);
    radio_device_loader_power_off();
    // Code below is not used (and will cause crash) since its called from tx_rx worker end!
    //if(radio_device != subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME)) {
    //    subghz_devices_end(radio_device);
    //}
}
