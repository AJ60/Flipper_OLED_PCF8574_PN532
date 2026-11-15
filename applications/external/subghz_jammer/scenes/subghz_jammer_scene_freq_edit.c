#include <furi.h>
#include <gui/scene_manager.h>

#include "subghz_jammer.h"
#include "jammer_freq_edit.h"
#include "scenes/subghz_jammer_scene.h"

/* The editor is a NumberInput view configured by jammer_freq_edit_enter().
 * OK commits through its result callback; Back cancels back to the menu. */
void subghz_jammer_scene_freq_edit_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    jammer_freq_edit_enter(jammer);
}

bool subghz_jammer_scene_freq_edit_on_event(void* context, SceneManagerEvent event) {
    Jammer* jammer = (Jammer*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        /* Discard changes and return to the menu. */
        scene_manager_search_and_switch_to_previous_scene(
            jammer->scene_manager, SubGhzJammerScenemenu);
        consumed = true;
    }

    return consumed;
}

void subghz_jammer_scene_freq_edit_on_exit(void* context) {
    UNUSED(context);
}
