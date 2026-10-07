#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Canonical persisted values. Must match applications/main/subghz/helpers/subghz_types.h:
 * 0=Auto (treated as Internal), 1=Internal, 2=ExternalCC1101. */
#define SUBGHZ_GLOBAL_RADIO_DEVICE_INTERNAL 1u
#define SUBGHZ_GLOBAL_RADIO_DEVICE_EXTERNAL 2u

/** Load persisted global selection from last_subghz.settings.
 * Returns SUBGHZ_GLOBAL_RADIO_DEVICE_INTERNAL (1) or EXTERNAL (2).
 * Missing/corrupt file => Internal. Never returns 0. */
uint32_t subghz_global_radio_device_load(void);

/** True if the user selected External globally. */
static inline bool subghz_global_radio_device_is_external(void) {
    return subghz_global_radio_device_load() == SUBGHZ_GLOBAL_RADIO_DEVICE_EXTERNAL;
}

#ifdef __cplusplus
}
#endif
