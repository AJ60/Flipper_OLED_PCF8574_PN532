#pragma once

#include <lib/subghz/devices/devices.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SubGhzRadioDeviceType - canonical values match main subghz helpers
 * (Auto=0, Internal=1, ExternalCC1101=2) so the persisted global selection
 * maps 1:1 without numeric confusion. */
typedef enum {
    SubGhzRadioDeviceTypeAuto,
    SubGhzRadioDeviceTypeInternal,
    SubGhzRadioDeviceTypeExternalCC1101,
} SubGhzRadioDeviceType;

/** Select radio per the persisted global selection (last_subghz.settings).
 * External selected => returns EXT or NULL (no silent internal fallback).
 * Internal selected => returns INT without probing OTG. */
const SubGhzDevice* radio_device_loader_set(
    const SubGhzDevice* current_radio_device,
    SubGhzRadioDeviceType radio_device_type);

void radio_device_loader_end(const SubGhzDevice* radio_device);

bool radio_device_loader_is_external(const SubGhzDevice* radio_device);

#ifdef __cplusplus
}
#endif
