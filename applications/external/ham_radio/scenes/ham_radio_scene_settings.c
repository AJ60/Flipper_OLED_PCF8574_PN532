#include "ham_radio_scene.h"
#include "ham_radio_scene_settings_i.h"
#include "../views/ham_view_band.h"

#define TAG "HamRadioSettings"

/** Power ceilings in dBm. The E07 can push past all of these, so the operator
 * picks the value their licence actually allows. */
static const uint8_t ham_settings_power_values[] = {0, 5, 10, 12, 15, 20};
static const char* const ham_settings_power_text[] = {
    "0 dBm",
    "5 dBm",
    "10 dBm",
    "12 dBm",
    "15 dBm",
    "20 dBm",
};
#define HAM_SETTINGS_POWER_COUNT \
    (sizeof(ham_settings_power_values) / sizeof(ham_settings_power_values[0]))

static const uint8_t ham_settings_duty_values[] = {1, 5, 10, 25, 50, 100};
static const char* const ham_settings_duty_text[] = {"1%", "5%", "10%", "25%", "50%", "100%"};
#define HAM_SETTINGS_DUTY_COUNT \
    (sizeof(ham_settings_duty_values) / sizeof(ham_settings_duty_values[0]))

static const char* const ham_settings_toggle_text[] = {"Off", "On"};

static uint8_t ham_settings_power_index(uint8_t dbm) {
    for(uint8_t i = 0; i < HAM_SETTINGS_POWER_COUNT; ++i) {
        if(ham_settings_power_values[i] == dbm) return i;
    }
    return 2; /* 10 dBm */
}

static uint8_t ham_settings_duty_index(uint8_t percent) {
    for(uint8_t i = 0; i < HAM_SETTINGS_DUTY_COUNT; ++i) {
        if(ham_settings_duty_values[i] == percent) return i;
    }
    return 2; /* 10% */
}

/** Rows navigate through the enter callback rather than left/right cycling, so
 * their change callback does nothing. */
static void ham_radio_scene_settings_passthrough(VariableItem* item) {
    UNUSED(item);
}

void ham_radio_scene_settings_refresh(HamRadioApp* app) {
    furi_assert(app);
    variable_item_list_reset(app->variable_item_list);

    VariableItem* item;

    item = variable_item_list_add(
        app->variable_item_list, "Callsign", 1, ham_radio_scene_settings_passthrough, app);
    variable_item_set_current_value_text(
        item, app->settings.callsign[0] != '\0' ? app->settings.callsign : "Not set");

    item = variable_item_list_add(
        app->variable_item_list,
        "TX arm",
        COUNT_OF(ham_settings_toggle_text),
        ham_radio_scene_settings_passthrough,
        app);
    variable_item_set_current_value_index(item, app->settings.tx_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, ham_settings_toggle_text[app->settings.tx_enabled]);

    item = variable_item_list_add(
        app->variable_item_list,
        "Licence ack",
        2,
        ham_radio_scene_settings_passthrough,
        app);
    variable_item_set_current_value_index(item, app->settings.consent_accepted ? 1 : 0);
    variable_item_set_current_value_text(
        item, app->settings.consent_accepted ? "Accepted" : "Required");

    item = variable_item_list_add(
        app->variable_item_list,
        "Power limit",
        HAM_SETTINGS_POWER_COUNT,
        ham_radio_scene_settings_passthrough,
        app);
    const uint8_t power_index = ham_settings_power_index(app->settings.power_limit_dbm);
    variable_item_set_current_value_index(item, power_index);
    variable_item_set_current_value_text(item, ham_settings_power_text[power_index]);

    item = variable_item_list_add(
        app->variable_item_list,
        "Duty cycle cap",
        HAM_SETTINGS_DUTY_COUNT,
        ham_radio_scene_settings_passthrough,
        app);
    const uint8_t duty_index = ham_settings_duty_index(app->settings.duty_cycle_percent);
    variable_item_set_current_value_index(item, duty_index);
    variable_item_set_current_value_text(item, ham_settings_duty_text[duty_index]);

    item = variable_item_list_add(
        app->variable_item_list, "Radio module", 1, ham_radio_scene_settings_passthrough, app);
    variable_item_set_current_value_text(
        item, app->radio_external ? "External E07" : "Internal CC1101");

    item = variable_item_list_add(
        app->variable_item_list, "Save to SD", 1, ham_radio_scene_settings_passthrough, app);
    variable_item_set_current_value_text(item, "settings");

    variable_item_list_set_enter_callback(
        app->variable_item_list, ham_radio_scene_settings_callback, app);
}

void ham_radio_scene_settings_callback(void* context, uint32_t index) {
    furi_assert(context);
    HamRadioApp* app = context;

    switch(index) {
    case HamRadioSubmenuItemCallsign:
        scene_manager_next_scene(app->scene_manager, HamRadioSceneSettingsCallsign);
        return;
    case HamRadioSubmenuItemToggleTx:
        app->settings_field = HamRadioSettingTxArm;
        scene_manager_next_scene(app->scene_manager, HamRadioSceneSettingsValue);
        return;
    case HamRadioSubmenuItemConsent:
        app->settings_field = HamRadioSettingConsent;
        scene_manager_next_scene(app->scene_manager, HamRadioSceneSettingsValue);
        return;
    case HamRadioSubmenuItemPower:
        app->settings_field = HamRadioSettingPower;
        scene_manager_next_scene(app->scene_manager, HamRadioSceneSettingsValue);
        return;
    case HamRadioSubmenuItemDuty:
        app->settings_field = HamRadioSettingDuty;
        scene_manager_next_scene(app->scene_manager, HamRadioSceneSettingsValue);
        return;
    case HamRadioSubmenuItemModule:
        // The module choice is global firmware state, owned by Sub-GHz's own
        // Radio Settings screen. This row is informational only.
        notification_message(app->notifications, &sequence_single_vibro);
        return;
    case HamRadioSubmenuSave:
        if(ham_settings_save(&app->settings)) {
            notification_message(app->notifications, &sequence_success);
        } else {
            notification_message(app->notifications, &sequence_error);
        }
        return;
    default:
        FURI_LOG_W(TAG, "Unhandled settings index %lu", (unsigned long)index);
        return;
    }
}

void ham_radio_scene_settings_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    ham_radio_scene_settings_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdVariableItemList);
}

bool ham_radio_scene_settings_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type != SceneManagerEventTypeBack) {
        return false;
    }

    // Persist on the way out: TX arming and consent are the settings most
    // likely to be changed without touching Save.
    if(!ham_settings_save(&app->settings)) {
        FURI_LOG_W(TAG, "Could not persist settings");
    }
    ham_view_band_set_tx_state(
        app->view_band,
        ham_settings_is_tx_armed(&app->settings),
        ham_settings_duty_cycle_percent_used(&app->settings));

    return scene_manager_previous_scene(app->scene_manager);
}

void ham_radio_scene_settings_on_exit(void* context) {
    UNUSED(context);
}

/* Shared value picker --------------------------------------------------- */

/** Submenu row callback for the power/duty pickers. */
static void ham_radio_scene_settings_value_callback(void* context, uint32_t index) {
    furi_assert(context);
    HamRadioApp* app = context;
    bool consumed = true;

    switch(app->settings_field) {
    case HamRadioSettingPower:
        if(index < HAM_SETTINGS_POWER_COUNT) {
            app->settings.power_limit_dbm = ham_settings_power_values[index];
        } else {
            consumed = false;
        }
        break;
    case HamRadioSettingDuty:
        if(index < HAM_SETTINGS_DUTY_COUNT) {
            app->settings.duty_cycle_percent = ham_settings_duty_values[index];
        } else {
            consumed = false;
        }
        break;
    case HamRadioSettingTxArm:
        app->settings.tx_enabled = (index != 0);
        // Arming without a callsign is meaningless, and consent is what
        // actually gates the radio path, so both are enforced here.
        if(app->settings.tx_enabled) {
            if(app->settings.callsign[0] == '\0') {
                app->settings.tx_enabled = false;
                notification_message(app->notifications, &sequence_error);
                FURI_LOG_W(TAG, "TX arm refused: no callsign set");
            } else if(!app->settings.consent_accepted) {
                notification_message(app->notifications, &sequence_single_vibro);
                FURI_LOG_W(TAG, "TX arm refused: licence acknowledgement missing");
            }
        }
        break;
    case HamRadioSettingConsent:
        app->settings.consent_accepted = (index != 0);
        if(!app->settings.consent_accepted) {
            // Withdrawing the acknowledgement also disarms.
            app->settings.tx_enabled = false;
        }
        break;
    default:
        consumed = false;
        break;
    }

    if(consumed) {
        ham_radio_scene_settings_refresh(app);
        scene_manager_previous_scene(app->scene_manager);
    }
}

void ham_radio_scene_settings_value_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    submenu_reset(app->submenu);

    switch(app->settings_field) {
    case HamRadioSettingPower:
        submenu_set_header(app->submenu, "Power limit");
        for(uint8_t i = 0; i < HAM_SETTINGS_POWER_COUNT; ++i) {
            submenu_add_item(
                app->submenu,
                ham_settings_power_text[i],
                i,
                ham_radio_scene_settings_value_callback,
                app);
        }
        submenu_set_selected_item(
            app->submenu, ham_settings_power_index(app->settings.power_limit_dbm));
        break;

    case HamRadioSettingDuty:
        submenu_set_header(app->submenu, "Duty cycle cap");
        for(uint8_t i = 0; i < HAM_SETTINGS_DUTY_COUNT; ++i) {
            submenu_add_item(
                app->submenu,
                ham_settings_duty_text[i],
                i,
                ham_radio_scene_settings_value_callback,
                app);
        }
        submenu_set_selected_item(
            app->submenu, ham_settings_duty_index(app->settings.duty_cycle_percent));
        break;

    case HamRadioSettingTxArm:
        submenu_set_header(app->submenu, "Arm transmit");
        submenu_add_item(app->submenu, "Off", 0, ham_radio_scene_settings_value_callback, app);
        submenu_add_item(app->submenu, "On", 1, ham_radio_scene_settings_value_callback, app);
        submenu_set_selected_item(app->submenu, app->settings.tx_enabled ? 1 : 0);
        break;

    case HamRadioSettingConsent:
        submenu_set_header(app->submenu, "Licence ack");
        submenu_add_item(
            app->submenu, "Not accepted", 0, ham_radio_scene_settings_value_callback, app);
        submenu_add_item(
            app->submenu, "I hold a licence", 1, ham_radio_scene_settings_value_callback, app);
        submenu_set_selected_item(app->submenu, app->settings.consent_accepted ? 1 : 0);
        break;

    default:
        scene_manager_next_scene(app->scene_manager, HamRadioSceneSettings);
        return;
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdSubmenu);
}

bool ham_radio_scene_settings_value_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        return scene_manager_previous_scene(app->scene_manager);
    }

    return false;
}

void ham_radio_scene_settings_value_on_exit(void* context) {
    UNUSED(context);
}
