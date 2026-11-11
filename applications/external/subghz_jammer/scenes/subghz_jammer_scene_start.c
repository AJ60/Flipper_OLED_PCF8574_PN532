#include <furi.h>
#include <furi_hal_subghz.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>

#include "subghz_jammer.h"
#include "scenes/subghz_jammer_scene.h"

static void format_status(char* buffer, size_t buf_size, Jammer* jammer) {
    const char* mode_str = "Unknown";
    switch(jammer->settings.mode) {
    case JammerSignalModeCarrier:
        mode_str = "Carrier";
        break;
    case JammerSignalModeNoise:
        mode_str = "Noise";
        break;
    case JammerSignalModeSweep:
        mode_str = "Sweep";
        break;
    case JammerSignalModePingPong:
        mode_str = "PingPong";
        break;
    case JammerSignalModePulse:
        mode_str = "Pulse";
        break;
    default:
        break;
    }

    const char* radio_str = "Auto";
    switch(jammer->settings.radio) {
    case JammerRadioAuto:
        radio_str = "Auto";
        break;
    case JammerRadioInternal:
        radio_str = "Internal";
        break;
    case JammerRadioExternalCC1101:
        radio_str = "External";
        break;
    }

    uint32_t freq_mhz = jammer->settings.frequency / 1000000;
    uint32_t freq_khz = (jammer->settings.frequency % 1000000) / 1000;

    char duty_line[32] = "";
    if(jammer->settings.mode == JammerSignalModePulse) {
        uint32_t duty_pct =
            jammer->settings.pulse_ratio > 100 ? 100u : jammer->settings.pulse_ratio;
        snprintf(
            duty_line,
            sizeof(duty_line),
            "\nDuty: %lu%%",
            (unsigned long)duty_pct);
    }

    char power_line[16];
    if(jammer->settings.power_level == 0) {
        snprintf(power_line, sizeof(power_line), "Auto");
    } else {
        snprintf(power_line, sizeof(power_line), "%lu%%", (unsigned long)jammer->settings.power_level);
    }

    const char* status_str = "IDLE";
    if(jammer->tx_error) {
        status_str = "BLOCKED";
    } else if(jammer->tx_active) {
        status_str = "TRANSMITTING";
    }

    snprintf(
        buffer,
        buf_size,
        "Mode: %s\nFreq: %lu.%03lu MHz\nRadio: %s%s\nPower: %s\nHops: %lu\nTemp: %lu C / %lu mA\nStatus: %s",
        mode_str,
        (unsigned long)freq_mhz,
        (unsigned long)freq_khz,
        radio_str,
        duty_line,
        power_line,
        (unsigned long)jammer->settings.hop_count,
        (unsigned long)jammer->thermal.temp_c,
        (unsigned long)jammer->thermal.avg_current_ma,
        status_str);
}

static void jammer_start_button_callback(GuiButtonType result, InputType type, void* context) {
    UNUSED(result);
    Jammer* jammer = (Jammer*)context;

    if(jammer->tx_error) {
        if(type == InputTypeShort) {
            scene_manager_search_and_switch_to_previous_scene(
                jammer->scene_manager, SubGhzJammerScenemenu);
        }
        return;
    }

    if(jammer->settings.latch) {
        if(type == InputTypeShort) {
            view_dispatcher_send_custom_event(jammer->view_dispatcher, SubGhzJammerEventStop);
        }
    } else {
        if(type == InputTypePress) {
            if(furi_hal_subghz_check_tx(jammer->settings.frequency) != SubGhzTxAllowed) {
                view_dispatcher_send_custom_event(
                    jammer->view_dispatcher, SubGhzJammerEventTxError);
                return;
            }
            jammer_start_worker(jammer);
            if(jammer->notifications) {
                notification_message(jammer->notifications, &sequence_set_red_255);
            }
        } else if(type == InputTypeRelease) {
            jammer_stop_worker(jammer);
            if(jammer->notifications) {
                notification_message(jammer->notifications, &sequence_reset_red);
            }
        }
    }
}

void subghz_jammer_scene_start_on_enter(void* context) {
    Jammer* jammer = (Jammer*)context;
    jammer->tx_error = false;
    Widget* widget = jammer->widget_start;
    widget_reset(widget);

    if(jammer->settings.latch) {
        // Latch mode holds TX continuously, so gate on the region check here
        // (matching the non-latch hold-to-jam path at :97) before committing
        // to transmit. On failure, show the blocked state instead of starting.
        if(furi_hal_subghz_check_tx(jammer->settings.frequency) != SubGhzTxAllowed) {
            jammer->tx_error = true;
            Widget* widget = jammer->widget_start;
            widget_reset(widget);
            widget_add_string_element(
                widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, "TX Error / Blocked");
            widget_add_string_element(
                widget, 64, 24, AlignCenter, AlignCenter, FontSecondary, "Region restricted or");
            widget_add_string_element(
                widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, "radio init failed.");
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Back", jammer_start_button_callback, jammer);
            view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewStart);
            return;
        }
        jammer_start_worker(jammer);
        if(jammer->notifications) {
            notification_message(jammer->notifications, &sequence_set_red_255);
        }
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Stop", jammer_start_button_callback, jammer);
    } else {
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Hold OK", jammer_start_button_callback, jammer);
    }

    char buffer[256];
    format_status(buffer, sizeof(buffer), jammer);
    widget_add_text_box_element(
        widget, 0, 0, 128, 52, AlignLeft, AlignTop, buffer, false);

    view_dispatcher_switch_to_view(jammer->view_dispatcher, SubGhzJammerViewStart);
}

bool subghz_jammer_scene_start_on_event(void* context, SceneManagerEvent event) {
    Jammer* jammer = (Jammer*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzJammerEventStop) {
            jammer_stop_worker(jammer);
            if(jammer->notifications) {
                notification_message(jammer->notifications, &sequence_reset_red);
            }
            scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenestop);
            consumed = true;
        } else if(event.event == SubGhzJammerEventTxError) {
            jammer_stop_worker(jammer);
            if(jammer->notifications) {
                notification_message(jammer->notifications, &sequence_reset_red);
            }
            Widget* widget = jammer->widget_start;
            widget_reset(widget);
            widget_add_string_element(
                widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, "TX Error / Blocked");
            widget_add_string_element(
                widget, 64, 24, AlignCenter, AlignCenter, FontSecondary, "Region restricted or");
            widget_add_string_element(
                widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, "radio init failed.");
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Back", jammer_start_button_callback, jammer);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        jammer_stop_worker(jammer);
        if(jammer->notifications) {
            notification_message(jammer->notifications, &sequence_reset_red);
        }
        if(jammer->tx_error) {
            scene_manager_search_and_switch_to_previous_scene(
                jammer->scene_manager, SubGhzJammerScenemenu);
        } else {
            scene_manager_next_scene(jammer->scene_manager, SubGhzJammerScenestop);
        }
        consumed = true;
    }

    return consumed;
}

void subghz_jammer_scene_start_on_exit(void* context) {
    Jammer* jammer = (Jammer*)context;
    jammer_stop_worker(jammer);
    if(jammer->notifications) {
        notification_message(jammer->notifications, &sequence_reset_red);
    }
    widget_reset(jammer->widget_start);
}
