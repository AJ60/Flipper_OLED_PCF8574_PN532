#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAM_BAND_MAX_ENTRIES      (32)
#define HAM_BAND_LABEL_SIZE       (12)
#define HAM_BAND_MIN_FREQUENCY    (300000000ul)
#define HAM_BAND_MAX_FREQUENCY    (928000000ul)
#define HAM_BAND_FREQUENCY_STEP   (25000ul)

/** Which radio the operator expects to serve this frequency. Any defers to the
 * firmware-wide selection persisted in last_subghz.settings. */
typedef enum {
    HamBandModuleAny,
    HamBandModuleInternal,
    HamBandModuleExternal,
} HamBandModule;

/** What a frequency entry is used for. */
typedef enum {
    HamBandUsageReceive = 1 << 0,
    HamBandUsageTransmit = 1 << 1,
} HamBandUsage;

typedef struct {
    char label[HAM_BAND_LABEL_SIZE];
    uint32_t frequency;
    /** For repeaters: the frequency a TX here lands on. 0 means same-as-RX. */
    uint32_t tx_frequency;
    uint8_t usage;
    HamBandModule module;
} HamBandEntry;

typedef struct {
    HamBandEntry entries[HAM_BAND_MAX_ENTRIES];
    uint8_t count;
} HamBand;

/** Initialize an empty plan. No frequencies are shipped: the operator enters
 * the ones their licence covers. */
void ham_band_init(HamBand* band);

/** Append an entry. Returns false when the plan is full or the frequency is
 * outside the transceiver's usable range. */
bool ham_band_add(HamBand* band, const HamBandEntry* entry);

/** Remove entry at index, shifting the remainder down. */
bool ham_band_remove(HamBand* band, uint8_t index);

/** TX frequency an entry transmits on, resolving the repeater case. */
uint32_t ham_band_entry_tx_frequency(const HamBandEntry* entry);

/** True when frequency sits inside the sub-GHz range this transceiver covers.
 * Does not imply a transmit permit: the region table decides that separately. */
bool ham_band_frequency_is_in_range(uint32_t frequency);

/** Keep only receive entries, in stored order. Returns how many there are. */
uint8_t ham_band_collect_rx(const HamBand* band, uint32_t* frequencies, uint8_t max_count);

/** Load band plan from storage, falling back to defaults if not found. */
bool ham_band_load(HamBand* band);

/** Save band plan to storage. */
bool ham_band_save(const HamBand* band);

#ifdef __cplusplus
}
#endif
