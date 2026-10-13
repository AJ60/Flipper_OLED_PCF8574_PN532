#include "ham_radio_scene.h"
#include "../views/ham_view_scan.h"
#include "../views/ham_view_tune.h"

/** Sweeps complete on the worker thread, so the result is parked in the app
 * struct and only applied to the view once the GUI thread picks it up. */
typedef struct {
    FuriMutex* mutex;
    HamScanResult pending;
    bool has_pending;
} HamScanBridge;

static HamScanBridge ham_radio_scene_scan_bridge;

void ham_radio_scene_scan_sweep_callback(const HamScanResult* result, void* context) {
    furi_assert(context);
    HamRadioApp* app = context;
    HamScanBridge* bridge = &ham_radio_scene_scan_bridge;
    if(!bridge->mutex) return;

    furi_check(furi_mutex_acquire(bridge->mutex, FuriWaitForever) == FuriStatusOk);
    bridge->pending = *result;
    bridge->has_pending = true;
    furi_mutex_release(bridge->mutex);

    view_dispatcher_send_custom_event(app->view_dispatcher, HamRadioCustomEventSweepDone);
}

bool ham_radio_scene_scan_take_pending(HamScanResult* out) {
    HamScanBridge* bridge = &ham_radio_scene_scan_bridge;
    bool taken = false;

    furi_check(furi_mutex_acquire(bridge->mutex, FuriWaitForever) == FuriStatusOk);
    if(bridge->has_pending) {
        *out = bridge->pending;
        bridge->has_pending = false;
        taken = true;
    }
    furi_mutex_release(bridge->mutex);

    return taken;
}

void ham_radio_scene_scan_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    HamScanBridge* bridge = &ham_radio_scene_scan_bridge;
    if(bridge->mutex == NULL) {
        bridge->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    }
    bridge->has_pending = false;

    ham_scan_worker_set_callback(app->scan_worker, ham_radio_scene_scan_sweep_callback, app);

    app->sweep_running = true;
    ham_scan_worker_set_enabled(app->scan_worker, true);

    ham_view_scan_set_module(app->view_scan, app->radio_external);
    ham_view_scan_set_running(app->view_scan, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdScan);
}

bool ham_radio_scene_scan_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case HamRadioCustomEventSweepDone: {
            HamScanResult result;
            if(ham_radio_scene_scan_take_pending(&result)) {
                ham_view_scan_set_result(app->view_scan, &result);
            }
            return true;
        }

        case HamRadioCustomEventSweepToggle:
            app->sweep_running = !app->sweep_running;
            ham_scan_worker_set_enabled(app->scan_worker, app->sweep_running);
            ham_view_scan_set_running(app->view_scan, app->sweep_running);
            return true;

        case HamRadioCustomEventTuneSelected: {
            // Drop into the strongest channel so the S-meter starts somewhere
            // useful rather than at an arbitrary entry.
            uint8_t peak = ham_view_scan_get_peak(app->view_scan);
            uint32_t frequency = ham_view_scan_get_frequency(app->view_scan, peak);
            const char* label = NULL;
            char label_buf[HAM_BAND_LABEL_SIZE];

            for(uint8_t i = 0; i < app->band.count; ++i) {
                if(app->band.entries[i].frequency == frequency) {
                    strncpy(label_buf, app->band.entries[i].label, sizeof(label_buf) - 1);
                    label_buf[sizeof(label_buf) - 1] = '\0';
                    label = label_buf;
                    break;
                }
            }

            ham_radio_radio_tune(app, frequency);
            app->tune_frequency = frequency;
            strncpy(app->tune_label, label ? label : "", sizeof(app->tune_label) - 1);
            ham_view_tune_set_frequency(app->view_tune, frequency, label);
            scene_manager_next_scene(app->scene_manager, HamRadioSceneTune);
            return true;
        }

        default:
            return false;
        }
    }

    if(event.type != SceneManagerEventTypeBack) {
        return false;
    }

    ham_scan_worker_set_enabled(app->scan_worker, false);
    ham_radio_radio_idle(app);
    return scene_manager_previous_scene(app->scene_manager);
}

void ham_radio_scene_scan_on_exit(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    ham_scan_worker_set_enabled(app->scan_worker, false);
    ham_scan_worker_set_callback(app->scan_worker, NULL, NULL);

    HamScanBridge* bridge = &ham_radio_scene_scan_bridge;
    if(bridge->mutex) {
        furi_check(furi_mutex_acquire(bridge->mutex, FuriWaitForever) == FuriStatusOk);
        bridge->has_pending = false;
        furi_mutex_release(bridge->mutex);
        furi_mutex_free(bridge->mutex);
        bridge->mutex = NULL;
    }
}
