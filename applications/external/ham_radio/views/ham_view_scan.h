#pragma once

#include <gui/view.h>
#include "../ham_radio_app_i.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HamRadioViewScan HamRadioViewScan;

/** Emitted when the operator leaves the sweep or pins a channel. */
typedef void (*HamViewScanCallback)(HamRadioCustomEvent event, void* context);

HamRadioViewScan* ham_view_scan_alloc(HamViewScanCallback callback, void* context);
void ham_view_scan_free(HamRadioViewScan* instance);
View* ham_view_scan_get_view(HamRadioViewScan* instance);

/** Frequencies currently being swept, used for the axis labels. */
void ham_view_scan_set_channels(HamRadioViewScan* instance, const uint32_t* frequencies, uint8_t count);

/** Feed one completed sweep. */
void ham_view_scan_set_result(HamRadioViewScan* instance, const HamScanResult* result);

/** Index of the strongest channel from the last sweep. */
uint8_t ham_view_scan_get_peak(HamRadioViewScan* instance);

/** Frequency at a channel index. Returns 0 when the index is out of range. */
uint32_t ham_view_scan_get_frequency(HamRadioViewScan* instance, uint8_t index);

void ham_view_scan_set_module(HamRadioViewScan* instance, bool external);

/** Running/paused indicator. */
void ham_view_scan_set_running(HamRadioViewScan* instance, bool running);

#ifdef __cplusplus
}
#endif
