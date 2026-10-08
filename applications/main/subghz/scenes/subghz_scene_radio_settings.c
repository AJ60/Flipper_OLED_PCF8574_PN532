#include "../subghz_i.h"
#include "../helpers/subghz_txrx_i.h"
#include "../helpers/subghz_custom_event.h"
#include <lib/toolbox/value_index.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>

#define RADIO_DEVICE_COUNT 2
const char* const radio_device_text[RADIO_DEVICE_COUNT] = {
    "Internal",
    "External",
};

const uint32_t radio_device_value[RADIO_DEVICE_COUNT] = {
    SubGhzRadioDeviceTypeInternal,
    SubGhzRadioDeviceTypeExternalCC1101,
};

#define ON_OFF_COUNT 2
const char* const on_off_text[ON_OFF_COUNT] = {
    "OFF",
    "ON",
};

#define REGION_COUNT 5
const char* const region_text[REGION_COUNT] = {
    "World (No restrictions)",
    "EU/RU",
    "US/CA/AU",
    "JP",
    "Custom File",
};

#define EXT_RANGE_COUNT 2
const char* const ext_range_text[EXT_RANGE_COUNT] = {
    "OFF",
    "ON (Yard Stick One)",
};

#define DEBUG_P_COUNT 2
const char* const debug_pin_text[DEBUG_P_COUNT] = {
    "OFF",
    "17(1W)",
};

#define GPS_COUNT 6
const char* const gps_text[GPS_COUNT] = {
    "OFF",
    "9600",
    "19200",
    "38400",
    "57600",
    "115200",
};

#define DEBUG_COUNTER_COUNT 17
const char* const debug_counter_text[DEBUG_COUNTER_COUNT] = {
    "+1",
    "+2",
    "+3",
    "+4",
    "+5",
    "+10",
    "+50",
    "OVFL",
    "OFEX",
    "No",
    "-1",
    "-2",
    "-3",
    "-4",
    "-5",
    "-10",
    "-50",
};
const int32_t debug_counter_val[DEBUG_COUNTER_COUNT] = {
    1,
    2,
    3,
    4,
    5,
    10,
    50,
    65535,
    -2147483647,
    0,
    -1,
    -2,
    -3,
    -4,
    -5,
    -10,
    -50,
};

//TX Power
#define TX_POWER_COUNT 9
const char* const tx_power_text[TX_POWER_COUNT] = {
    "Preset",
    "10dBm +",
    "7dBm",
    "5dBm",
    "0dBm",
    "-10dBm",
    "-15dBm",
    "-20dBm",
    "-30dBm",
};

static void subghz_scene_radio_settings_set_device(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    // Probing the external module costs a full SPI transaction batch, so cache it
    // instead of re-probing for every use below.
    bool external_connected = subghz_txrx_radio_device_is_external_connected(
        subghz->txrx, SUBGHZ_DEVICE_CC1101_EXT_NAME);

    FURI_LOG_I(
        "SubGhzRadioSettings",
        "Device selector changed: index=%u text=%s external_connected=%u",
        index,
        radio_device_text[index],
        external_connected);

    if(!external_connected && radio_device_value[index] == SubGhzRadioDeviceTypeExternalCC1101) {
        FURI_LOG_E("SubGhzRadioSettings", "External CC1101 not detected or plugin missing");
        index = 0;
        variable_item_set_current_value_index(item, 0);
    }
    SubGhzRadioDeviceType actual =
        subghz_txrx_radio_device_set(subghz->txrx, radio_device_value[index]);
    if(actual != radio_device_value[index]) {
        index = 0;
        variable_item_set_current_value_index(item, 0);
    }
    // Persist global selection so external SubGHz apps honor it.
    // Canonical values: 1=Internal, 2=ExternalCC1101.
    subghz->last_settings->radio_device = (uint32_t)radio_device_value[index];
    subghz_last_settings_save(subghz->last_settings);
    variable_item_set_current_value_text(item, radio_device_text[index]);
}

static void subghz_scene_radio_settings_set_tx_power(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    //Update the Menu Item on screen
    variable_item_set_current_value_text(item, tx_power_text[index]);

    //Set TX power and remember setting
    subghz->last_settings->tx_power = subghz->tx_power = index;

    //Save the settings now, this is the convention here!
    subghz_last_settings_save(subghz->last_settings);

    // Apply TX power to current preset data immediately so it takes effect
    // on the next TX without requiring a preset reload.
    subghz_txrx_set_preset_internal(
        subghz->txrx, subghz->last_settings->frequency, subghz->last_settings->preset_index, index);
}

static void subghz_scene_receiver_config_set_debug_pin(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, debug_pin_text[index]);

    subghz_txrx_set_debug_pin_state(subghz->txrx, index == 1);
}

static void subghz_scene_receiver_config_set_debug_counter(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, debug_counter_text[index]);
    furi_hal_subghz_set_rolling_counter_mult(debug_counter_val[index]);
}

static void subghz_scene_receiver_config_set_gps(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, gps_text[index]);

    switch(index) {
    case 0:
        subghz->last_settings->gps_baudrate = 0;
        break;
    case 1:
        subghz->last_settings->gps_baudrate = 9600;
        break;
    case 2:
        subghz->last_settings->gps_baudrate = 19200;
        break;
    case 3:
        subghz->last_settings->gps_baudrate = 38400;
        break;
    case 4:
        subghz->last_settings->gps_baudrate = 57600;
        break;
    case 5:
        subghz->last_settings->gps_baudrate = 115200;
        break;
    }
    subghz_last_settings_save(subghz->last_settings);

    if(subghz->gps) {
        subghz_gps_plugin_deinit(subghz->gps);
        subghz->gps = NULL;
    }
    if(subghz->last_settings->gps_baudrate != 0) {
        subghz->gps = subghz_gps_plugin_init(subghz->last_settings->gps_baudrate);
    }
}

static void subghz_scene_receiver_config_set_protocol_file_names(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, on_off_text[index]);

    subghz->last_settings->protocol_file_names = (index == 1);
    subghz_last_settings_save(subghz->last_settings);
}

static void subghz_scene_radio_settings_set_region(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    const char* regions[] = {"00", "EU", "US", "JP", "CUSTOM"};
    variable_item_set_current_value_text(item, region_text[index]);
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    subghz_setting_set_region(setting, regions[index]);
    subghz_setting_apply_region(setting);
    subghz_setting_save(setting, EXT_PATH("subghz/assets/setting_user"));
}

static void subghz_scene_radio_settings_set_ext_range(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    bool enabled = (index == 1);

    variable_item_set_current_value_text(item, ext_range_text[index]);
    furi_hal_subghz_set_extended_range(enabled);
    furi_hal_subghz_set_bypass_region(enabled);

    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    if(enabled) {
        subghz_setting_load_default_extended(setting);
    }

    // Save to extend_range.txt and setting_user
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ext_file = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_always(ext_file, EXT_PATH("subghz/assets/extend_range.txt"))) {
        flipper_format_write_header_cstr(ext_file, "Flipper SubGhz Setting File", 1);
        flipper_format_write_bool(ext_file, "acknowledged", &enabled, 1);
        flipper_format_write_bool(ext_file, "use_ext_range_at_own_risk", &enabled, 1);
        flipper_format_write_bool(ext_file, "ignore_default_tx_region", &enabled, 1);
    }
    flipper_format_free(ext_file);
    furi_record_close(RECORD_STORAGE);

    subghz_setting_save(setting, EXT_PATH("subghz/assets/setting_user"));
}

void subghz_scene_radio_settings_on_enter(void* context) {
    SubGhz* subghz = context;

    VariableItemList* variable_item_list = subghz->variable_item_list;
    int32_t value_index;
    VariableItem* item;

    uint8_t value_count_device = RADIO_DEVICE_COUNT;
    item = variable_item_list_add(
        subghz->variable_item_list,
        "Module",
        value_count_device,
        subghz_scene_radio_settings_set_device,
        subghz);
    value_index = value_index_uint32(
        subghz_txrx_radio_device_get(subghz->txrx), radio_device_value, value_count_device);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, radio_device_text[value_index]);

    //Add TX Power
    item = variable_item_list_add(
        subghz->variable_item_list,
        "TX Power",
        TX_POWER_COUNT,
        subghz_scene_radio_settings_set_tx_power,
        subghz);

    value_index = subghz->tx_power;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, tx_power_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "GPS Baudrate",
        GPS_COUNT,
        subghz_scene_receiver_config_set_gps,
        subghz);
    value_index = value_index_uint32(
        subghz->last_settings->gps_baudrate,
        (const uint32_t[]){0, 9600, 19200, 38400, 57600, 115200},
        GPS_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, gps_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Protocol Names",
        ON_OFF_COUNT,
        subghz_scene_receiver_config_set_protocol_file_names,
        subghz);
    value_index = subghz->last_settings->protocol_file_names;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, on_off_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Counter Incr.",
        furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug) ? DEBUG_COUNTER_COUNT : 3,
        subghz_scene_receiver_config_set_debug_counter,
        subghz);
    value_index = value_index_int32(
        furi_hal_subghz_get_rolling_counter_mult(),
        debug_counter_val,
        furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug) ? DEBUG_COUNTER_COUNT : 3);
    furi_hal_subghz_set_rolling_counter_mult(debug_counter_val[value_index]);

    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, debug_counter_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Debug Pin",
        DEBUG_P_COUNT,
        subghz_scene_receiver_config_set_debug_pin,
        subghz);
    value_index = subghz_txrx_get_debug_pin_state(subghz->txrx);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, debug_pin_text[value_index]);
    variable_item_set_locked(
        item,
        !furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug),
        "Enable\n"
        "Settings >\n"
        "System >\n"
        "Debug");

    // Add Region selection
    item = variable_item_list_add(
        variable_item_list, "Region", REGION_COUNT, subghz_scene_radio_settings_set_region, subghz);
    const char* current_region = subghz_setting_get_region(subghz_txrx_get_setting(subghz->txrx));
    uint8_t region_index = 0;
    if(strcmp(current_region, "EU") == 0)
        region_index = 1;
    else if(strcmp(current_region, "US") == 0)
        region_index = 2;
    else if(strcmp(current_region, "JP") == 0)
        region_index = 3;
    else if(strcmp(current_region, "CUSTOM") == 0)
        region_index = 4;
    variable_item_set_current_value_index(item, region_index);
    variable_item_set_current_value_text(item, region_text[region_index]);

    // Add Extended Range toggle
    item = variable_item_list_add(
        variable_item_list,
        "Extended Range",
        EXT_RANGE_COUNT,
        subghz_scene_radio_settings_set_ext_range,
        subghz);
    value_index = furi_hal_subghz_get_extended_range() ? 1 : 0;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, ext_range_text[value_index]);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
}

bool subghz_scene_radio_settings_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    UNUSED(subghz);
    UNUSED(event);

    return false;
}

void subghz_scene_radio_settings_on_exit(void* context) {
    SubGhz* subghz = context;
    variable_item_list_set_selected_item(subghz->variable_item_list, 0);
    variable_item_list_reset(subghz->variable_item_list);
}
