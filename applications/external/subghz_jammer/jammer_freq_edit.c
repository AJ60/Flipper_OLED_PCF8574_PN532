#include "jammer_freq_edit.h"

/* kHz bounds for the full 300-928 MHz band (1 kHz resolution). */
#define FREQ_MIN_KHZ (300000u)
#define FREQ_MAX_KHZ (928000u)

/* kHz -> Hz. Pure math, host-tested. */
uint32_t freq_khz_to_hz(uint32_t khz) {
    return khz * 1000u;
}

/* Hz -> kHz (integer division). Pure math, host-tested. */
uint32_t freq_hz_to_khz(uint32_t hz) {
    return hz / 1000u;
}

/* Clamp kHz into the band. Pure math, host-tested. */
uint32_t freq_khz_clamp(uint32_t khz) {
    if(khz < FREQ_MIN_KHZ) {
        return FREQ_MIN_KHZ;
    }
    if(khz > FREQ_MAX_KHZ) {
        return FREQ_MAX_KHZ;
    }
    return khz;
}

#ifndef HOST_TEST

#include <gui/scene_manager.h>
#include <gui/modules/number_input.h>
#include <gui/view_dispatcher.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

/* Commit the edited kHz value (kHz*1000 = Hz), persist, and return to the menu. */
static void freq_edit_result_callback(void* context, int32_t number) {
    Jammer* jammer = (Jammer*)context;
    if(jammer == NULL) {
        return;
    }
    uint32_t khz = freq_khz_clamp((uint32_t)number);
    jammer->settings.frequency = freq_khz_to_hz(khz);
    jammer_settings_save(&jammer->settings);
    scene_manager_search_and_switch_to_previous_scene(
        jammer->scene_manager, SubGhzJammerScenemenu);
}

void jammer_freq_edit_enter(Jammer* jammer) {
    furi_assert(jammer);
    furi_assert(jammer->number_input);

    uint32_t khz = freq_khz_clamp(freq_hz_to_khz(jammer->settings.frequency));

    /* set_result_callback() fully (re)initialises the NumberInput model
     * (value, bounds, text buffer, callback), so no separate reset is needed. */
    number_input_set_header_text(jammer->number_input, "Freq (kHz)");
    number_input_set_result_callback(
        jammer->number_input,
        freq_edit_result_callback,
        jammer,
        (int32_t)khz,
        FREQ_MIN_KHZ,
        FREQ_MAX_KHZ);

    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewFreqEdit);
}

#endif /* HOST_TEST */
