#pragma once

#include <gui/view.h>
#include "../ham_radio_app_i.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HamRadioViewBand HamRadioViewBand;

/** Emitted when the operator commits to an action on the band list. The
 * selected index is read back with ham_view_band_get_selected(). */
typedef void (*HamViewBandCallback)(HamRadioCustomEvent event, void* context);

HamRadioViewBand* ham_view_band_alloc(HamViewBandCallback callback, void* context);
void ham_view_band_free(HamRadioViewBand* instance);
View* ham_view_band_get_view(HamRadioViewBand* instance);

/** Replace the displayed plan and refresh the selection bounds. */
void ham_view_band_set_plan(HamRadioViewBand* instance, const HamBand* band);

void ham_view_band_set_selected(HamRadioViewBand* instance, uint8_t index);
uint8_t ham_view_band_get_selected(HamRadioViewBand* instance);

/** Copy the selected entry out. Returns false when the selection is invalid. */
bool ham_view_band_get_selected_entry(HamRadioViewBand* instance, HamBandEntry* out_entry);

/** Transmit arm state and duty-cycle usage for the status strip. */
void ham_view_band_set_tx_state(HamRadioViewBand* instance, bool armed, uint8_t duty_used);
void ham_view_band_set_module(HamRadioViewBand* instance, bool external);

#ifdef __cplusplus
}
#endif
