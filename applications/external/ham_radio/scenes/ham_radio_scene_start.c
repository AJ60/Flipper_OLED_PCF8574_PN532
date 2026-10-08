#include "ham_radio_scene.h"
#include "../ham_radio_app_i.h"

#define TAG "HamRadioStart"

static void ham_radio_scene_start_submenu_callback(void* context, uint32_t index) {
    HamRadioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/** Row order matches HamRadioStartItem. */
void ham_radio_scene_start_on_enter(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Ham Radio");
    submenu_add_item(app->submenu, "Frequencies", 0, ham_radio_scene_start_submenu_callback, app);
    submenu_add_item(app->submenu, "Scan All", 1, ham_radio_scene_start_submenu_callback, app);
    submenu_add_item(app->submenu, "Settings", 2, ham_radio_scene_start_submenu_callback, app);
    submenu_add_item(app->submenu, "About", 3, ham_radio_scene_start_submenu_callback, app);

    submenu_set_selected_item(app->submenu, app->start_selected);
    view_dispatcher_switch_to_view(app->view_dispatcher, HamRadioViewIdSubmenu);
}

bool ham_radio_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    HamRadioApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case 0:
            scene_manager_next_scene(app->scene_manager, HamRadioSceneBand);
            return true;
        case 1:
            ham_radio_push_rx_channels(app);
            scene_manager_next_scene(app->scene_manager, HamRadioSceneScan);
            return true;
        case 2:
            scene_manager_next_scene(app->scene_manager, HamRadioSceneSettings);
            return true;
        case 3:
            scene_manager_next_scene(app->scene_manager, HamRadioSceneAbout);
            return true;
        default:
            return false;
        }
    }
    return false;
}

void ham_radio_scene_start_on_exit(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    app->start_selected = submenu_get_selected_item(app->submenu);
}
