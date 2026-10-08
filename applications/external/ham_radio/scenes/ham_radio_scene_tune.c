#include "ham_radio_scene.h"
#include "../views/ham_view_tune.h"

/** Poll the receiver on the app tick so the meter moves without a thread. */
bool ham_radio_scene_tune_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;
    bool consumed = true;

    if(event.type == SceneManagerEventTypeTick) {
        ham_view_tune_set_level(
            app->view_tune, ham_radio_radio_rssi(app), ham_radio_radio_lqi(app));

        const char* blocked = ham_radio_tx_block_reason(app, app->tune_frequency);
        ham_view_tune_set_tx_state(app->view_tune, blocked == NULL, blocked);
        return consumed;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == HamRadioCustomEventTuneOk) {
            // Transmit is not implemented in this build: the guard rails are in
            // place and the reason is shown, but nothing goes on air yet.
            notification_message(app->notifications, &sequence_error);
            return true;
        }
        return false;
    }

    if(event.type != SceneManagerEventTypeBack) {
        return false;
    }

    ham_radio_radio_idle(app);
    return scene_manager_previous_scene(app->scene_manager);
}

void ham_radio_scene_tune_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    ham_radio_radio_tune(app, app->tune_frequency);
    ham_view_tune_set_frequency(app->view_tune, app->tune_frequency, app->tune_label);
    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdTune);
}

void ham_radio_scene_tune_on_exit(void* context) {
    UNUSED(context);
}
