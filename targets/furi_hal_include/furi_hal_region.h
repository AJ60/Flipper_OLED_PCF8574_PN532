#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t start;
    uint32_t end;
    int8_t power_limit;
    uint8_t duty_cycle;
    // Duty cycle enforcement (token bucket)
    uint32_t tokens_ms; // Available TX time in ms (scaled *1000)
    uint32_t last_update_tick; // Last token refill tick
    uint32_t last_tx_tick; // Last TX start tick
    // Listen-Before-Talk (LBT)
    bool lbt_required; // True for ETSI 868, ARIB, CN
    int8_t lbt_rssi_threshold; // RSSI threshold in dBm (e.g., -90)
    uint16_t lbt_min_backoff_ms; // Min backoff (ms)
    uint16_t lbt_max_backoff_ms; // Max backoff (ms)
} FuriHalRegionBand;

typedef struct {
    char country_code[4];
    uint16_t bands_count;
    FuriHalRegionBand bands[];
} FuriHalRegion;

/** Initialize region */
void furi_hal_region_init(void);

/** Get Region Data.
 * 
 * Region data may be allocated in Flash or in RAM.
 * Keep in mind that we don't do memory management on our side.
 *
 * @return     pointer to FuriHalRegion instance (in RAM or Flash, check before freeing on region update)
 */
const FuriHalRegion* furi_hal_region_get(void);

/** Set device region data
 *
 * @param      region  pointer to the FuriHalRegion
 */
void furi_hal_region_set(FuriHalRegion* region);

/** Check if region data provisioned
 *
 * @return     true if provisioned, false otherwise
 */
bool furi_hal_region_is_provisioned(void);

/** Get region name
 * 
 * 2 letter Region code according to iso 3166 standard
 * There are 2 extra values that we use in special cases:
 * - "00" - developer edition, unlocked
 * - "WW" - world wide, region provisioned by default
 * - "--" - no provisioned region
 *
 * @return     Pointer to string
 */
const char* furi_hal_region_get_name(void);

/** Сheck if transmission is allowed on this frequency for your flipper region
 *
 * @param[in]  frequency  The frequency
 * @param      value  frequency in Hz
 *
 * @return     true if allowed
 */
bool furi_hal_region_is_frequency_allowed(uint32_t frequency);

/** Get band data for frequency
 * 
 * 
 *
 * @param[in]  frequency  The frequency
 *
 * @return     { description_of_the_return_value }
 */
const FuriHalRegionBand* furi_hal_region_get_band(uint32_t frequency);

extern const FuriHalRegion furi_hal_region_zero;
extern const FuriHalRegion furi_hal_region_eu_ru;
extern const FuriHalRegion furi_hal_region_us_ca_au;
extern const FuriHalRegion furi_hal_region_jp;

#ifdef __cplusplus
}
#endif
