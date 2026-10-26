#include <furi.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <input/input.h>
#include <gui/modules/widget.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

static const char* const status_text[] = {
    "Jamming",
    "Idle",
};

static const char* mode_text(JammerSignalMode mode) {
    return mode == JammerSignalModeCarrier ? "Carrier" : "Noise";
}

static const char* radio_text(JammerRadioChoice radio) {
    switch(radio) {
    case JammerRadioAuto: return "Auto";
    case JammerRadioInternal: return "Internal";
    case JammerRadioExternalCC1101: return "External";
    }
    return "Unknown";
}

static void format_status(char* buffer, Jammer* jammer) {
    snprintf(
        buffer,
        128,
        "%s\n%s\n%u Hz\n%u ms\n%s\n",
        mode_text(jammer->settings.mode),
        radio_text(jammer->settings.radio),
        (unsigned int)jammer->settings.frequency,
        (unsigned int)jammer->settings.dwell_ms,
        status_text[jammer->tx_active]);
}

static void jammer_start_input_callback(const void* message, void* context) {
    Jammer* jammer = (Jammer*)context;
    const InputEvent* event = (const InputEvent*)message;

    if(event->type == InputTypePress && event->key == InputKeyLeft) {
        /* Begin the transmission worker on the next event-loop tick. */
        jammer->worker = furi_thread_alloc_ex(
            "SubGhzJammerWorker", 2 * 1024, jammer_worker_thread, jammer);
        if(jammer->worker == NULL) {
            FURI_LOG_E(TAG, "thread alloc failed");
            return;
        }
        furi_thread_start(jammer->worker);
    } else if(event->type == InputTypeRelease && event->key == InputKeyLeft) {
        /* Release the trigger: request a clean abort. */
        jammer->abort_requested = true;
    }
}

void subghz_jammer_scene_start_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    Widget* widget = jammer->widget;
    char buffer[128];

    jammer->abort_requested = false;
    jammer->tx_active = false;

    format_status(buffer, jammer);
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, buffer);

    view_dispatcher_add_view(
        jammer->view_dispatcher, SubGhzJammerViewStart, widget_get_view(widget));
    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewStart);

    jammer->input_subscription = furi_pubsub_subscribe(
        jammer->input_pubsub, jammer_start_input_callback, jammer);
}

bool subghz_jammer_scene_start_on_event(void* context, SceneManagerEvent event) {
    Jammer* jammer = (Jammer*)context;

    if(event.type == SceneManagerEventTypeBack) {
        jammer->abort_requested = true;
        scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenestop);
        return true;
    }

    return false;
}

void subghz_jammer_scene_start_on_exit(void* context) {
    Jammer* jammer = (Jammer*)context;
    Widget* widget = jammer->widget;

    if(jammer->input_subscription != NULL) {
        furi_pubsub_unsubscribe(jammer->input_pubsub, jammer->input_subscription);
        jammer->input_subscription = NULL;
    }

    widget_reset(widget);
}
