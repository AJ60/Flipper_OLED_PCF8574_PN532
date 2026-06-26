#include "../nfc_app_i.h"
#include <dolphin/dolphin.h>

#define NFC_SCENE_DETECT_STATE_IDLE     0U
#define NFC_SCENE_DETECT_STATE_DETECTED 1U

void nfc_scene_detect_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);

    NfcApp* instance = context;

    if(event.type == NfcScannerEventTypeDetected) {
        if(scene_manager_get_scene_state(instance->scene_manager, NfcSceneDetect) ==
           NFC_SCENE_DETECT_STATE_DETECTED) {
            return;
        }

        scene_manager_set_scene_state(
            instance->scene_manager, NfcSceneDetect, NFC_SCENE_DETECT_STATE_DETECTED);
        nfc_detected_protocols_set(
            instance->detected_protocols, event.data.protocols, event.data.protocol_num);
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventWorkerExit);
    }
}

void nfc_scene_detect_on_enter(void* context) {
    NfcApp* instance = context;
    scene_manager_set_scene_state(instance->scene_manager, NfcSceneDetect, NFC_SCENE_DETECT_STATE_IDLE);

    // Setup view
    popup_reset(instance->popup);
    popup_set_header(instance->popup, "Reading", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        instance->popup, "Hold card next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcViewPopup);

    nfc_detected_protocols_reset(instance->detected_protocols);

    // Scanner runs in its own worker thread and acquires the NFC HAL there.
    // Release the GUI-thread lock first, otherwise the worker's acquire blocks
    // and the internal furi_check crashes/freezes the UI. Re-acquired in on_exit.
    if(furi_hal_nfc_is_mine()) {
        furi_hal_nfc_release();
    }
    instance->nfc_hal_acquired = false;

    instance->scanner = nfc_scanner_alloc(instance->nfc);
    nfc_scanner_start(instance->scanner, nfc_scene_detect_scan_callback, instance);

    nfc_blink_detect_start(instance);
}

bool nfc_scene_detect_on_event(void* context, SceneManagerEvent event) {
    NfcApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventWorkerExit) {
            if(nfc_detected_protocols_get_num(instance->detected_protocols) > 1) {
                notification_message(instance->notifications, &sequence_single_vibro);
                scene_manager_next_scene(instance->scene_manager, NfcSceneSelectProtocol);
            } else {
                scene_manager_next_scene(instance->scene_manager, NfcSceneRead);
            }

            consumed = true;
        }
    }

    return consumed;
}

void nfc_scene_detect_on_exit(void* context) {
    NfcApp* instance = context;

    scene_manager_set_scene_state(instance->scene_manager, NfcSceneDetect, NFC_SCENE_DETECT_STATE_IDLE);

    if(instance->scanner) {
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    // Re-acquire the HAL for the GUI thread now that the worker is gone.
    if(!furi_hal_nfc_is_mine()) {
        furi_check(furi_hal_nfc_acquire() == FuriHalNfcErrorNone);
    }
    instance->nfc_hal_acquired = true;

    popup_reset(instance->popup);

    nfc_blink_stop(instance);
}
