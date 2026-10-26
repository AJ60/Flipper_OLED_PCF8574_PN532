#include <furi.h>
#include <stdlib.h>
#include <string.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <input/input.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

#define TAG "SubGhzJammer"
#define RECORD_GUI "gui"
#define RECORD_DIALOGS "dialogs"
#define RECORD_NOTIFICATION "notification"

/**
 * Worker thread: owns the transmission lifecycle. Starts the async-TX loop
 * (the actual RF energy is emitted by the DMA interrupt) and, for hop mode,
 * reconfigures the frequency on each dwell. It waits for the abort signal
 * before stopping.
 */
int32_t jammer_worker_thread(void* context) {
    Jammer* jammer = (Jammer*)context;
    const SubGhzDevice* device = NULL;
    JammerSignal* signal = NULL;

    device = jammer_radio_get_device();
    if(device == NULL) {
        FURI_LOG_E(TAG, "No radio device");
        jammer->tx_active = false;
        return 0;
    }

    signal = jammer_signal_alloc();
    if(signal == NULL) {
        jammer->tx_active = false;
        return 0;
    }
    jammer_signal_set_mode(signal, jammer->settings.mode);
    jammer_signal_set_frequency(signal, jammer->settings.frequency);

    if(!jammer_radio_begin(device, jammer->settings.frequency)) {
        FURI_LOG_E(TAG, "radio begin failed");
        jammer_signal_free(signal);
        jammer->tx_active = false;
        return 0;
    }
    if(!jammer_radio_start(device, signal)) {
        FURI_LOG_E(TAG, "radio start failed");
        jammer_signal_free(signal);
        jammer_radio_stop(device);
        jammer->tx_active = false;
        return 0;
    }

    jammer->tx_active = true;

    /* Main loop: for hop mode, change frequency on each dwell. */
    uint32_t hop_index = 0;
    while(!jammer->abort_requested) {
        if(jammer->settings.mode == JammerSignalModeNoise &&
           jammer->settings.hop_count > 1) {
            uint32_t freq = jammer_signal_hop_frequency(
                jammer->settings.band_min,
                jammer->settings.band_max,
                hop_index % jammer->settings.hop_count,
                jammer->settings.hop_count);
            jammer_radio_set_frequency(device, freq);
            jammer_signal_reset(signal);
            hop_index++;
            furi_delay_ms(jammer->settings.dwell_ms);
        } else {
            furi_delay_ms(jammer->settings.dwell_ms * 10);
        }
    }

    /* Stop transmission. */
    jammer->tx_active = false;
    jammer_signal_free(signal);
    jammer_radio_stop(device);
    return 0;
}

/** Request a clean abort of the current transmission. */
void jammer_worker_request_abort(void) {
    // No-op: abort is coordinated via jammer->abort_requested in the scene.
}

/** @return true if a transmission is currently active. */
bool jammer_worker_is_active(void) {
    return false;
}

/** Back-event handler: forwards a scene "back" to a custom abort event. */
static bool subghz_jammer_navigation_callback(void* context);

Jammer* jammer_alloc(void) {
    Jammer* jammer = malloc(sizeof(Jammer));
    if(jammer == NULL) {
        return NULL;
    }
    memset(jammer, 0, sizeof(Jammer));

    jammer->scene_manager = scene_manager_alloc(&subghz_jammer_scene_handlers, jammer);
    jammer->view_dispatcher = view_dispatcher_alloc();
    jammer->widget = widget_alloc();

    jammer->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_set_event_callback_context(jammer->view_dispatcher, jammer);
    view_dispatcher_set_navigation_event_callback(
        jammer->view_dispatcher, subghz_jammer_navigation_callback);
    view_dispatcher_attach_to_gui(jammer->view_dispatcher, jammer->gui, ViewDispatcherTypeFullscreen);

    jammer->dialogs = furi_record_open(RECORD_DIALOGS);
    jammer->notifications = furi_record_open(RECORD_NOTIFICATION);
    jammer->input_pubsub = furi_record_open(RECORD_INPUT_EVENTS);

    if(!jammer_radio_init(jammer->settings.radio)) {
        FURI_LOG_E(TAG, "Radio init failed");
        scene_manager_free(jammer->scene_manager);
        view_dispatcher_free(jammer->view_dispatcher);
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_DIALOGS);
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_INPUT_EVENTS);
        free(jammer);
        return NULL;
    }

    jammer_settings_defaults(&jammer->settings);
    jammer_settings_load(&jammer->settings);

    return jammer;
}

void jammer_free(Jammer* jammer) {
    furi_assert(jammer);

    jammer->abort_requested = true;
    if(jammer->worker != NULL) {
        furi_thread_free(jammer->worker);
        jammer->worker = NULL;
    }

    jammer_radio_deinit();

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_INPUT_EVENTS);

    scene_manager_free(jammer->scene_manager);
    view_dispatcher_free(jammer->view_dispatcher);

    widget_free(jammer->widget);

    jammer_settings_save(&jammer->settings);

    free(jammer);
}

static bool subghz_jammer_navigation_callback(void* context) {
    Jammer* jammer = (Jammer*)context;
    if(scene_manager_handle_back_event(jammer->scene_manager)) {
        view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventBack);
        return true;
    }
    return false;
}

int32_t subghz_jammer_app(void* p) {
    UNUSED(p);
    furi_hal_power_suppress_charge_enter();

    Jammer* jammer = jammer_alloc();
    if(jammer == NULL) {
        furi_hal_power_suppress_charge_exit();
        return 255;
    }

    scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenemenu);

    view_dispatcher_run(jammer->view_dispatcher);

    jammer_free(jammer);

    furi_hal_power_suppress_charge_exit();
    return 0;
}
