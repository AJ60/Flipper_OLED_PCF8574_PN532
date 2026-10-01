#include <furi_hal.h>

#include "../desktop_i.h"

#define DesktopFaultEventExit 0x00FF00FF

void desktop_scene_fault_callback(void* context) {
    Desktop* desktop = (Desktop*)context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopFaultEventExit);
}

void desktop_scene_fault_on_enter(void* context) {
    Desktop* desktop = (Desktop*)context;

    Popup* popup = desktop->popup;
    popup_set_context(popup, desktop);
    popup_set_header(
        popup,
        "Flipper crashed\n and was rebooted",
        64,
        14 + STATUS_BAR_Y_SHIFT,
        AlignCenter,
        AlignCenter);

    char* message = (char*)furi_hal_rtc_get_fault_data();
    popup_set_text(popup, message, 64, 37 + STATUS_BAR_Y_SHIFT, AlignCenter, AlignCenter);
    popup_set_callback(popup, desktop_scene_fault_callback);

    // Auto-dismiss after 3 seconds so the device never stays stuck on this screen
    popup_set_timeout(popup, 3000);
    popup_enable_timeout(popup);

    // Bug #14 fix: do NOT clear fault data here — the on_exit handler already clears it
    // after the popup is dismissed. Clearing in on_enter made the RTC fault register
    // appear empty to any diagnostic tool the instant the screen was shown.

    view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdPopup);
}
bool desktop_scene_fault_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = (Desktop*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DesktopFaultEventExit:
            scene_manager_previous_scene(desktop->scene_manager);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void desktop_scene_fault_on_exit(void* context) {
    Desktop* desktop = (Desktop*)context;
    furi_assert(desktop);

    Popup* popup = desktop->popup;
    popup_reset(popup);

    furi_hal_rtc_set_fault_data(0);
}
