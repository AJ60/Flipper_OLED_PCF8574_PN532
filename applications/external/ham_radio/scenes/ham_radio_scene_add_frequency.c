#include "ham_radio_scene.h"
#include "../ham_radio_app_i.h"

/** Frequencies are entered in kHz: the whole 300-928 MHz range fits within the
 * keypad's digit budget, and every 25 kHz step maps onto a valid channel. */
#define TAG "HamRadioAddFreq"

#define HAM_FREQ_INPUT_MIN_KHZ (300000)
#define HAM_FREQ_INPUT_MAX_KHZ (928000)

void ham_radio_scene_add_frequency_callback(void* context, int32_t number) {
    furi_assert(context);
    HamRadioApp* app = context;

    app->draft_frequency = (uint32_t)number * 1000ul;
    app->draft_in_use = true;

    FURI_LOG_I(TAG, "Draft frequency %lu Hz", (unsigned long)app->draft_frequency);
    scene_manager_next_scene(app->scene_manager, HamRadioSceneAddLabel);
}

void ham_radio_scene_add_frequency_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    const uint32_t current_khz =
        app->draft_in_use ? (app->draft_frequency / 1000ul) : 433000ul;

    number_input_set_header_text(app->number_input, "Frequency (kHz)");
    number_input_set_result_callback(
        app->number_input,
        ham_radio_scene_add_frequency_callback,
        app,
        (int32_t)current_khz,
        HAM_FREQ_INPUT_MIN_KHZ,
        HAM_FREQ_INPUT_MAX_KHZ);

    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdNumberInput);
}

bool ham_radio_scene_add_frequency_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        app->draft_in_use = false;
        return scene_manager_previous_scene(app->scene_manager);
    }

    return false;
}

void ham_radio_scene_add_frequency_on_exit(void* context) {
    UNUSED(context);
}
