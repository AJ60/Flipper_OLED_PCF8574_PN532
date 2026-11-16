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
#include "jammer_thermal.h"
#include "scenes/subghz_jammer_scene.h"

#define TAG "SubGhzJammer"
#define RECORD_GUI "gui"
#define RECORD_DIALOGS "dialogs"
#define RECORD_NOTIFICATION "notification"

/* Next hop frequency for the current mode.
 *
 * Sweep / PingPong derive the target set from the band span + step_hz so the
 * hops resolve at the user's configured frequency resolution and span the whole
 * [band_min, band_max]. PingPong reverses direction at the band ends. Noise
 * (with hop_count > 1) steps broadband bursts by noise_step_hz across the band.
 * Carrier / single-hop modes hold on the fixed frequency.
 */
/* Stage the next window of hop targets into signal->hop_q (FIFO ring).
 *
 * The queue holds up to JAMMER_HOP_QUEUE_LEN targets; the worker drains one
 * target per hop and refills when exhausted, advancing @p base_step by the
 * window size each refill so the sweep / ping-pong cadence is preserved across
 * windows. Carrier / single-hop modes leave the queue empty.
 *
 * @return the next base_step to use for the following refill.
 */
static uint32_t jammer_worker_fill_queue(JammerSignal* signal, JammerSettings* s, uint32_t base_step) {
    jammer_hop_queue_init(&signal->hop_q);
    switch(s->mode) {
    case JammerSignalModeSweep:
    case JammerSignalModePingPong: {
        uint32_t count = jammer_freq_hop_count_for_span(
            s->band_min, s->band_max, s->sweep_step_hz);
        for(uint32_t i = 0u; i < JAMMER_HOP_QUEUE_LEN; i++) {
            uint32_t idx;
            if(s->mode == JammerSignalModePingPong) {
                idx = jammer_freq_hop_pingpong(base_step + i, count);
            } else {
                idx = (base_step + i) % count;
            }
            uint32_t freq = jammer_freq_step_target(
                s->band_min, s->sweep_step_hz, idx, s->band_min, s->band_max);
            jammer_signal_hop_enqueue(signal, freq);
        }
        break;
    }
    case JammerSignalModeNoise:
        if(s->hop_count > 1) {
            uint32_t count = jammer_freq_hop_count_for_span(
                s->band_min, s->band_max, s->noise_step_hz);
            for(uint32_t i = 0u; i < JAMMER_HOP_QUEUE_LEN; i++) {
                uint32_t idx = (base_step + i) % count;
                uint32_t freq = jammer_freq_noise_hop_target(
                    s->band_min, s->band_max, s->noise_step_hz, idx);
                jammer_signal_hop_enqueue(signal, freq);
            }
        }
        break;
    default:
        break;
    }
    return base_step + JAMMER_HOP_QUEUE_LEN;
}

/* Pure thermal throttle: estimate junction temperature and average TX current
 * at the target frequency, then apply the decision (reduce duty -> reduce
 * power -> stop). Uses runtime_pulse_ratio (NOT settings.pulse_ratio) so the
 * user's configured duty-cycle survives the session and is not flash-saved with
 * a thermally-degraded value. Returns true if the TX session should stop now.
 */
static bool jammer_worker_apply_thermal(
    Jammer* jammer,
    uint32_t freq,
    bool* duty_already_reduced,
    uint32_t elapsed_ms) {
    const bool external_pa = (freq >= 900000000u);
    uint32_t i_full = jammer_thermal_est_current_ma(
        freq, jammer->settings.power_level, external_pa);
    uint32_t i_avg = (i_full * jammer->runtime_pulse_ratio) / 100u;
    uint32_t temp = jammer_thermal_est_temp_c(25u, i_avg);
    jammer->thermal.temp_c = temp;
    jammer->thermal.avg_current_ma = i_avg;
    JammerThermalDecision d = jammer_thermal_decide(
        temp, i_avg, 0u, jammer->settings.max_tx_duration_ms, elapsed_ms, *duty_already_reduced);
    switch(d) {
    case JammerThermalReduceDuty:
        // Reduce the runtime duty only — never touch settings.pulse_ratio which
        // is the user's persisted config and is saved to flash on app exit.
        if(jammer->runtime_pulse_ratio > 5u) {
            jammer->runtime_pulse_ratio -= 5u;
            FURI_LOG_W(
                TAG,
                "thermal: duty throttled to %lu%%",
                (unsigned long)jammer->runtime_pulse_ratio);
        }
        *duty_already_reduced = true;
        break;
    case JammerThermalReducePower:
        // Do NOT destructively mutate jammer->settings.power_level.
        // Mutating settings permanently degraded the user's config down to 1% in flash.
        // Duty cycle throttling (JammerThermalReduceDuty) already cools the radio safely.
        FURI_LOG_W(TAG, "thermal: power-reduce requested; using duty throttle instead");
        break;
    case JammerThermalForceStop:
        return true;
    case JammerThermalOk:
    default:
        break;
    }
    return false;
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
    // Initialize runtime_pulse_ratio from the user's setting at session start.
    // Thermal throttling reduces runtime_pulse_ratio only; settings.pulse_ratio
    // is never modified during a TX session so it is preserved for flash save.
    jammer->runtime_pulse_ratio = jammer->settings.pulse_ratio;
    signal->pulse_ratio = jammer->runtime_pulse_ratio;

    // Set the user's TX power before begin so the first hop's PA table is
    // correct: jammer_radio_begin() applies it after load_preset + set_frequency.
    jammer_radio_set_tx_power(device, jammer->settings.power_level);

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

    // Power is now applied by jammer_radio_begin() above (and re-applied on each
    // hop via jammer_radio_commit_hop, and immediately by the thermal throttle
    // below). Kept here as a belt-and-braces re-apply in case the setting changed
    // between begin and here.
    jammer_radio_set_tx_power(device, jammer->settings.power_level);

    jammer->tx_active = true;
    jammer->tx_error = false;

    uint32_t sweep_step = 0;
    bool duty_already_reduced = false;
    uint32_t hop_cursor = 0u;
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
            // Refill the hop queue when empty or exhausted, advancing the
            // persistent sweep step so the sweep / ping-pong cadence is
            // preserved across windows.
            if(signal->hop_q.count == 0u || hop_cursor >= signal->hop_q.count) {
                sweep_step = jammer_worker_fill_queue(signal, &jammer->settings, sweep_step);
                hop_cursor = 0u;
            }
            // Drain the next hop target from the queue.
            uint32_t freq = jammer_freq_hop_queue_next(
                hop_cursor, signal->hop_q.freq, signal->hop_q.count);
            hop_cursor++;
            // Thermal throttle before committing the hop: may reduce duty /
            // power, or stop the session.
            if(jammer_worker_apply_thermal(
                   jammer, freq, &duty_already_reduced, elapsed_ms)) {
                break;
            }
            signal->pulse_ratio = jammer->runtime_pulse_ratio;
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
            // Inter-hop gap (DwellCycle): brief pause between hop targets.
            uint32_t hold_elapsed = 0;
            while(!jammer->abort_requested &&
                   hold_elapsed < jammer->settings.dwell_hold_ms) {
                uint32_t step = (jammer->settings.dwell_hold_ms - hold_elapsed > tick_ms) ?
                                    tick_ms :
                                    (jammer->settings.dwell_hold_ms - hold_elapsed);
                furi_delay_ms(step);
                hold_elapsed += step;
            }
            elapsed_ms += hold_elapsed;
        } else {
            // Carrier / single-hop: steady frequency, but still throttle.
            uint32_t freq = jammer->settings.frequency;
            if(jammer_worker_apply_thermal(
                   jammer, freq, &duty_already_reduced, elapsed_ms)) {
                break;
            }
            signal->pulse_ratio = jammer->runtime_pulse_ratio;
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

static void subghz_jammer_tick_event_callback(void* context) {
    furi_assert(context);
    Jammer* jammer = (Jammer*)context;
    scene_manager_handle_tick_event(jammer->scene_manager);
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
    view_dispatcher_set_tick_event_callback(
        jammer->view_dispatcher, subghz_jammer_tick_event_callback, 250);
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

    jammer->number_input = number_input_alloc();
    if(jammer->number_input == NULL) {
        FURI_LOG_E(TAG, "number_input_alloc(freq_edit) failed");
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
        number_input_get_view(jammer->number_input));

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
    number_input_free(jammer->number_input);

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
