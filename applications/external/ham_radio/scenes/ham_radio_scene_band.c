#include "ham_radio_scene.h"
#include "../views/ham_view_band.h"
#include "../views/ham_view_tune.h"

#define TAG "HamRadioBand"

void ham_radio_scene_band_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    ham_view_band_set_plan(app->view_band, &app->band);
    ham_view_band_set_tx_state(
        app->view_band,
        ham_settings_is_tx_armed(&app->settings),
        ham_settings_duty_cycle_percent_used(&app->settings));
    ham_view_band_set_module(app->view_band, app->radio_external);

    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdBand);
}

bool ham_radio_scene_band_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case HamRadioCustomEventAddEntry:
            app->draft_in_use = false;
            scene_manager_next_scene(app->scene_manager, HamRadioSceneAddFrequency);
            return true;
        case HamRadioCustomEventTuneSelected: {
            HamBandEntry entry;
            if(!ham_view_band_get_selected_entry(app->view_band, &entry)) {
                return false;
            }
            if(entry.usage & HamBandUsageReceive) {
                ham_radio_radio_tune(app, entry.frequency);
                app->tune_frequency = entry.frequency;
                strncpy(app->tune_label, entry.label, sizeof(app->tune_label) - 1);
                ham_view_tune_set_frequency(app->view_tune, entry.frequency, entry.label);
                scene_manager_next_scene(app->scene_manager, HamRadioSceneTune);
                return true;
            }
            // A transmit-only entry has nothing to receive; refuse politely.
            notification_message(app->notifications, &sequence_single_vibro);
            return true;
        }
        case HamRadioCustomEventRemoveEntry: {
            const uint8_t index = ham_view_band_get_selected(app->view_band);
            if(ham_band_remove(&app->band, index)) {
                ham_view_band_set_plan(app->view_band, &app->band);
                ham_radio_push_rx_channels(app);
                ham_band_save(&app->band);
                FURI_LOG_D(TAG, "Removed entry %u", index);
            }
            return true;
        }
        default:
            return false;
        }
    }

    if(event.type != SceneManagerEventTypeBack) {
        return false;
    }

    // Leaving the band list parks the radio rather than leaving it streaming
    // data into a view nobody is looking at.
    ham_radio_radio_idle(app);
    return scene_manager_previous_scene(app->scene_manager);
}

void ham_radio_scene_band_on_exit(void* context) {
    UNUSED(context);
}
