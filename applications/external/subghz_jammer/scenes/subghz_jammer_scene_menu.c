/*
 * Copyright (c) 2025 Flipper Devices
 * SPDX-License-Identifier: Apache-2.0
 */

#include <furi.h>
#include <furi_hal_subghz.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <dialogs/dialogs.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

/* ------------------------------------------------------------------ */
/*  Preset tables                                                     */
/* ------------------------------------------------------------------ */

static const char* const mode_names[] = {
    "Carrier",
    "Noise",
    "Sweep",
    "PingPong",
    "Pulse",
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

static const uint32_t power_presets[] = {0, 10, 25, 50, 75, 100};
static const char* const power_names[] = {
    "Auto",
    "10%",
    "25%",
    "50%",
    "75%",
    "100%",
};
#define POWER_COUNT (sizeof(power_presets) / sizeof(power_presets[0]))

static const uint32_t max_duration_presets[] = {
    0,      // Infinite
    30000,  // 30 s
    60000,  // 1 min
    300000, // 5 min
    600000, // 10 min
};
static const char* const max_duration_names[] = {
    "Infinite",
    "30 s",
    "1 min",
    "5 min",
    "10 min",
};
#define MAX_DURATION_COUNT (sizeof(max_duration_presets) / sizeof(max_duration_presets[0]))

/* --- Advanced scheduling editors --- */
static const uint32_t sweep_step_presets[] = {1000, 5000, 10000, 25000, 50000, 100000, 250000, 500000};
static const char* const sweep_step_names[] = {"1 kHz", "5 kHz", "10 kHz", "25 kHz", "50 kHz", "100 kHz", "250 kHz", "500 kHz"};
#define SWEEP_STEP_COUNT (sizeof(sweep_step_presets) / sizeof(sweep_step_presets[0]))

static const uint32_t noise_step_presets[] = {1000, 5000, 10000, 25000, 50000, 100000, 250000, 500000};
static const char* const noise_step_names[] = {"1 kHz", "5 kHz", "10 kHz", "25 kHz", "50 kHz", "100 kHz", "250 kHz", "500 kHz"};
#define NOISE_STEP_COUNT (sizeof(noise_step_presets) / sizeof(noise_step_presets[0]))

static const uint32_t hop_interval_presets[] = {5, 10, 20, 50, 100, 200, 500, 1000};
static const char* const hop_interval_names[] = {"5 ms", "10 ms", "20 ms", "50 ms", "100 ms", "200 ms", "500 ms", "1 s"};
#define HOP_INTERVAL_COUNT (sizeof(hop_interval_presets) / sizeof(hop_interval_presets[0]))

static const uint32_t dwell_hold_presets[] = {10, 20, 50, 100, 200, 500, 1000, 2000};
static const char* const dwell_hold_names[] = {"10 ms", "20 ms", "50 ms", "100 ms", "200 ms", "500 ms", "1 s", "2 s"};
#define DWELL_HOLD_COUNT (sizeof(dwell_hold_presets) / sizeof(dwell_hold_presets[0]))

static const char* const tx_gate_names[] = {"Continuous", "Gate On Input", "Pulse Gate"};
#define TX_GATE_COUNT 3

/* ------------------------------------------------------------------ */
/*  Change handlers                                                     */
/* ------------------------------------------------------------------ */

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

static void menu_pulse_ratio_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > 100) index = 100;
    jammer->settings.pulse_ratio = index;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u%%", index);
    variable_item_set_current_value_text(item, buf);
}

static void menu_power_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= POWER_COUNT) index = POWER_COUNT - 1;
    jammer->settings.power_level = power_presets[index];
    variable_item_set_current_value_text(item, power_names[index]);
}

static void menu_max_duration_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= MAX_DURATION_COUNT) index = MAX_DURATION_COUNT - 1;
    jammer->settings.max_tx_duration_ms = max_duration_presets[index];
    variable_item_set_current_value_text(item, max_duration_names[index]);
}

static void menu_bypass_region_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    jammer->settings.bypass_region = !jammer->settings.bypass_region;
    variable_item_set_current_value_text(
        item, jammer->settings.bypass_region ? "Bypass" : "EU Gate");
}

static void menu_sweep_step_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= SWEEP_STEP_COUNT) index = (uint8_t)(SWEEP_STEP_COUNT - 1);
    jammer->settings.sweep_step_hz = sweep_step_presets[index];
    variable_item_set_current_value_text(item, sweep_step_names[index]);
}

static void menu_noise_step_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= NOISE_STEP_COUNT) index = (uint8_t)(NOISE_STEP_COUNT - 1);
    jammer->settings.noise_step_hz = noise_step_presets[index];
    variable_item_set_current_value_text(item, noise_step_names[index]);
}

static void menu_hop_interval_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= HOP_INTERVAL_COUNT) index = (uint8_t)(HOP_INTERVAL_COUNT - 1);
    jammer->settings.hop_interval_ms = hop_interval_presets[index];
    variable_item_set_current_value_text(item, hop_interval_names[index]);
}

static void menu_dwell_hold_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= DWELL_HOLD_COUNT) index = (uint8_t)(DWELL_HOLD_COUNT - 1);
    jammer->settings.dwell_hold_ms = dwell_hold_presets[index];
    variable_item_set_current_value_text(item, dwell_hold_names[index]);
}

static void menu_tx_gate_change(VariableItem* item) {
    Jammer* jammer = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= TX_GATE_COUNT) index = (uint8_t)(TX_GATE_COUNT - 1);
    jammer->settings.tx_gate_mode = (uint8_t)index;
    variable_item_set_current_value_text(item, tx_gate_names[index]);
}

/* ------------------------------------------------------------------ */
/*  Hierarchical menu (level stack on a single VariableItemList)      */
/* ------------------------------------------------------------------ */

typedef enum {
    MenuLevelTop = 0,
    MenuLevelConfigure,
    MenuLevelAdvanced,
    MenuLevelTrigger,
} MenuLevel;

static int g_level_stack[8];
static int g_level_count;
static uint32_t g_pending_enter_index;

static void menu_enter_callback(void* context, uint32_t index);

static void menu_build_top(Jammer* jammer, VariableItemList* list) {
    (void)jammer;
    variable_item_list_reset(list);
    variable_item_list_add(list, "Configure", 0, NULL, NULL);
    variable_item_list_add(list, "Trigger Mode", 0, NULL, NULL);
    variable_item_list_add(list, "Advanced", 0, NULL, NULL);
    variable_item_list_add(list, "Edit Frequency", 0, NULL, NULL);
    variable_item_list_add(list, "[ START JAMMING ]", 0, NULL, NULL);
}

static void menu_build_configure(Jammer* jammer, VariableItemList* list) {
    variable_item_list_reset(list);
    VariableItem* item;

    item = variable_item_list_add(list, "Mode", MODE_COUNT, menu_mode_change, jammer);
    uint8_t mode_idx = (jammer->settings.mode < MODE_COUNT) ? (uint8_t)jammer->settings.mode : 0;
    variable_item_set_current_value_index(item, mode_idx);
    variable_item_set_current_value_text(item, mode_names[mode_idx]);

    item = variable_item_list_add(list, "Band", BAND_COUNT, menu_band_change, jammer);
    uint8_t band_idx = 1;
    for(size_t i = 0; i < BAND_COUNT; i++) {
        if(band_presets[i].min == jammer->settings.band_min &&
           band_presets[i].max == jammer->settings.band_max) {
            band_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, band_idx);
    variable_item_set_current_value_text(item, band_presets[band_idx].name);

    item = variable_item_list_add(list, "Frequency", FREQ_COUNT, menu_freq_change, jammer);
    uint8_t freq_idx = 1;
    for(size_t i = 0; i < FREQ_COUNT; i++) {
        if(frequency_presets[i] == jammer->settings.frequency) {
            freq_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, freq_idx);
    variable_item_set_current_value_text(item, frequency_names[freq_idx]);

    item = variable_item_list_add(list, "Radio", RADIO_COUNT, menu_radio_change, jammer);
    uint8_t radio_idx = (jammer->settings.radio < RADIO_COUNT) ? (uint8_t)jammer->settings.radio : 0;
    variable_item_set_current_value_index(item, radio_idx);
    variable_item_set_current_value_text(item, radio_names[radio_idx]);

    item = variable_item_list_add(list, "Hop Count", HOP_COUNT, menu_hop_change, jammer);
    uint8_t hop_idx = 2;
    for(size_t i = 0; i < HOP_COUNT; i++) {
        if(hop_presets[i] == jammer->settings.hop_count) {
            hop_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, hop_idx);
    variable_item_set_current_value_text(item, hop_names[hop_idx]);

    item = variable_item_list_add(list, "Dwell", DWELL_COUNT, menu_dwell_change, jammer);
    uint8_t dwell_idx = 2;
    for(size_t i = 0; i < DWELL_COUNT; i++) {
        if(dwell_presets[i] == jammer->settings.dwell_ms) {
            dwell_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, dwell_idx);
    variable_item_set_current_value_text(item, dwell_names[dwell_idx]);

    item = variable_item_list_add(list, "Power", POWER_COUNT, menu_power_change, jammer);
    uint8_t power_idx = 0;
    for(size_t i = 0; i < POWER_COUNT; i++) {
        if(power_presets[i] == jammer->settings.power_level) {
            power_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, power_idx);
    variable_item_set_current_value_text(item, power_names[power_idx]);
}

static void menu_build_advanced(Jammer* jammer, VariableItemList* list) {
    variable_item_list_reset(list);
    VariableItem* item;

    item = variable_item_list_add(list, "Sweep Step", SWEEP_STEP_COUNT, menu_sweep_step_change, jammer);
    uint8_t s_idx = 0;
    for(size_t i = 0; i < SWEEP_STEP_COUNT; i++) {
        if(sweep_step_presets[i] == jammer->settings.sweep_step_hz) {
            s_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, s_idx);
    variable_item_set_current_value_text(item, sweep_step_names[s_idx]);

    item = variable_item_list_add(list, "Noise Step", NOISE_STEP_COUNT, menu_noise_step_change, jammer);
    uint8_t n_idx = 0;
    for(size_t i = 0; i < NOISE_STEP_COUNT; i++) {
        if(noise_step_presets[i] == jammer->settings.noise_step_hz) {
            n_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, n_idx);
    variable_item_set_current_value_text(item, noise_step_names[n_idx]);

    item = variable_item_list_add(list, "Hop Interval", HOP_INTERVAL_COUNT, menu_hop_interval_change, jammer);
    uint8_t hi_idx = 0;
    for(size_t i = 0; i < HOP_INTERVAL_COUNT; i++) {
        if(hop_interval_presets[i] == jammer->settings.hop_interval_ms) {
            hi_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, hi_idx);
    variable_item_set_current_value_text(item, hop_interval_names[hi_idx]);

    item = variable_item_list_add(list, "Dwell Hold", DWELL_HOLD_COUNT, menu_dwell_hold_change, jammer);
    uint8_t dh_idx = 0;
    for(size_t i = 0; i < DWELL_HOLD_COUNT; i++) {
        if(dwell_hold_presets[i] == jammer->settings.dwell_hold_ms) {
            dh_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, dh_idx);
    variable_item_set_current_value_text(item, dwell_hold_names[dh_idx]);

    item = variable_item_list_add(list, "Bypass Region", 2, menu_bypass_region_change, jammer);
    variable_item_set_current_value_index(item, jammer->settings.bypass_region ? 1 : 0);
    variable_item_set_current_value_text(item, jammer->settings.bypass_region ? "Bypass" : "EU Gate");

    item = variable_item_list_add(list, "Max TX Dur", MAX_DURATION_COUNT, menu_max_duration_change, jammer);
    uint8_t m_idx = 0;
    for(size_t i = 0; i < MAX_DURATION_COUNT; i++) {
        if(max_duration_presets[i] == jammer->settings.max_tx_duration_ms) {
            m_idx = (uint8_t)i;
            break;
        }
    }
    variable_item_set_current_value_index(item, m_idx);
    variable_item_set_current_value_text(item, max_duration_names[m_idx]);

    item = variable_item_list_add(list, "TX Gate", TX_GATE_COUNT, menu_tx_gate_change, jammer);
    uint8_t g_idx = jammer->settings.tx_gate_mode < TX_GATE_COUNT ? (uint8_t)jammer->settings.tx_gate_mode : 0;
    variable_item_set_current_value_index(item, g_idx);
    variable_item_set_current_value_text(item, tx_gate_names[g_idx]);

    item = variable_item_list_add(list, "Pulse Ratio", 101, menu_pulse_ratio_change, jammer);
    uint8_t p_idx = jammer->settings.pulse_ratio > 100 ? 100 : (uint8_t)jammer->settings.pulse_ratio;
    variable_item_set_current_value_index(item, p_idx);
    char p_buf[16];
    snprintf(p_buf, sizeof(p_buf), "%u%%", p_idx);
    variable_item_set_current_value_text(item, p_buf);
}

static void menu_build_trigger(Jammer* jammer, VariableItemList* list) {
    variable_item_list_reset(list);
    VariableItem* item = variable_item_list_add(list, "Latch", LATCH_COUNT, menu_latch_change, jammer);
    uint8_t latch_idx = jammer->settings.latch ? 0 : 1;
    variable_item_set_current_value_index(item, latch_idx);
    variable_item_set_current_value_text(item, latch_names[latch_idx]);
}

static void menu_build_current(Jammer* jammer, VariableItemList* list) {
    int level = g_level_stack[g_level_count - 1];
    switch(level) {
    case MenuLevelTop:
        menu_build_top(jammer, list);
        break;
    case MenuLevelConfigure:
        menu_build_configure(jammer, list);
        break;
    case MenuLevelAdvanced:
        menu_build_advanced(jammer, list);
        break;
    case MenuLevelTrigger:
        menu_build_trigger(jammer, list);
        break;
    }
    variable_item_list_set_enter_callback(list, menu_enter_callback, jammer);
}

/* Rebuild the current level's list. variable_item_list_add() commits the view
 * model, so the list auto-redraws; reset the selection to the top so a fresh
 * submenu (or a return to a parent level) starts on its first item. */
static void menu_refresh(Jammer* jammer) {
    menu_build_current(jammer, jammer->var_item_list);
    variable_item_list_set_selected_item(jammer->var_item_list, 0);
}

static void menu_enter_callback(void* context, uint32_t index) {
    Jammer* jammer = (Jammer*)context;
    g_pending_enter_index = index;
    view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventMenuEnter);
}

/* ------------------------------------------------------------------ */
/*  Scene                                                             */
/* ------------------------------------------------------------------ */

void subghz_jammer_scene_menu_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    VariableItemList* var_list = jammer->var_item_list;
    variable_item_list_reset(var_list);
    variable_item_list_set_enter_callback(var_list, menu_enter_callback, jammer);

    g_level_count = 1;
    g_level_stack[0] = MenuLevelTop;
    menu_build_current(jammer, var_list);

    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewMenu);
}

bool subghz_jammer_scene_menu_on_event(void* context, SceneManagerEvent event) {
    Jammer* jammer = (Jammer*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(g_level_count > 1) {
            g_level_count--;
            menu_refresh(jammer);
            consumed = true;
        }
        return consumed;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == SubGhzJammerEventMenuEnter) {
        uint32_t index = g_pending_enter_index;
        int level = g_level_stack[g_level_count - 1];

        if(level == MenuLevelTop) {
            switch(index) {
            case 0: /* Configure */
                g_level_stack[g_level_count++] = MenuLevelConfigure;
                menu_refresh(jammer);
                consumed = true;
                break;
            case 1: /* Trigger Mode */
                g_level_stack[g_level_count++] = MenuLevelTrigger;
                menu_refresh(jammer);
                consumed = true;
                break;
            case 2: /* Advanced */
                g_level_stack[g_level_count++] = MenuLevelAdvanced;
                menu_refresh(jammer);
                consumed = true;
                break;
            case 3: /* Edit Frequency */
                scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenefreq_edit);
                consumed = true;
                break;
            case 4: /* Start */
            {
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
                } else {
                    scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenestart);
                }
                consumed = true;
                break;
            }
            }
        }
    }

    return consumed;
}

void subghz_jammer_scene_menu_on_exit(void* context) {
    Jammer* jammer = (Jammer*)context;
    variable_item_list_reset(jammer->var_item_list);
    g_level_count = 0;
}
