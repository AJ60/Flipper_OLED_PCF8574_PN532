#include <furi.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/menu.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

/** Common sub-GHz operating frequencies (Hz). */
static const uint32_t frequency_presets[] = {
    315000000u, 433920000u, 433980000u, 868000000u, 915000000u};
#define FREQUENCY_PRESET_COUNT (sizeof(frequency_presets) / sizeof(frequency_presets[0]))

static const char* format_freq(char* buffer, uint32_t freq_hz) {
    double mhz = (double)freq_hz / (double)1e6;
    snprintf(buffer, 24, "%.3f MHz", mhz);
    return buffer;
}

static void menu_item_mode(void* context, uint32_t index) {
    UNUSED(index);
    Jammer* jammer = (Jammer*)context;
    jammer->settings.mode =
        (jammer->settings.mode == JammerSignalModeCarrier) ? JammerSignalModeNoise
                                                          : JammerSignalModeCarrier;
}

static void menu_item_frequency(void* context, uint32_t index) {
    UNUSED(index);
    Jammer* jammer = (Jammer*)context;

    /* Cycle through a small set of common sub-GHz presets. */
    uint32_t current = jammer->settings.frequency;
    size_t i = 0;
    for(; i < FREQUENCY_PRESET_COUNT; i++) {
        if(frequency_presets[i] == current) {
            break;
        }
    }
    size_t next = (i + 1) % FREQUENCY_PRESET_COUNT;
    jammer->settings.frequency = frequency_presets[next];
}

static void menu_item_band(void* context, uint32_t index) {
    UNUSED(index);
    Jammer* jammer = (Jammer*)context;
    /* Cycle through common bands; set frequency to a representative value. */
    static const struct {
        uint32_t min;
        uint32_t max;
        uint32_t default_freq;
    } bands[] = {
        {300000000u, 348000000u, 315000000u},
        {387000000u, 464000000u, 433920000u},
        {779000000u, 928000000u, 868000000u},
    };
    size_t count = sizeof(bands) / sizeof(bands[0]);
    size_t idx = 0;
    for(; idx < count; idx++) {
        if(jammer->settings.band_min == bands[idx].min &&
           jammer->settings.band_max == bands[idx].max) {
            break;
        }
    }
    size_t next = (idx + 1) % count;
    jammer->settings.band_min = bands[next].min;
    jammer->settings.band_max = bands[next].max;
    jammer->settings.frequency = bands[next].default_freq;
}

static void menu_item_radio(void* context, uint32_t index) {
    UNUSED(index);
    Jammer* jammer = (Jammer*)context;
    jammer->settings.radio = (JammerRadioChoice)((jammer->settings.radio + 1) % 3);
}

static void menu_item_latch(void* context, uint32_t index) {
    UNUSED(index);
    Jammer* jammer = (Jammer*)context;
    jammer->settings.latch = !jammer->settings.latch;
}

static void menu_item_hop(void* context, uint32_t index) {
    UNUSED(index);
    Jammer* jammer = (Jammer*)context;
    jammer->settings.hop_count = (uint16_t)((jammer->settings.hop_count % 16) + 1);
}

static void menu_item_dwell(void* context, uint32_t index) {
    UNUSED(index);
    Jammer* jammer = (Jammer*)context;
    /* Dwell between 500ms and 5000ms in 500ms steps. */
    uint16_t dwell = jammer->settings.dwell_ms;
    if(dwell < 500) {
        dwell = 500;
    }
    if(dwell >= 5000) {
        dwell = 500;
    } else {
        dwell += 500;
    }
    jammer->settings.dwell_ms = dwell;
}

static void menu_item_start(void* context, uint32_t index) {
    UNUSED(index);
    Jammer* jammer = (Jammer*)context;
    view_dispatcher_send_custom_event(
        jammer->view_dispatcher, SubGhzJammerEventStart);
}

void subghz_jammer_scene_menu_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    Menu* menu = menu_alloc();

    char freq_buf[24];
    format_freq(freq_buf, jammer->settings.frequency);

    menu_add_item(
        menu,
        "Mode",
        NULL,
        0,
        menu_item_mode,
        jammer);
    menu_add_item(
        menu,
        "Frequency",
        NULL,
        0,
        menu_item_frequency,
        jammer);
    menu_add_item(
        menu,
        "Band",
        NULL,
        0,
        menu_item_band,
        jammer);
    menu_add_item(
        menu,
        "Radio",
        NULL,
        0,
        menu_item_radio,
        jammer);
    menu_add_item(
        menu,
        "Hop Count",
        NULL,
        0,
        menu_item_hop,
        jammer);
    menu_add_item(
        menu,
        "Dwell (ms)",
        NULL,
        0,
        menu_item_dwell,
        jammer);
    menu_add_item(
        menu,
        "Latch-on",
        NULL,
        0,
        menu_item_latch,
        jammer);
    menu_add_item(
        menu,
        "START",
        NULL,
        0,
        menu_item_start,
        jammer);

    view_dispatcher_add_view(
        jammer->view_dispatcher, SubGhzJammerViewMenu, menu_get_view(menu));
    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewMenu);
}

bool subghz_jammer_scene_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    if(event.type == SceneManagerEventTypeBack) {
        return false;
    }

    return false;
}

void subghz_jammer_scene_menu_on_exit(void* context) {
    Jammer* jammer = (Jammer*)context;
    view_dispatcher_remove_view(jammer->view_dispatcher, SubGhzJammerViewMenu);
}
