#include <furi.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

static void jammer_stop_button_callback(GuiButtonType result, InputType type, void* context) {
    UNUSED(result);
    Jammer* jammer = (Jammer*)context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventBack);
    }
}

void subghz_jammer_scene_stop_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    Widget* widget = jammer->widget_stop;
    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 15, AlignCenter, AlignCenter, FontPrimary, "Jamming Stopped");
    widget_add_string_element(
        widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, "Radio is idle.");
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Back", jammer_stop_button_callback, jammer);

    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewStop);
}

bool subghz_jammer_scene_stop_on_event(void* context, SceneManagerEvent event) {
    Jammer* jammer = (Jammer*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzJammerEventBack) {
            scene_manager_search_and_switch_to_previous_scene(
                jammer->scene_manager, SubGhzJammerScenemenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            jammer->scene_manager, SubGhzJammerScenemenu);
        consumed = true;
    }

    return consumed;
}

void subghz_jammer_scene_stop_on_exit(void* context) {
    Jammer* jammer = (Jammer*)context;
    widget_reset(jammer->widget_stop);
}
