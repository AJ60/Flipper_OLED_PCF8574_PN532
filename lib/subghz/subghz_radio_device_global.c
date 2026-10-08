#include "subghz_radio_device_global.h"

#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <toolbox/path.h>

#define SUBGHZ_GLOBAL_LAST_SETTINGS_PATH     EXT_PATH("subghz/assets/last_subghz.settings")
#define SUBGHZ_GLOBAL_LAST_SETTING_FILE_TYPE "Flipper SubGhz Last Setting File"
#define SUBGHZ_GLOBAL_FIELD_RADIO_DEVICE     "RadioDevice"

uint32_t subghz_global_radio_device_load(void) {
    uint32_t radio_device = SUBGHZ_GLOBAL_RADIO_DEVICE_INTERNAL;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    uint32_t version = 0;

    do {
        if(storage_sd_status(storage) != FSE_OK) break;
        if(!flipper_format_file_open_existing(file, SUBGHZ_GLOBAL_LAST_SETTINGS_PATH)) break;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(strcmp(furi_string_get_cstr(temp_str), SUBGHZ_GLOBAL_LAST_SETTING_FILE_TYPE) != 0)
            break;
        // RadioDevice key only exists in v4+. v3 files => default Internal.
        if(version < 4) break;
        uint32_t value = SUBGHZ_GLOBAL_RADIO_DEVICE_INTERNAL;
        if(!flipper_format_read_uint32(file, SUBGHZ_GLOBAL_FIELD_RADIO_DEVICE, &value, 1)) break;
        radio_device = value;
    } while(0);

    furi_string_free(temp_str);
    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    if(radio_device != SUBGHZ_GLOBAL_RADIO_DEVICE_INTERNAL &&
       radio_device != SUBGHZ_GLOBAL_RADIO_DEVICE_EXTERNAL) {
        radio_device = SUBGHZ_GLOBAL_RADIO_DEVICE_INTERNAL;
    }
    return radio_device;
}
