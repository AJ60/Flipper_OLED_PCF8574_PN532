#include <furi.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

void subghz_jammer_scene_stop_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    Widget* widget = jammer->widget;

    widget_add_text_scroll_element(
        widget, 0, 0, 128, 64, "Jamming stopped.\nPress to return.");

    view_dispatcher_add_view(
        jammer->view_dispatcher, SubGhzJammerViewStop, widget_get_view(widget));
    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewStop);
}

bool subghz_jammer_scene_stop_on_event(void* context, SceneManagerEvent event) {
    Jammer* jammer = (Jammer*)context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenemenu);
        return true;
    }

    return false;
}

void subghz_jammer_scene_stop_on_exit(void* context) {
    Jammer* jammer = (Jammer*)context;
    Widget* widget = jammer->widget;
    widget_reset(widget);
}
