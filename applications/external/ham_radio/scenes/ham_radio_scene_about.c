#include "ham_radio_scene.h"
#include "../ham_radio_app_i.h"

#define TAG "HamRadioAbout"

void ham_radio_scene_about_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        5,
        AlignCenter,
        AlignTop,
        FontPrimary,
        "Ham Radio");
    widget_add_string_element(app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary, "Receive build v0.1");

    char module_str[24];
    snprintf(
        module_str,
        sizeof(module_str),
        "Module: %s",
        app->radio_external ? "External E07" : "Internal CC1101");
    widget_add_string_element(
        app->widget, 64, 33, AlignCenter, AlignTop, FontSecondary, module_str);
    widget_add_string_element(
        app->widget, 64, 44, AlignCenter, AlignTop, FontSecondary, "CC1101: OOK/FSK only");
    widget_add_string_element(
        app->widget, 64, 54, AlignCenter, AlignTop, FontSecondary, "No voice, no TX yet");

    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdWidget);
}

bool ham_radio_scene_about_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        return scene_manager_previous_scene(app->scene_manager);
    }

    return false;
}

void ham_radio_scene_about_on_exit(void* context) {
    UNUSED(context);
}
