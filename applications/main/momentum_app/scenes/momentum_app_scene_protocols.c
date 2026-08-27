#include "../momentum_app.h"

enum VarItemListIndex {
    VarItemListIndexSubghzFreqs,
    VarItemListIndexSubghzBypass,
    VarItemListIndexSubghzExtend,
    VarItemListIndexSubghzCalib,
    VarItemListIndexGpioPins,
    VarItemListIndexFileNamingPrefix,
};

void momentum_app_scene_protocols_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void momentum_app_scene_protocols_subghz_bypass_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    view_dispatcher_send_custom_event(app->view_dispatcher, VarItemListIndexSubghzBypass);
}

static void momentum_app_scene_protocols_subghz_extend_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    view_dispatcher_send_custom_event(app->view_dispatcher, VarItemListIndexSubghzExtend);
}

static void momentum_app_scene_protocols_file_naming_prefix_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "After" : "Before");
    momentum_settings.file_naming_prefix_after = value;
    app->save_settings = true;
}

static void momentum_app_scene_protocols_subghz_calib_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint32_t index = variable_item_get_current_value_index(item);

    if(index == 201) {
        // Auto mode: re-enable hardware auto-calibration on IDLE->TX.
        // Also reset ppm correction to 0 so firmware applies no frequency offset.
        variable_item_set_current_value_text(item, "Auto");
        momentum_settings.subghz_autocal = true;
        momentum_settings.subghz_calib = 0;
    } else {
        int32_t ppm = ((int32_t)index * 20) - 2000;
        char calib_str[32];
        int32_t khz_val = ppm * 43392;
        int32_t khz_int = khz_val / 100000;
        int32_t khz_frac = khz_val % 100000;
        if(khz_frac < 0) khz_frac = -khz_frac;
        khz_frac /= 10000;
        snprintf(
            calib_str,
            sizeof(calib_str),
            "%+ld ppm (%+ld.%ld kHz)",
            (long)ppm,
            (long)khz_int,
            (long)khz_frac);
        variable_item_set_current_value_text(item, calib_str);
        momentum_settings.subghz_calib = ppm;
        momentum_settings.subghz_autocal = false;
    }
    app->save_settings = true;
}

void momentum_app_scene_protocols_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(var_item_list, "SubGHz Freqs", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(
        var_item_list,
        "SubGHz Bypass Region Lock",
        2,
        momentum_app_scene_protocols_subghz_bypass_changed,
        app);
    variable_item_set_current_value_index(item, app->subghz_bypass);
    variable_item_set_current_value_text(item, app->subghz_bypass ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "SubGHz Extend Freq Bands",
        2,
        momentum_app_scene_protocols_subghz_extend_changed,
        app);
    variable_item_set_current_value_index(item, app->subghz_extend);
    variable_item_set_current_value_text(item, app->subghz_extend ? "ON" : "OFF");
    variable_item_set_locked(item, !app->subghz_bypass, "Must bypass\nregion lock\nfirst!");

    item = variable_item_list_add(
        var_item_list,
        "SubGHz Calib",
        202, // indices 0..200 = -2000..+2000 ppm (step 20), index 201 = Auto
        momentum_app_scene_protocols_subghz_calib_changed,
        app);
    if(momentum_settings.subghz_autocal) {
        variable_item_set_current_value_index(item, 201);
        variable_item_set_current_value_text(item, "Auto");
    } else {
        int32_t calib_val = momentum_settings.subghz_calib;
        if(calib_val < -2000) calib_val = -2000;
        if(calib_val > 2000) calib_val = 2000;
        uint32_t calib_idx = (uint32_t)((calib_val + 2000) / 20);
        variable_item_set_current_value_index(item, calib_idx);
        char calib_str[32];
        int32_t ppm = ((int32_t)calib_idx * 20) - 2000;
        int32_t khz_val = ppm * 43392;
        int32_t khz_int = khz_val / 100000;
        int32_t khz_frac = khz_val % 100000;
        if(khz_frac < 0) khz_frac = -khz_frac;
        khz_frac /= 10000;
        snprintf(
            calib_str,
            sizeof(calib_str),
            "%+ld ppm (%+ld.%ld kHz)",
            (long)ppm,
            (long)khz_int,
            (long)khz_frac);
        variable_item_set_current_value_text(item, calib_str);
    }

    item = variable_item_list_add(var_item_list, "GPIO Pins", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(
        var_item_list,
        "File Naming Prefix",
        2,
        momentum_app_scene_protocols_file_naming_prefix_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.file_naming_prefix_after);
    variable_item_set_current_value_text(
        item, momentum_settings.file_naming_prefix_after ? "After" : "Before");

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_protocols_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneProtocols));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_protocols_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneProtocols, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexSubghzFreqs:
            scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneProtocolsFreqs, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneProtocolsFreqs);
            break;
        case VarItemListIndexSubghzBypass:
        case VarItemListIndexSubghzExtend: {
            bool* setting = (event.event == VarItemListIndexSubghzBypass) ? &app->subghz_bypass :
                                                                            &app->subghz_extend;
            VariableItem* item = variable_item_list_get(app->var_item_list, event.event);
            bool value = variable_item_get_current_value_index(item);
            if(value == *setting) value = !value; // Invoked via click
            bool change = !value; // Change without confirm if going from ON to OFF
            if(value) {
                DialogMessage* msg = dialog_message_alloc();
                dialog_message_set_header(msg, "Are you sure?", 64, 4, AlignCenter, AlignTop);
                dialog_message_set_buttons(msg, "No", NULL, "Yes");
                dialog_message_set_text(
                    msg,
                    (event.event == VarItemListIndexSubghzBypass) ? "Unlocks TX to 300-350,\n"
                                                                    "387-467, 779-928 MHz\n"
                                                                    "Use responsibly, check\n"
                                                                    "local laws" :
                                                                    "Extends TX to 281-361,\n"
                                                                    "378-481, 749-962 MHz\n"
                                                                    "Use at own risk, may\n"
                                                                    "damage Flipper",
                    64,
                    36,
                    AlignCenter,
                    AlignCenter);
                if(dialog_message_show(app->dialogs, msg) == DialogMessageButtonRight) {
                    change = true;
                }
                dialog_message_free(msg);
            }
            if(change) {
                *setting = value;
                app->save_subghz = true;
                app->require_reboot = true;
                if(event.event == VarItemListIndexSubghzBypass) {
                    variable_item_set_locked(
                        variable_item_list_get(app->var_item_list, VarItemListIndexSubghzExtend),
                        !value,
                        NULL);
                }
            } else {
                value = !value;
            }
            variable_item_set_current_value_index(item, value);
            variable_item_set_current_value_text(item, value ? "ON" : "OFF");
            break;
        }
        case VarItemListIndexGpioPins:
            scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneProtocolsGpio, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneProtocolsGpio);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_protocols_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
