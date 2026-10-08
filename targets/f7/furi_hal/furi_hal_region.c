#include <furi_hal_region.h>
#include <furi_hal_version.h>
#include <furi_hal_subghz.h>
#include <furi.h>
#include <string.h>

const FuriHalRegion furi_hal_region_zero = {
    .country_code = "00",
    .bands_count = 1,
    .bands = {{
        .start = 0,
        .end = 1000000000,
        .power_limit = 12,
        .duty_cycle = 100,
        .tokens_ms = 0,
        .last_update_tick = 0,
        .last_tx_tick = 0,
        .lbt_required = false,
        .lbt_rssi_threshold = 0,
        .lbt_min_backoff_ms = 0,
        .lbt_max_backoff_ms = 0,
    }}};

const FuriHalRegion furi_hal_region_eu_ru = {
    .country_code = "EU",
    .bands_count = 2,
    .bands = {
        {
            .start = 433050000,
            .end = 434790000,
            .power_limit = 12,
            .duty_cycle = 1,
            .tokens_ms = 0,
            .last_update_tick = 0,
            .last_tx_tick = 0,
            .lbt_required = false,
            .lbt_rssi_threshold = 0,
            .lbt_min_backoff_ms = 0,
            .lbt_max_backoff_ms = 0,
        },
        {
            .start = 868150000,
            .end = 868550000,
            .power_limit = 12,
            .duty_cycle = 10,
            .tokens_ms = 0,
            .last_update_tick = 0,
            .last_tx_tick = 0,
            .lbt_required = true,
            .lbt_rssi_threshold = -90,
            .lbt_min_backoff_ms = 5,
            .lbt_max_backoff_ms = 50,
        }}};

const FuriHalRegion furi_hal_region_us_ca_au = {
    .country_code = "US",
    .bands_count = 3,
    .bands = {
        {
            .start = 304100000,
            .end = 321950000,
            .power_limit = 12,
            .duty_cycle = 100,
            .tokens_ms = 0,
            .last_update_tick = 0,
            .last_tx_tick = 0,
            .lbt_required = false,
            .lbt_rssi_threshold = 0,
            .lbt_min_backoff_ms = 0,
            .lbt_max_backoff_ms = 0,
        },
        {
            .start = 433050000,
            .end = 434790000,
            .power_limit = 12,
            .duty_cycle = 100,
            .tokens_ms = 0,
            .last_update_tick = 0,
            .last_tx_tick = 0,
            .lbt_required = false,
            .lbt_rssi_threshold = 0,
            .lbt_min_backoff_ms = 0,
            .lbt_max_backoff_ms = 0,
        },
        {
            .start = 915000000,
            .end = 928000000,
            .power_limit = 12,
            .duty_cycle = 100,
            .tokens_ms = 0,
            .last_update_tick = 0,
            .last_tx_tick = 0,
            .lbt_required = false,
            .lbt_rssi_threshold = 0,
            .lbt_min_backoff_ms = 0,
            .lbt_max_backoff_ms = 0,
        }}};

const FuriHalRegion furi_hal_region_jp = {
    .country_code = "JP",
    .bands_count = 2,
    .bands = {
        {
            .start = 312000000,
            .end = 315250000,
            .power_limit = 12,
            .duty_cycle = 1,
            .tokens_ms = 0,
            .last_update_tick = 0,
            .last_tx_tick = 0,
            .lbt_required = true,
            .lbt_rssi_threshold = -90,
            .lbt_min_backoff_ms = 5,
            .lbt_max_backoff_ms = 50,
        },
        {
            .start = 920500000,
            .end = 923500000,
            .power_limit = 12,
            .duty_cycle = 1,
            .tokens_ms = 0,
            .last_update_tick = 0,
            .last_tx_tick = 0,
            .lbt_required = true,
            .lbt_rssi_threshold = -90,
            .lbt_min_backoff_ms = 5,
            .lbt_max_backoff_ms = 50,
        }}};

static const FuriHalRegion* const furi_hal_static_regions[] = {
    [FuriHalVersionRegionUnknown] = &furi_hal_region_zero,
    [FuriHalVersionRegionEuRu] = &furi_hal_region_eu_ru,
    [FuriHalVersionRegionUsCaAu] = &furi_hal_region_us_ca_au,
    [FuriHalVersionRegionJp] = &furi_hal_region_jp,
};

static FuriHalRegion* furi_hal_dynamic_region;
static FuriMutex* furi_hal_dynamic_region_mutex;

static inline bool furi_hal_region_is_in_flash(const void* ptr) {
    uint32_t addr = (uint32_t)ptr;
    return (addr >= 0x08000000 && addr < 0x08100000);
}

void furi_hal_region_init(void) {
    furi_assert(furi_hal_dynamic_region_mutex == NULL);
    furi_hal_dynamic_region_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // Initialize to World region (no restrictions) by default in RAM
    size_t size =
        sizeof(FuriHalRegion) + sizeof(FuriHalRegionBand) * furi_hal_region_zero.bands_count;
    furi_hal_dynamic_region = malloc(size);
    furi_check(furi_hal_dynamic_region);
    memcpy(furi_hal_dynamic_region, &furi_hal_region_zero, size);
}

const FuriHalRegion* furi_hal_region_get(void) {
    const FuriHalVersionRegion region = furi_hal_version_get_hw_region();
    const FuriHalRegion* ret;

    furi_check(furi_mutex_acquire(furi_hal_dynamic_region_mutex, FuriWaitForever) == FuriStatusOk);

    if(region < FuriHalVersionRegionWorld && furi_hal_dynamic_region == NULL) {
        ret = furi_hal_static_regions[region];
    } else {
        ret = furi_hal_dynamic_region;
    }

    furi_check(furi_mutex_release(furi_hal_dynamic_region_mutex) == FuriStatusOk);

    return ret;
}

void furi_hal_region_set(FuriHalRegion* region) {
    furi_check(region);

    furi_check(furi_mutex_acquire(furi_hal_dynamic_region_mutex, FuriWaitForever) == FuriStatusOk);

    FuriHalRegion* new_region = NULL;
    if(furi_hal_region_is_in_flash(region)) {
        size_t size = sizeof(FuriHalRegion) + sizeof(FuriHalRegionBand) * region->bands_count;
        new_region = malloc(size);
        furi_check(new_region);
        memcpy(new_region, region, size);
    } else {
        new_region = region;
    }

    if(furi_hal_dynamic_region && !furi_hal_region_is_in_flash(furi_hal_dynamic_region)) {
        free(furi_hal_dynamic_region);
    }

    furi_hal_dynamic_region = new_region;

    furi_check(furi_mutex_release(furi_hal_dynamic_region_mutex) == FuriStatusOk);
}

bool furi_hal_region_is_provisioned(void) {
    return furi_hal_region_get() != NULL;
}

const char* furi_hal_region_get_name(void) {
    const FuriHalRegion* region = furi_hal_region_get();

    if(region) {
        return region->country_code;
    } else {
        return "--";
    }
}

bool _furi_hal_region_is_frequency_allowed(uint32_t frequency) {
    return furi_hal_region_get_band(frequency) != NULL;
}

// Check furi_hal_subghz settings for region bypass, if not it uses function above
bool furi_hal_region_is_frequency_allowed(uint32_t frequency) {
    return furi_hal_subghz_is_tx_allowed(frequency);
}

const FuriHalRegionBand* furi_hal_region_get_band(uint32_t frequency) {
    const FuriHalRegion* region = furi_hal_region_get();

    if(!region) {
        return NULL;
    }

    for(size_t i = 0; i < region->bands_count; i++) {
        if(region->bands[i].start <= frequency && region->bands[i].end >= frequency) {
            return &region->bands[i];
        }
    }

    return NULL;
}
