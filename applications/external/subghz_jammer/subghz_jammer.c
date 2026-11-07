#include <furi.h>
#include <stdlib.h>
#include <string.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <input/input.h>

#include "subghz_jammer.h"
#include "jammer_freq.h"
#include "scenes/subghz_jammer_scene.h"

#define TAG "SubGhzJammer"
#define RECORD_GUI "gui"
#define RECORD_DIALOGS "dialogs"
#define RECORD_NOTIFICATION "notification"

static uint32_t jammer_worker_next_frequency(JammerSettings* s, uint32_t* sweep_step) {
    switch(s->mode) {
    case JammerSignalModeNoise:
        if(s->hop_count > 1) {
            uint32_t idx = (*sweep_step)++ % s->hop_count;
            return jammer_signal_hop_frequency(
                s->band_min, s->band_max, idx, s->hop_count);
        }
        return s->frequency;
    case JammerSignalModeSweep:
        return jammer_freq_sweep_point(
            s->band_min, s->band_max, (*sweep_step)++, s->hop_count);
    case JammerSignalModePingPong: {
        uint32_t pos = jammer_freq_pingpong((*sweep_step)++, s->hop_count);
        return s->band_min +
            (s->band_max > s->band_min ?
                (uint64_t)(s->band_max - s->band_min) * pos /
                    (s->hop_count > 1 ? s->hop_count - 1 : 1) :
                0);
    }
    case JammerSignalModeCarrier:
    default:
        return s->frequency;
    }
}

int32_t jammer_worker_thread(void* context) {
    Jammer* jammer = (Jammer*)context;
    const SubGhzDevice* device = jammer_radio_get_device();
    if(device == NULL) {
        FURI_LOG_E(TAG, "No radio device");
        jammer->tx_active = false;
        jammer->tx_error = true;
        view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventTxError);
        return 0;
    }

    JammerSignal* signal = jammer_signal_alloc();
    if(signal == NULL) {
        jammer->tx_active = false;
        jammer->tx_error = true;
        view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventTxError);
        return 0;
    }
    jammer_signal_set_mode(signal, jammer->settings.mode);
    jammer_signal_set_frequency(signal, jammer->settings.frequency);
    signal->pulse_ratio = jammer->settings.pulse_ratio;

    if(!jammer_radio_begin(device, jammer->settings.frequency)) {
        FURI_LOG_E(TAG, "radio begin failed");
        jammer_signal_free(signal);
        jammer->tx_active = false;
        jammer->tx_error = true;
        view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventTxError);
        return 0;
    }

    if(!jammer_radio_start(device, signal)) {
        FURI_LOG_E(TAG, "radio start failed");
        jammer_signal_free(signal);
        jammer->tx_active = false;
        jammer->tx_error = true;
        view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventTxError);
        return 0;
    }

    // Apply the user's TX power setting once at start.
    jammer_radio_set_tx_power(device, jammer->settings.power_level);

    jammer->tx_active = true;
    jammer->tx_error = false;

    uint32_t sweep_step = 0;
    const uint32_t tick_ms = 50;
    // Real wall-clock watchdog: stop after max_tx_duration_ms of actual elapsed
    // time. We accumulate the real delay each iteration (tick_ms for the
    // skip/idle branches, the actual dwell time for a hop) so the timeout means
    // milliseconds regardless of dwell_ms or hop cadence. The old code counted
    // loop iterations, which in sweep/hop modes each spanned a dwell_ms delay
    // (~2000 ms), making the 5-minute default permit ~200 minutes of TX.
    uint32_t elapsed_ms = 0;
    while(!jammer->abort_requested) {
        bool sweep = (jammer->settings.mode == JammerSignalModeSweep ||
                      jammer->settings.mode == JammerSignalModePingPong ||
                      (jammer->settings.mode == JammerSignalModeNoise &&
                       jammer->settings.hop_count > 1));
        if(sweep) {
            uint32_t freq = jammer_worker_next_frequency(&jammer->settings, &sweep_step);
            // Validate against the band + EU-region gate before committing.
            if(!jammer_radio_pre_tx_check(device, freq, jammer->settings.bypass_region)) {
                // Frequency gated: skip this hop, keep the loop alive.
                furi_delay_ms(tick_ms);
                elapsed_ms += tick_ms;
                if(jammer->settings.max_tx_duration_ms &&
                   elapsed_ms >= jammer->settings.max_tx_duration_ms) {
                    break;
                }
                continue;
            }
            // Clean hop: stop -> reconfigure -> restart at the new frequency.
            if(!jammer_radio_commit_hop(device, signal, freq)) {
                FURI_LOG_E(TAG, "hop commit failed: %lu", freq);
                furi_delay_ms(tick_ms);
                elapsed_ms += tick_ms;
                if(jammer->settings.max_tx_duration_ms &&
                   elapsed_ms >= jammer->settings.max_tx_duration_ms) {
                    break;
                }
                continue;
            }
            jammer_signal_reset(signal);

            uint32_t dwell_elapsed = 0;
            while(!jammer->abort_requested && dwell_elapsed < jammer->settings.dwell_ms) {
                uint32_t step = (jammer->settings.dwell_ms - dwell_elapsed > tick_ms) ?
                                    tick_ms :
                                    (jammer->settings.dwell_ms - dwell_elapsed);
                furi_delay_ms(step);
                dwell_elapsed += step;
            }
            elapsed_ms += dwell_elapsed;
        } else {
            furi_delay_ms(tick_ms);
            elapsed_ms += tick_ms;
        }
        if(jammer->settings.max_tx_duration_ms &&
           elapsed_ms >= jammer->settings.max_tx_duration_ms) {
            break;
        }
    }

    jammer_radio_stop(device);
    jammer_signal_free(signal);
    jammer->tx_active = false;
    return 0;
}

void jammer_start_worker(Jammer* jammer) {
    if(jammer->worker == NULL) {
        jammer->abort_requested = false;
        jammer->worker = furi_thread_alloc_ex(
            "SubGhzJammerWorker", 2 * 1024, jammer_worker_thread, jammer);
        furi_check(jammer->worker);
        furi_thread_start(jammer->worker);
    }
}

void jammer_stop_worker(Jammer* jammer) {
    if(jammer->worker != NULL) {
        jammer->abort_requested = true;
        furi_thread_join(jammer->worker);
        furi_thread_free(jammer->worker);
        jammer->worker = NULL;
    }
}

static bool subghz_jammer_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Jammer* jammer = (Jammer*)context;
    return scene_manager_handle_custom_event(jammer->scene_manager, event);
}

static bool subghz_jammer_navigation_callback(void* context) {
    furi_assert(context);
    Jammer* jammer = (Jammer*)context;
    return scene_manager_handle_back_event(jammer->scene_manager);
}

Jammer* jammer_alloc(void) {
    Jammer* jammer = malloc(sizeof(Jammer));
    if(jammer == NULL) {
        return NULL;
    }
    memset(jammer, 0, sizeof(Jammer));

    jammer_settings_load(&jammer->settings);

    if(!jammer_radio_init(jammer->settings.radio)) {
        FURI_LOG_E(TAG, "Radio init failed");
        free(jammer);
        return NULL;
    }

    jammer->scene_manager = scene_manager_alloc(&subghz_jammer_scene_handlers, jammer);
    if(jammer->scene_manager == NULL) {
        FURI_LOG_E(TAG, "scene_manager_alloc failed");
        jammer_radio_deinit();
        free(jammer);
        return NULL;
    }
    jammer->view_dispatcher = view_dispatcher_alloc();
    if(jammer->view_dispatcher == NULL) {
        FURI_LOG_E(TAG, "view_dispatcher_alloc failed");
        scene_manager_free(jammer->scene_manager);
        jammer_radio_deinit();
        free(jammer);
        return NULL;
    }

    jammer->gui = furi_record_open(RECORD_GUI);
    if(jammer->gui == NULL) {
        FURI_LOG_E(TAG, "furi_record_open(gui) failed");
        view_dispatcher_free(jammer->view_dispatcher);
        scene_manager_free(jammer->scene_manager);
        jammer_radio_deinit();
        free(jammer);
        return NULL;
    }
    view_dispatcher_set_event_callback_context(jammer->view_dispatcher, jammer);
    view_dispatcher_set_custom_event_callback(
        jammer->view_dispatcher, subghz_jammer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        jammer->view_dispatcher, subghz_jammer_navigation_callback);
    view_dispatcher_attach_to_gui(jammer->view_dispatcher, jammer->gui, ViewDispatcherTypeFullscreen);

    jammer->dialogs = furi_record_open(RECORD_DIALOGS);
    jammer->notifications = furi_record_open(RECORD_NOTIFICATION);

    jammer->var_item_list = variable_item_list_alloc();
    if(jammer->var_item_list == NULL) {
        FURI_LOG_E(TAG, "variable_item_list_alloc failed");
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_DIALOGS);
        furi_record_close(RECORD_NOTIFICATION);
        view_dispatcher_free(jammer->view_dispatcher);
        scene_manager_free(jammer->scene_manager);
        jammer_radio_deinit();
        free(jammer);
        return NULL;
    }
    view_dispatcher_add_view(
        jammer->view_dispatcher,
        SubGhzJammerViewMenu,
        variable_item_list_get_view(jammer->var_item_list));

    jammer->widget_start = widget_alloc();
    if(jammer->widget_start == NULL) {
        FURI_LOG_E(TAG, "widget_alloc(start) failed");
        view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewMenu);
        variable_item_list_free(jammer->var_item_list);
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_DIALOGS);
        furi_record_close(RECORD_NOTIFICATION);
        view_dispatcher_free(jammer->view_dispatcher);
        scene_manager_free(jammer->scene_manager);
        jammer_radio_deinit();
        free(jammer);
        return NULL;
    }
    view_dispatcher_add_view(
        jammer->view_dispatcher,
        SubGhzJammerViewStart,
        widget_get_view(jammer->widget_start));

    jammer->widget_stop = widget_alloc();
    if(jammer->widget_stop == NULL) {
        FURI_LOG_E(TAG, "widget_alloc(stop) failed");
        view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewStart);
        view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewMenu);
        widget_free(jammer->widget_start);
        variable_item_list_free(jammer->var_item_list);
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_DIALOGS);
        furi_record_close(RECORD_NOTIFICATION);
        view_dispatcher_free(jammer->view_dispatcher);
        scene_manager_free(jammer->scene_manager);
        jammer_radio_deinit();
        free(jammer);
        return NULL;
    }
    view_dispatcher_add_view(
        jammer->view_dispatcher,
        SubGhzJammerViewStop,
        widget_get_view(jammer->widget_stop));

    jammer->widget_freq_edit = widget_alloc();
    if(jammer->widget_freq_edit == NULL) {
        FURI_LOG_E(TAG, "widget_alloc(freq_edit) failed");
        view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewStop);
        widget_free(jammer->widget_start);
        widget_free(jammer->widget_stop);
        variable_item_list_free(jammer->var_item_list);
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_DIALOGS);
        furi_record_close(RECORD_NOTIFICATION);
        view_dispatcher_free(jammer->view_dispatcher);
        scene_manager_free(jammer->scene_manager);
        jammer_radio_deinit();
        free(jammer);
        return NULL;
    }
    view_dispatcher_add_view(
        jammer->view_dispatcher,
        SubGhzJammerViewFreqEdit,
        widget_get_view(jammer->widget_freq_edit));

    return jammer;
}

void jammer_free(Jammer* jammer) {
    furi_assert(jammer);

    jammer_stop_worker(jammer);
    jammer_settings_save(&jammer->settings);

    view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewMenu);
    view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewStart);
    view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewStop);
    view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewFreqEdit);

    variable_item_list_free(jammer->var_item_list);
    widget_free(jammer->widget_start);
    widget_free(jammer->widget_stop);
    widget_free(jammer->widget_freq_edit);

    view_dispatcher_free(jammer->view_dispatcher);
    scene_manager_free(jammer->scene_manager);

    jammer_radio_deinit();

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);

    free(jammer);
}

int32_t subghz_jammer_app(void* p) {
    UNUSED(p);

    Jammer* jammer = jammer_alloc();
    if(jammer == NULL) {
        return 255;
    }

    scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenemenu);
    view_dispatcher_run(jammer->view_dispatcher);
    jammer_free(jammer);

    return 0;
}
