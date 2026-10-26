#include "jammer_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format.h>

#include "subghz_jammer.h"

#define SUBGHZ_JAMMER_SETTINGS_FILE_TYPE    "Sub-GHz Jammer Settings File"
#define SUBGHZ_JAMMER_SETTINGS_FILE_VERSION 1
#define SUBGHZ_JAMMER_SETTINGS_PATH         APP_DATA_PATH("subghz_jammer.settings")

#define FIELD_MODE        "Mode"
#define FIELD_FREQUENCY   "Frequency"
#define FIELD_BAND_MIN    "BandMin"
#define FIELD_BAND_MAX    "BandMax"
#define FIELD_HOP_COUNT   "HopCount"
#define FIELD_DWELL_MS    "DwellMs"
#define FIELD_RADIO       "Radio"
#define FIELD_LATCH       "Latch"

void jammer_settings_defaults(JammerSettings* settings) {
    settings->mode = JammerSignalModeCarrier;
    settings->frequency = 433920000u;
    settings->band_min = 300000000u;
    settings->band_max = 464000000u;
    settings->hop_count = 8;
    settings->dwell_ms = 2000;
    settings->radio = JammerRadioAuto;
    settings->latch = true;
}

void jammer_settings_load(JammerSettings* settings) {
    jammer_settings_defaults(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool loaded = false;
    uint32_t temp_mode = 0;
    uint32_t temp_freq = 0;
    uint32_t temp_band_min = 0;
    uint32_t temp_band_max = 0;
    uint32_t temp_hop = 0;
    uint32_t temp_dwell = 0;
    uint32_t temp_radio = 0;
    bool temp_latch = false;

    if(flipper_format_file_open_existing(fff, SUBGHZ_JAMMER_SETTINGS_PATH)) {
        loaded =
            flipper_format_read_uint32(fff, FIELD_MODE, &temp_mode, 1) &&
            flipper_format_read_uint32(fff, FIELD_FREQUENCY, &temp_freq, 1) &&
            flipper_format_read_uint32(fff, FIELD_BAND_MIN, &temp_band_min, 1) &&
            flipper_format_read_uint32(fff, FIELD_BAND_MAX, &temp_band_max, 1) &&
            flipper_format_read_uint32(fff, FIELD_HOP_COUNT, &temp_hop, 1) &&
            flipper_format_read_uint32(fff, FIELD_DWELL_MS, &temp_dwell, 1) &&
            flipper_format_read_uint32(fff, FIELD_RADIO, &temp_radio, 1) &&
            flipper_format_read_bool(fff, FIELD_LATCH, &temp_latch, 1);
    }

    flipper_format_file_close(fff);
    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);

    if(loaded) {
        settings->mode = (JammerSignalMode)temp_mode;
        settings->frequency = temp_freq;
        settings->band_min = temp_band_min;
        settings->band_max = temp_band_max;
        settings->hop_count = (uint16_t)temp_hop;
        settings->dwell_ms = (uint16_t)temp_dwell;
        settings->radio = (JammerRadioChoice)temp_radio;
        settings->latch = temp_latch;
    } else {
        FURI_LOG_W(TAG, "no saved settings, using defaults");
    }
}

void jammer_settings_save(const JammerSettings* settings) {
    furi_assert(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool saved = false;

    do {
        if(!flipper_format_file_open_always(fff, SUBGHZ_JAMMER_SETTINGS_PATH)) {
            break;
        }
        if(!flipper_format_write_header_cstr(
               fff, SUBGHZ_JAMMER_SETTINGS_FILE_TYPE, SUBGHZ_JAMMER_SETTINGS_FILE_VERSION)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_MODE, (uint32_t*)&settings->mode, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_FREQUENCY, (uint32_t*)&settings->frequency, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_BAND_MIN, (uint32_t*)&settings->band_min, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_BAND_MAX, (uint32_t*)&settings->band_max, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_HOP_COUNT, (uint32_t*)&settings->hop_count, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_DWELL_MS, (uint32_t*)&settings->dwell_ms, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_RADIO, (uint32_t*)&settings->radio, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_bool(
               fff, FIELD_LATCH, &settings->latch, 1)) {
            break;
        }
        saved = true;
        break;
    } while(true);

    if(!saved) {
        FURI_LOG_E(TAG, "failed to save settings");
    }

    flipper_format_file_close(fff);
    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
}
