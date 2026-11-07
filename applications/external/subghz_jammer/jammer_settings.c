#include "jammer_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format.h>

#include "subghz_jammer.h"

#define SUBGHZ_JAMMER_SETTINGS_FILE_TYPE    "Sub-GHz Jammer Settings File"
#define SUBGHZ_JAMMER_SETTINGS_FILE_VERSION 1
#define SUBGHZ_JAMMER_SETTINGS_PATH         APP_DATA_PATH("subghz_jammer.settings")

#define FIELD_MODE "Mode"
#define FIELD_PULSE_RATIO "PulseRatio"
#define FIELD_FREQUENCY "Frequency"
#define FIELD_BAND_MIN "BandMin"
#define FIELD_BAND_MAX "BandMax"
#define FIELD_HOP_COUNT "HopCount"
#define FIELD_DWELL_MS "DwellMs"
#define FIELD_RADIO "Radio"
#define FIELD_LATCH "Latch"

/* --- Advanced scheduling (Stage 3) --- */
#define FIELD_HOP_INTERVAL_MS "HopIntervalMs"
#define FIELD_DWELL_HOLD_MS "DwellHoldMs"
#define FIELD_SWEEP_STEP_HZ "SweepStepHz"
#define FIELD_NOISE_STEP_HZ "NoiseStepHz"
#define FIELD_POWER_LEVEL "PowerLevel"
#define FIELD_BYPASS_REGION "BypassRegion"
#define FIELD_MAX_TX_DURATION_MS "MaxTxDurationMs"
#define FIELD_TX_GATE_MODE "TxGateMode"

void jammer_settings_defaults(JammerSettings* settings) {
    settings->mode = JammerSignalModeCarrier;
    settings->pulse_ratio = 50;
    settings->frequency = 433920000u;
    settings->band_min = 300000000u;
    settings->band_max = 464000000u;
    settings->hop_count = 8;
    settings->dwell_ms = 2000;
    settings->radio = JammerRadioAuto;
    settings->latch = true;

    /* --- Advanced scheduling (Stage 3) defaults --- */
    settings->hop_interval_ms = 200;
    settings->dwell_hold_ms = 100;
    settings->sweep_step_hz = 250000;
    settings->noise_step_hz = 500000;
    settings->power_level = 0; /* 0 = auto */
    settings->bypass_region = 0;
    settings->max_tx_duration_ms = 300000; /* 5 minutes */
    settings->tx_gate_mode = 0; /* continuous */
}

void jammer_settings_load(JammerSettings* settings) {
    jammer_settings_defaults(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool file_open = false;
    size_t applied = 0;
    uint32_t temp_mode = 0;
    uint32_t temp_pulse_ratio = 0;
    uint32_t temp_freq = 0;
    uint32_t temp_band_min = 0;
    uint32_t temp_band_max = 0;
    uint32_t temp_hop = 0;
    uint32_t temp_dwell = 0;
    uint32_t temp_radio = 0;
    bool temp_latch = false;
    /* --- Advanced scheduling (Stage 3) --- */
    uint32_t temp_hop_interval_ms = 0;
    uint32_t temp_dwell_hold_ms = 0;
    uint32_t temp_sweep_step_hz = 0;
    uint32_t temp_noise_step_hz = 0;
    uint32_t temp_power_level = 0;
    uint32_t temp_bypass_region = 0;
    uint32_t temp_max_tx_duration_ms = 0;
    uint32_t temp_tx_gate_mode = 0;

    if((file_open = flipper_format_file_open_existing(fff, SUBGHZ_JAMMER_SETTINGS_PATH))) {
        // Read each field independently, rewinding before each search so a
        // missing/legacy field can't mask the others. FlipperFormat reads by
        // key name from the current cursor position, so a failed read for one
        // key would otherwise leave the cursor positioned to fail the rest.
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_MODE, &temp_mode, 1)) {
            settings->mode = (JammerSignalMode)temp_mode;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_PULSE_RATIO, &temp_pulse_ratio, 1)) {
            settings->pulse_ratio = temp_pulse_ratio;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_FREQUENCY, &temp_freq, 1)) {
            // Migrate legacy 868.00 MHz preset to standard 868.35 MHz
            if(temp_freq == 868000000u) {
                temp_freq = 868350000u;
            }
            settings->frequency = temp_freq;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_BAND_MIN, &temp_band_min, 1)) {
            settings->band_min = temp_band_min;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_BAND_MAX, &temp_band_max, 1)) {
            settings->band_max = temp_band_max;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_HOP_COUNT, &temp_hop, 1)) {
            settings->hop_count = (uint16_t)temp_hop;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_DWELL_MS, &temp_dwell, 1)) {
            settings->dwell_ms = (uint16_t)temp_dwell;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_RADIO, &temp_radio, 1)) {
            settings->radio = (JammerRadioChoice)temp_radio;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_bool(fff, FIELD_LATCH, &temp_latch, 1)) {
            settings->latch = temp_latch;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_HOP_INTERVAL_MS, &temp_hop_interval_ms, 1)) {
            settings->hop_interval_ms = temp_hop_interval_ms;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_DWELL_HOLD_MS, &temp_dwell_hold_ms, 1)) {
            settings->dwell_hold_ms = temp_dwell_hold_ms;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_SWEEP_STEP_HZ, &temp_sweep_step_hz, 1)) {
            settings->sweep_step_hz = temp_sweep_step_hz;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_NOISE_STEP_HZ, &temp_noise_step_hz, 1)) {
            settings->noise_step_hz = temp_noise_step_hz;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_POWER_LEVEL, &temp_power_level, 1)) {
            settings->power_level = temp_power_level;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_BYPASS_REGION, &temp_bypass_region, 1)) {
            settings->bypass_region = (uint8_t)temp_bypass_region;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_MAX_TX_DURATION_MS, &temp_max_tx_duration_ms, 1)) {
            settings->max_tx_duration_ms = temp_max_tx_duration_ms;
            applied++;
        }
        if(flipper_format_rewind(fff) &&
           flipper_format_read_uint32(fff, FIELD_TX_GATE_MODE, &temp_tx_gate_mode, 1)) {
            settings->tx_gate_mode = (uint8_t)temp_tx_gate_mode;
            applied++;
        }
    }

    flipper_format_file_close(fff);
    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);

    if(!file_open) {
        FURI_LOG_W(TAG, "settings file not found, using defaults");
    } else if(applied == 0) {
        FURI_LOG_W(TAG, "no saved settings, using defaults");
    } else if(applied < 17) {
        FURI_LOG_W(TAG, "loaded %zu of 17 settings; using defaults for the rest", applied);
    }
}

void jammer_settings_save(const JammerSettings* settings) {
    furi_assert(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool saved = false;

    /* Promote every field to a local uint32_t before handing its address to
     * flipper_format. The struct mixes uint8_t/uint16_t/bool/enum fields, so a
     * direct (uint32_t*)&settings->field would read past the field's own bytes
     * and alias the neighbour (e.g. hop_count overlaps dwell_ms, tx_gate_mode
     * reads past the struct end). */
    uint32_t s_mode = (uint32_t)settings->mode;
    uint32_t s_pulse_ratio = (uint32_t)settings->pulse_ratio;
    uint32_t s_frequency = settings->frequency;
    uint32_t s_band_min = settings->band_min;
    uint32_t s_band_max = settings->band_max;
    uint32_t s_hop_count = (uint32_t)settings->hop_count;
    uint32_t s_dwell_ms = (uint32_t)settings->dwell_ms;
    uint32_t s_radio = (uint32_t)settings->radio;
    uint32_t s_hop_interval_ms = settings->hop_interval_ms;
    uint32_t s_dwell_hold_ms = settings->dwell_hold_ms;
    uint32_t s_sweep_step_hz = settings->sweep_step_hz;
    uint32_t s_noise_step_hz = settings->noise_step_hz;
    uint32_t s_power_level = settings->power_level;
    uint32_t s_bypass_region = (uint32_t)settings->bypass_region;
    uint32_t s_max_tx_duration_ms = settings->max_tx_duration_ms;
    uint32_t s_tx_gate_mode = (uint32_t)settings->tx_gate_mode;

    do {
        if(!flipper_format_file_open_always(fff, SUBGHZ_JAMMER_SETTINGS_PATH)) {
            break;
        }
        if(!flipper_format_write_header_cstr(
               fff, SUBGHZ_JAMMER_SETTINGS_FILE_TYPE, SUBGHZ_JAMMER_SETTINGS_FILE_VERSION)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_MODE, &s_mode, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_PULSE_RATIO, &s_pulse_ratio, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_FREQUENCY, &s_frequency, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_BAND_MIN, &s_band_min, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_BAND_MAX, &s_band_max, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_HOP_COUNT, &s_hop_count, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_DWELL_MS, &s_dwell_ms, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_RADIO, &s_radio, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_bool(
               fff, FIELD_LATCH, &settings->latch, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_HOP_INTERVAL_MS, &s_hop_interval_ms, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_DWELL_HOLD_MS, &s_dwell_hold_ms, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_SWEEP_STEP_HZ, &s_sweep_step_hz, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_NOISE_STEP_HZ, &s_noise_step_hz, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_POWER_LEVEL, &s_power_level, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_BYPASS_REGION, &s_bypass_region, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_MAX_TX_DURATION_MS, &s_max_tx_duration_ms, 1)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               fff, FIELD_TX_GATE_MODE, &s_tx_gate_mode, 1)) {
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
