#pragma once

#include <gui/view.h>
#include "../ham_radio_app_i.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HamRadioViewTune HamRadioViewTune;

typedef void (*HamViewTuneCallback)(HamRadioCustomEvent event, void* context);

HamRadioViewTune* ham_view_tune_alloc(HamViewTuneCallback callback, void* context);
void ham_view_tune_free(HamRadioViewTune* instance);
View* ham_view_tune_get_view(HamRadioViewTune* instance);

void ham_view_tune_set_frequency(HamRadioViewTune* instance, uint32_t frequency, const char* label);
void ham_view_tune_set_level(HamRadioViewTune* instance, float rssi_dbm, uint8_t lqi);

/** Short transmit-state string: why TX is blocked, or NULL when armed. The
 * `armed` flag drives the inverted rendering. */
void ham_view_tune_set_tx_state(HamRadioViewTune* instance, bool armed, const char* tx_state);

#ifdef __cplusplus
}
#endif
