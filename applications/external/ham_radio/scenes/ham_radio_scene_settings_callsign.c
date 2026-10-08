#include "ham_radio_scene.h"
#include "ham_radio_scene_settings_i.h"

#define TAG "HamRadioCallsign"

void ham_radio_scene_settings_callsign_callback(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    // TextInput writes straight into this buffer, so the result is already in
    // place by the time the callback runs.
    memset(app->settings.callsign, 0, sizeof(app->settings.callsign));
    strncpy(app->settings.callsign, app->callsign_buffer, sizeof(app->settings.callsign) - 1);
    app->callsign_buffer[0] = '\0';

    // Clearing the callsign disarms: with no identity there is nothing to
    // transmit against, so TX must not stay armed from a previous session.
    if(app->settings.callsign[0] == '\0') {
        app->settings.tx_enabled = false;
        app->settings.consent_accepted = false;
        FURI_LOG_W(TAG, "Callsign cleared, transmit disarmed");
    }

    FURI_LOG_I(TAG, "Callsign set to '%s'", app->settings.callsign);
    if(!ham_settings_save(&app->settings)) {
        FURI_LOG_W(TAG, "Could not persist settings");
    }

    ham_radio_scene_settings_refresh(app);
    scene_manager_previous_scene(app->scene_manager);
}

void ham_radio_scene_settings_callsign_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Your callsign");
    text_input_set_minimum_length(app->text_input, 3);
    text_input_set_result_callback(
        app->text_input,
        ham_radio_scene_settings_callsign_callback,
        app,
        app->callsign_buffer,
        sizeof(app->callsign_buffer),
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdTextInput);
}

bool ham_radio_scene_settings_callsign_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        return scene_manager_previous_scene(app->scene_manager);
    }

    return false;
}

void ham_radio_scene_settings_callsign_on_exit(void* context) {
    UNUSED(context);
}
