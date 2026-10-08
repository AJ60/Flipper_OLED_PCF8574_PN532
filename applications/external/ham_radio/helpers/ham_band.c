#include "ham_band.h"

#include <furi.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format.h>

#include <string.h>

#define TAG "HamBand"
#define HAM_BAND_FILE_PATH EXT_PATH("ham_radio/band_plan.txt")

static const HamBandEntry ham_band_defaults[] = {
    {.label = "70cm FM Call", .frequency = 433500000, .tx_frequency = 0, .usage = HamBandUsageReceive | HamBandUsageTransmit, .module = HamBandModuleAny},
    {.label = "Simplex 0",    .frequency = 433000000, .tx_frequency = 0, .usage = HamBandUsageReceive | HamBandUsageTransmit, .module = HamBandModuleAny},
    {.label = "Simplex 1",    .frequency = 433050000, .tx_frequency = 0, .usage = HamBandUsageReceive | HamBandUsageTransmit, .module = HamBandModuleAny},
    {.label = "Simplex 2",    .frequency = 433100000, .tx_frequency = 0, .usage = HamBandUsageReceive | HamBandUsageTransmit, .module = HamBandModuleAny},
    {.label = "Simplex 4",    .frequency = 433200000, .tx_frequency = 0, .usage = HamBandUsageReceive | HamBandUsageTransmit, .module = HamBandModuleAny},
    {.label = "ISM/LPD 433",  .frequency = 433920000, .tx_frequency = 0, .usage = HamBandUsageReceive | HamBandUsageTransmit, .module = HamBandModuleAny},
    {.label = "Digital/LoRa", .frequency = 434000000, .tx_frequency = 0, .usage = HamBandUsageReceive | HamBandUsageTransmit, .module = HamBandModuleAny},
    {.label = "US Call/PMR",  .frequency = 446000000, .tx_frequency = 0, .usage = HamBandUsageReceive | HamBandUsageTransmit, .module = HamBandModuleAny},
};

void ham_band_init(HamBand* band) {
    furi_assert(band);
    memset(band, 0, sizeof(HamBand));

    for(size_t i = 0; i < COUNT_OF(ham_band_defaults); i++) {
        ham_band_add(band, &ham_band_defaults[i]);
    }
}

bool ham_band_frequency_is_in_range(uint32_t frequency) {
    return (frequency >= HAM_BAND_MIN_FREQUENCY) && (frequency <= HAM_BAND_MAX_FREQUENCY);
}

bool ham_band_add(HamBand* band, const HamBandEntry* entry) {
    furi_assert(band);
    furi_assert(entry);

    if(band->count >= HAM_BAND_MAX_ENTRIES) {
        FURI_LOG_W(TAG, "Plan full (%u entries)", band->count);
        return false;
    }

    if(!ham_band_frequency_is_in_range(entry->frequency)) {
        FURI_LOG_W(TAG, "Frequency %lu Hz out of range", (unsigned long)entry->frequency);
        return false;
    }

    // An entry with neither flag set would be invisible to the scan and unusable
    // for TX, which is never what the operator meant.
    if((entry->usage & (HamBandUsageReceive | HamBandUsageTransmit)) == 0) {
        FURI_LOG_W(TAG, "Entry needs at least one usage flag");
        return false;
    }

    if(entry->tx_frequency != 0 && !ham_band_frequency_is_in_range(entry->tx_frequency)) {
        FURI_LOG_W(TAG, "TX frequency %lu Hz out of range", (unsigned long)entry->tx_frequency);
        return false;
    }

    band->entries[band->count] = *entry;
    band->entries[band->count].label[HAM_BAND_LABEL_SIZE - 1] = '\0';
    band->count++;

    FURI_LOG_D(
        TAG,
        "Added '%s' @ %lu Hz usage=0x%02X module=%d",
        band->entries[band->count - 1].label,
        (unsigned long)band->entries[band->count - 1].frequency,
        band->entries[band->count - 1].usage,
        band->entries[band->count - 1].module);
    return true;
}

bool ham_band_remove(HamBand* band, uint8_t index) {
    furi_assert(band);

    if(index >= band->count) {
        FURI_LOG_W(TAG, "Remove index %u out of range (count %u)", index, band->count);
        return false;
    }

    if(index + 1 < band->count) {
        memmove(
            &band->entries[index],
            &band->entries[index + 1],
            (band->count - index - 1) * sizeof(HamBandEntry));
    }
    band->count--;
    memset(&band->entries[band->count], 0, sizeof(HamBandEntry));

    return true;
}

uint32_t ham_band_entry_tx_frequency(const HamBandEntry* entry) {
    furi_assert(entry);
    return (entry->tx_frequency != 0) ? entry->tx_frequency : entry->frequency;
}

uint8_t ham_band_collect_rx(const HamBand* band, uint32_t* frequencies, uint8_t max_count) {
    furi_assert(band);
    furi_assert(frequencies);

    uint8_t collected = 0;
    for(uint8_t i = 0; i < band->count && collected < max_count; ++i) {
        if(band->entries[i].usage & HamBandUsageReceive) {
            frequencies[collected] = band->entries[i].frequency;
            collected++;
        }
    }
    return collected;
}

bool ham_band_save(const HamBand* band) {
    furi_assert(band);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    bool saved = false;
    do {
        storage_simply_mkdir(storage, EXT_PATH("ham_radio"));
        if(!flipper_format_file_open_always(file, HAM_BAND_FILE_PATH)) break;

        flipper_format_write_header_cstr(file, "HamRadio BandPlan", 1);
        uint32_t count = band->count;
        flipper_format_write_uint32(file, "count", &count, 1);

        for(uint8_t i = 0; i < band->count; ++i) {
            char key[32];
            snprintf(key, sizeof(key), "entry_%u_label", i);
            flipper_format_write_string_cstr(file, key, band->entries[i].label);

            snprintf(key, sizeof(key), "entry_%u_freq", i);
            uint32_t freq = band->entries[i].frequency;
            flipper_format_write_uint32(file, key, &freq, 1);

            snprintf(key, sizeof(key), "entry_%u_tx_freq", i);
            uint32_t tx_freq = band->entries[i].tx_frequency;
            flipper_format_write_uint32(file, key, &tx_freq, 1);

            snprintf(key, sizeof(key), "entry_%u_usage", i);
            uint32_t usage = band->entries[i].usage;
            flipper_format_write_uint32(file, key, &usage, 1);
        }

        saved = true;
    } while(0);

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return saved;
}

bool ham_band_load(HamBand* band) {
    furi_assert(band);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    bool loaded = false;
    do {
        if(FSE_OK != storage_sd_status(storage)) break;
        if(!flipper_format_file_open_existing(file, HAM_BAND_FILE_PATH)) break;

        uint32_t version = 0;
        if(!flipper_format_read_uint32(file, "version", &version, 1)) {
            flipper_format_rewind(file);
        }

        uint32_t count = 0;
        if(!flipper_format_read_uint32(file, "count", &count, 1) || count == 0) {
            break;
        }

        memset(band, 0, sizeof(HamBand));
        FuriString* scratch = furi_string_alloc();

        for(uint8_t i = 0; i < count && i < HAM_BAND_MAX_ENTRIES; ++i) {
            HamBandEntry entry;
            memset(&entry, 0, sizeof(entry));

            char key[32];
            snprintf(key, sizeof(key), "entry_%u_label", i);
            if(flipper_format_read_string(file, key, scratch)) {
                strncpy(entry.label, furi_string_get_cstr(scratch), sizeof(entry.label) - 1);
            }

            snprintf(key, sizeof(key), "entry_%u_freq", i);
            uint32_t freq = 0;
            if(flipper_format_read_uint32(file, key, &freq, 1)) {
                entry.frequency = freq;
            }

            snprintf(key, sizeof(key), "entry_%u_tx_freq", i);
            uint32_t tx_freq = 0;
            if(flipper_format_read_uint32(file, key, &tx_freq, 1)) {
                entry.tx_frequency = tx_freq;
            }

            snprintf(key, sizeof(key), "entry_%u_usage", i);
            uint32_t usage = HamBandUsageReceive | HamBandUsageTransmit;
            flipper_format_read_uint32(file, key, &usage, 1);
            entry.usage = (uint8_t)usage;

            entry.module = HamBandModuleAny;

            if(ham_band_frequency_is_in_range(entry.frequency)) {
                band->entries[band->count++] = entry;
            }
        }

        furi_string_free(scratch);
        loaded = (band->count > 0);
    } while(0);

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!loaded) {
        ham_band_init(band);
    }

    return loaded;
}
