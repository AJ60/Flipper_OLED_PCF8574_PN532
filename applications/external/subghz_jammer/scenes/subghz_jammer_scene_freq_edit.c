#include <furi.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <input/input.h>

#include "subghz_jammer.h"
#include "jammer_freq_edit.h"
#include "scenes/subghz_jammer_scene.h"

/* Forward declaration: used by freq_edit_render before its definition. */
static void freq_edit_button_callback(GuiButtonType result, InputType type, void* context);

/* Render the frequency editor: title, current frequency, and +/- / save buttons. */
static void freq_edit_render(Jammer* jammer) {
    Widget* widget = jammer->widget_freq_edit;
    widget_reset(widget);

    char freq_buf[32];
    freq_hz_to_text(jammer->settings.frequency, freq_buf, sizeof(freq_buf));

    widget_add_string_element(widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Edit Frequency");
    widget_add_string_element(
        widget, 64, 26, AlignCenter, AlignCenter, FontBigNumbers, freq_buf);
    widget_add_string_element(
        widget, 64, 44, AlignCenter, AlignBottom, FontSecondary, "L/R: +/-  OK: save");

    widget_add_button_element(widget, GuiButtonTypeLeft, "<", freq_edit_button_callback, jammer);
    widget_add_button_element(
        widget, GuiButtonTypeRight, ">", freq_edit_button_callback, jammer);
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Save", freq_edit_button_callback, jammer);
}

/* Apply a signed step, clamping into range and keeping settings.frequency in sync. */
static void freq_edit_step(Jammer* jammer, int32_t delta) {
    int32_t index = (int32_t)freq_hz_to_index(jammer->settings.frequency);
    int32_t new_index = freq_edit_step_index(index, delta);
    if(new_index >= 0) {
        jammer->settings.frequency = freq_index_to_hz((uint16_t)new_index);
    }
}

void freq_edit_button_callback(GuiButtonType result, InputType type, void* context) {
    Jammer* jammer = (Jammer*)context;
    if(type != InputTypeShort) {
        return;
    }

    switch(result) {
    case GuiButtonTypeLeft:
        freq_edit_step(jammer, -1);
        break;
    case GuiButtonTypeRight:
        freq_edit_step(jammer, +1);
        break;
    case GuiButtonTypeCenter:
        /* Confirm: persist the live-edited frequency (kept in sync by freq_edit_step)
         * and return to the menu. Do NOT recompute from freq_edit_index, which is set
         * once at scene entry and would overwrite the edited value with the original
         * 1 MHz-grid frequency. */
        jammer_settings_save(&jammer->settings);
        scene_manager_search_and_switch_to_previous_scene(
            jammer->scene_manager, SubGhzJammerScenemenu);
        return;
    default:
        return;
    }

    freq_edit_render(jammer);
}

void subghz_jammer_scene_freq_edit_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    jammer->freq_edit_index = freq_hz_to_index(jammer->settings.frequency);
    freq_edit_render(jammer);

    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewFreqEdit);
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
    Jammer* jammer = (Jammer*)context;
    widget_reset(jammer->widget_freq_edit);
}
