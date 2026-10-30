#include <furi.h>
#include <furi_hal_subghz.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <dialogs/dialogs.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

static const char* const mode_names[] = {
    "Carrier",
    "Noise",
    "Sweep",
    "PingPong",
};
#define MODE_COUNT (sizeof(mode_names) / sizeof(mode_names[0]))

static const uint32_t frequency_presets[] = {
    315000000u,
    433920000u,
    433980000u,
    868350000u,
    915000000u,
};
static const char* const frequency_names[] = {
    "315.00 MHz",
    "433.92 MHz",
    "433.98 MHz",
    "868.35 MHz",
    "915.00 MHz",
};
#define FREQ_COUNT (sizeof(frequency_presets) / sizeof(frequency_presets[0]))

static const struct {
    uint32_t min;
    uint32_t max;
    uint32_t default_freq;
    const char* name;
} band_presets[] = {
    {300000000u, 348000000u, 315000000u, "300-348 MHz"},
    {387000000u, 464000000u, 433920000u, "387-464 MHz"},
    {779000000u, 928000000u, 868350000u, "779-928 MHz"},
};
#define BAND_COUNT (sizeof(band_presets) / sizeof(band_presets[0]))

static const char* const radio_names[] = {
    "Auto",
    "Internal",
    "External",
};
#define RADIO_COUNT (sizeof(radio_names) / sizeof(radio_names[0]))

static const uint16_t hop_presets[] = {2, 4, 8, 16};
static const char* const hop_names[] = {"2", "4", "8", "16"};
#define HOP_COUNT (sizeof(hop_presets) / sizeof(hop_presets[0]))

static const uint16_t dwell_presets[] = {100, 250, 500, 1000, 2000, 5000};
static const char* const dwell_names[] = {"100 ms", "250 ms", "500 ms", "1.0 s", "2.0 s", "5.0 s"};
#define DWELL_COUNT (sizeof(dwell_presets) / sizeof(dwell_presets[0]))

static const char* const latch_names[] = {
    "Latch",
    "Hold OK",
};
#define LATCH_COUNT (sizeof(latch_names) / sizeof(latch_names[0]))

enum MenuItemIndex {
    MenuItemMode,
    MenuItemFrequency,
    MenuItemBand,
    MenuItemRadio,
    MenuItemHop,
    MenuItemDwell,
    MenuItemLatch,
    MenuItemStart,
};

static void menu_mode_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    jammer->settings.mode = (JammerSignalMode)index;
    variable_item_set_current_value_text(item, mode_names[index]);
}

static void menu_freq_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    jammer->settings.frequency = frequency_presets[index];
    variable_item_set_current_value_text(item, frequency_names[index]);
}

static void menu_band_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    jammer->settings.band_min = band_presets[index].min;
    jammer->settings.band_max = band_presets[index].max;
    jammer->settings.frequency = band_presets[index].default_freq;
    variable_item_set_current_value_text(item, band_presets[index].name);
}

static void menu_radio_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    jammer->settings.radio = (JammerRadioChoice)index;
    variable_item_set_current_value_text(item, radio_names[index]);
}

static void menu_hop_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    jammer->settings.hop_count = hop_presets[index];
    variable_item_set_current_value_text(item, hop_names[index]);
}

static void menu_dwell_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    jammer->settings.dwell_ms = dwell_presets[index];
    variable_item_set_current_value_text(item, dwell_names[index]);
}

static void menu_latch_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    jammer->settings.latch = (index == 0);
    variable_item_set_current_value_text(item, latch_names[index]);
}

static void menu_enter_callback(void* context, uint32_t index) {
    Jammer* jammer = (Jammer*)context;
    if(index == MenuItemStart) {
        view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventStart);
    }
}

void subghz_jammer_scene_menu_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    VariableItemList* var_list = jammer->var_item_list;
    variable_item_list_reset(var_list);

    VariableItem* item;

    // 1. Mode
    item = variable_item_list_add(var_list, "Mode", MODE_COUNT, menu_mode_change, jammer);
    uint8_t mode_idx = (jammer->settings.mode < MODE_COUNT) ? jammer->settings.mode : 0;
    variable_item_set_current_value_index(item, mode_idx);
    variable_item_set_current_value_text(item, mode_names[mode_idx]);

    // 2. Frequency
    item = variable_item_list_add(var_list, "Frequency", FREQ_COUNT, menu_freq_change, jammer);
    uint8_t freq_idx = 1; // Default 433.92
    for(size_t i = 0; i < FREQ_COUNT; i++) {
        if(frequency_presets[i] == jammer->settings.frequency) {
            freq_idx = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, freq_idx);
    variable_item_set_current_value_text(item, frequency_names[freq_idx]);

    // 3. Band
    item = variable_item_list_add(var_list, "Band", BAND_COUNT, menu_band_change, jammer);
    uint8_t band_idx = 1; // Default 387-464
    for(size_t i = 0; i < BAND_COUNT; i++) {
        if(band_presets[i].min == jammer->settings.band_min &&
           band_presets[i].max == jammer->settings.band_max) {
            band_idx = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, band_idx);
    variable_item_set_current_value_text(item, band_presets[band_idx].name);

    // 4. Radio
    item = variable_item_list_add(var_list, "Radio", RADIO_COUNT, menu_radio_change, jammer);
    uint8_t radio_idx = (jammer->settings.radio < RADIO_COUNT) ? jammer->settings.radio : 0;
    variable_item_set_current_value_index(item, radio_idx);
    variable_item_set_current_value_text(item, radio_names[radio_idx]);

    // 5. Hop Count
    item = variable_item_list_add(var_list, "Hop Count", HOP_COUNT, menu_hop_change, jammer);
    uint8_t hop_idx = 2; // Default 8
    for(size_t i = 0; i < HOP_COUNT; i++) {
        if(hop_presets[i] == jammer->settings.hop_count) {
            hop_idx = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, hop_idx);
    variable_item_set_current_value_text(item, hop_names[hop_idx]);

    // 6. Dwell
    item = variable_item_list_add(var_list, "Dwell", DWELL_COUNT, menu_dwell_change, jammer);
    uint8_t dwell_idx = 2; // Default 500ms
    for(size_t i = 0; i < DWELL_COUNT; i++) {
        if(dwell_presets[i] == jammer->settings.dwell_ms) {
            dwell_idx = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, dwell_idx);
    variable_item_set_current_value_text(item, dwell_names[dwell_idx]);

    // 7. Latch
    item = variable_item_list_add(var_list, "Trigger", LATCH_COUNT, menu_latch_change, jammer);
    uint8_t latch_idx = jammer->settings.latch ? 0 : 1;
    variable_item_set_current_value_index(item, latch_idx);
    variable_item_set_current_value_text(item, latch_names[latch_idx]);

    // 8. START JAMMING action item
    variable_item_list_add(var_list, "[ START JAMMING ]", 0, NULL, NULL);

    variable_item_list_set_enter_callback(var_list, menu_enter_callback, jammer);
    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewMenu);
}

bool subghz_jammer_scene_menu_on_event(void* context, SceneManagerEvent event) {
    Jammer* jammer = (Jammer*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzJammerEventStart) {
            SubGhzTx can_tx = furi_hal_subghz_check_tx(jammer->settings.frequency);
            if(can_tx != SubGhzTxAllowed) {
                DialogMessage* message = dialog_message_alloc();
                const char* header = "TX Blocked";
                const char* text = "Frequency blocked\nby region regulation.\nSettings -> Protocols\n-> Bypass Region";
                if(can_tx == SubGhzTxUnsupported) {
                    header = "Unsupported Freq";
                    text = "Frequency is outside\nhardware CC1101 range.";
                } else if(can_tx == SubGhzTxBlockedDefault) {
                    text = "Frequency outside\ndefault range.\nSettings -> Protocols\n-> Extend Bands";
                }
                dialog_message_set_header(message, header, 64, 3, AlignCenter, AlignTop);
                dialog_message_set_text(message, text, 3, 20, AlignLeft, AlignTop);
                dialog_message_show(jammer->dialogs, message);
                dialog_message_free(message);
                consumed = true;
            } else {
                scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenestart);
                consumed = true;
            }
        }
    }

    return consumed;
}

void subghz_jammer_scene_menu_on_exit(void* context) {
    Jammer* jammer = (Jammer*)context;
    variable_item_list_reset(jammer->var_item_list);
}
