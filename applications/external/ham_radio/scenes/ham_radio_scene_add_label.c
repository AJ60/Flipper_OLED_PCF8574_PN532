#include "ham_radio_scene.h"
#include "../views/ham_view_band.h"

#define TAG "HamRadioAddLabel"

/** Entries land receive-enabled: scanning needs to be able to see them, and
 * transmit stays gated on callsign plus consent regardless. */
void ham_radio_scene_add_label_callback(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    HamBandEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.label, app->label_buffer, sizeof(entry.label) - 1);
    app->label_buffer[0] = '\0';
    if(entry.label[0] == '\0') {
        snprintf(entry.label, sizeof(entry.label), "%.3f", (double)app->draft_frequency / 1000000);
    }
    entry.frequency = app->draft_frequency;
    entry.usage = HamBandUsageReceive | HamBandUsageTransmit;
    entry.module = HamBandModuleAny;

    if(!ham_band_add(&app->band, &entry)) {
        notification_message(app->notifications, &sequence_error);
        FURI_LOG_W(TAG, "Rejected frequency %lu Hz", (unsigned long)entry.frequency);
    } else {
        ham_view_band_set_plan(app->view_band, &app->band);
        ham_radio_push_rx_channels(app);
        ham_band_save(&app->band);
    }

    app->draft_in_use = false;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, HamRadioSceneBand);
}

void ham_radio_scene_add_label_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Label");
    text_input_set_minimum_length(app->text_input, 0);
    text_input_set_result_callback(
        app->text_input,
        ham_radio_scene_add_label_callback,
        app,
        app->label_buffer,
        sizeof(app->label_buffer),
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdTextInput);
}

bool ham_radio_scene_add_label_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        app->draft_in_use = false;
        return scene_manager_previous_scene(app->scene_manager);
    }

    return false;
}

void ham_radio_scene_add_label_on_exit(void* context) {
    UNUSED(context);
}
