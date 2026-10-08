#include "ham_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format.h>

#include <string.h>

#define TAG "HamSettings"

#define HAM_SETTINGS_FILE_TYPE "HamRadio Settings"
#define HAM_SETTINGS_FILE_VERSION 1
#define HAM_SETTINGS_PATH EXT_PATH("ham_radio/settings")

#define HAM_SETTINGS_FIELD_CALLSIGN        "callsign"
#define HAM_SETTINGS_FIELD_PATH            "path"
#define HAM_SETTINGS_FIELD_BAND_LABEL      "band_label"
#define HAM_SETTINGS_FIELD_TX_ENABLED      "tx_enabled"
#define HAM_SETTINGS_FIELD_CONSENT         "consent_accepted"
#define HAM_SETTINGS_FIELD_POWER_LIMIT     "power_limit_dbm"
#define HAM_SETTINGS_FIELD_DUTY_PERCENT    "duty_cycle_percent"
#define HAM_SETTINGS_FIELD_DUTY_WINDOW_MS  "duty_window_ms"

/** Copy at most size-1 bytes and always NUL terminate, so a hand-edited file
 * cannot overrun the destination. */
static void ham_settings_copy_cstr(char* dest, size_t size, const char* src) {
    if(size == 0) return;

    memset(dest, 0, size);
    if(src == NULL) return;

    const size_t length = strlen(src);
    const size_t copied = (length < size - 1) ? length : size - 1;
    memcpy(dest, src, copied);
}

void ham_settings_reset(HamSettings* instance) {
    furi_assert(instance);
    memset(instance, 0, sizeof(HamSettings));
    instance->power_limit_dbm = HAM_SETTINGS_DEFAULT_POWER_LIMIT_DBM;
    instance->duty_cycle_percent = HAM_SETTINGS_DEFAULT_DUTY_PERCENT;
    instance->duty_window_ms = HAM_SETTINGS_DEFAULT_DUTY_WINDOW_MS;
}

bool ham_settings_load(HamSettings* instance) {
    furi_assert(instance);
    ham_settings_reset(instance);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    bool loaded = false;
    do {
        if(FSE_OK != storage_sd_status(storage)) {
            FURI_LOG_W(TAG, "No SD card, keeping defaults");
            break;
        }
        if(!flipper_format_file_open_existing(file, HAM_SETTINGS_PATH)) {
            FURI_LOG_I(TAG, "No settings file yet, keeping defaults");
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_uint32(file, "version", &version, 1)) {
            flipper_format_rewind(file);
        } else if(version != HAM_SETTINGS_FILE_VERSION) {
            FURI_LOG_W(TAG, "Settings version %lu not supported", (unsigned long)version);
            flipper_format_file_close(file);
            break;
        }

        // Every field is optional: a short file must not leave garbage behind,
        // so each read starts from the value set by ham_settings_reset().
        FuriString* scratch = furi_string_alloc();
        flipper_format_read_string(file, HAM_SETTINGS_FIELD_CALLSIGN, scratch);
        ham_settings_copy_cstr(instance->callsign, sizeof(instance->callsign), furi_string_get_cstr(scratch));
        flipper_format_read_string(file, HAM_SETTINGS_FIELD_PATH, scratch);
        ham_settings_copy_cstr(instance->path, sizeof(instance->path), furi_string_get_cstr(scratch));
        flipper_format_read_string(file, HAM_SETTINGS_FIELD_BAND_LABEL, scratch);
        ham_settings_copy_cstr(
            instance->band_label, sizeof(instance->band_label), furi_string_get_cstr(scratch));
        furi_string_free(scratch);

        flipper_format_read_bool(file, HAM_SETTINGS_FIELD_TX_ENABLED, &instance->tx_enabled, 1);
        flipper_format_read_bool(file, HAM_SETTINGS_FIELD_CONSENT, &instance->consent_accepted, 1);
        flipper_format_read_uint32(file, HAM_SETTINGS_FIELD_POWER_LIMIT, (uint32_t*)&instance->power_limit_dbm, 1);
        flipper_format_read_uint32(file, HAM_SETTINGS_FIELD_DUTY_PERCENT, (uint32_t*)&instance->duty_cycle_percent, 1);
        flipper_format_read_uint32(file, HAM_SETTINGS_FIELD_DUTY_WINDOW_MS, &instance->duty_window_ms, 1);

        loaded = true;
    } while(0);

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    // Clamp anything a hand-edited file could put out of range.
    if(instance->power_limit_dbm > 20) {
        instance->power_limit_dbm = HAM_SETTINGS_DEFAULT_POWER_LIMIT_DBM;
    }
    if(instance->duty_cycle_percent == 0 || instance->duty_cycle_percent > 100) {
        instance->duty_cycle_percent = HAM_SETTINGS_DEFAULT_DUTY_PERCENT;
    }
    if(instance->duty_window_ms == 0) {
        instance->duty_window_ms = HAM_SETTINGS_DEFAULT_DUTY_WINDOW_MS;
    }

    // Consent is deliberately not persisted as an armed state: re-deriving it
    // from the file keeps the "operator asked for this" signal, while the
    // callsign check below is what actually gates transmit.
    if(instance->callsign[0] == '\0') {
        instance->tx_enabled = false;
        instance->consent_accepted = false;
    }

    FURI_LOG_I(TAG, "Settings loaded: call=%s tx=%d consent=%d", instance->callsign, instance->tx_enabled, instance->consent_accepted);
    return loaded;
}

bool ham_settings_save(const HamSettings* instance) {
    furi_assert(instance);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    bool saved = false;
    do {
        if(FSE_OK != storage_sd_status(storage)) {
            FURI_LOG_W(TAG, "No SD card, cannot persist settings");
            break;
        }
        if(!flipper_format_file_open_always(file, HAM_SETTINGS_PATH)) {
            FURI_LOG_E(TAG, "Cannot open %s", HAM_SETTINGS_PATH);
            break;
        }

        uint32_t version = HAM_SETTINGS_FILE_VERSION;
        if(!flipper_format_write_uint32(file, "version", &version, 1)) break;
        if(!flipper_format_write_string_cstr(file, HAM_SETTINGS_FIELD_CALLSIGN, instance->callsign)) break;
        if(!flipper_format_write_string_cstr(file, HAM_SETTINGS_FIELD_PATH, instance->path)) break;
        if(!flipper_format_write_string_cstr(file, HAM_SETTINGS_FIELD_BAND_LABEL, instance->band_label)) break;
        if(!flipper_format_write_bool(file, HAM_SETTINGS_FIELD_TX_ENABLED, &instance->tx_enabled, 1)) break;
        if(!flipper_format_write_bool(file, HAM_SETTINGS_FIELD_CONSENT, &instance->consent_accepted, 1)) break;
        if(!flipper_format_write_uint32(file, HAM_SETTINGS_FIELD_POWER_LIMIT, (uint32_t*)&instance->power_limit_dbm, 1)) break;
        if(!flipper_format_write_uint32(file, HAM_SETTINGS_FIELD_DUTY_PERCENT, (uint32_t*)&instance->duty_cycle_percent, 1)) break;
        if(!flipper_format_write_uint32(file, HAM_SETTINGS_FIELD_DUTY_WINDOW_MS, &instance->duty_window_ms, 1)) break;

        saved = true;
    } while(0);

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    return saved;
}

bool ham_settings_is_tx_armed(const HamSettings* instance) {
    furi_assert(instance);
    return instance->tx_enabled && instance->consent_accepted && instance->callsign[0] != '\0';
}

bool ham_settings_duty_cycle_allows(const HamSettings* instance, uint32_t now_tick) {
    furi_assert(instance);
    if(instance->duty_cycle_percent == 0 || instance->duty_cycle_percent >= 100) return true;

    const uint32_t window_ticks = furi_ms_to_ticks(instance->duty_window_ms);
    if(now_tick - instance->last_tx_tick >= window_ticks) {
        // Window elapsed, so the accumulated on-air time resets.
        return true;
    }

    const uint32_t budget_ms = (instance->duty_window_ms / 100u) * instance->duty_cycle_percent;
    return instance->tx_time_ms < budget_ms;
}

void ham_settings_tx_record(HamSettings* instance, uint32_t duration_ms) {
    furi_assert(instance);

    const uint32_t now_tick = furi_get_tick();
    const uint32_t window_ticks = furi_ms_to_ticks(instance->duty_window_ms);
    if(now_tick - instance->last_tx_tick >= window_ticks) {
        instance->tx_time_ms = 0;
        instance->last_tx_tick = now_tick;
    }

    instance->tx_time_ms += duration_ms;
}

uint8_t ham_settings_duty_cycle_percent_used(const HamSettings* instance) {
    furi_assert(instance);
    if(instance->duty_window_ms == 0) return 0;

    const uint32_t used = (instance->tx_time_ms / 100u * 100u) / instance->duty_window_ms;
    return (uint8_t)CLAMP(used, 0u, 100u);
}
