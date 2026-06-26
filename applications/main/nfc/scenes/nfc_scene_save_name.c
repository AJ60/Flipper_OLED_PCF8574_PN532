#include "../nfc_app_i.h"
#include "../helpers/protocol_support/nfc_protocol_support.h"

void nfc_scene_save_name_on_enter(void* context) {
    NfcApp* instance = context;
    if(furi_hal_nfc_is_mine()) {
        furi_hal_nfc_release();
    }
    instance->nfc_hal_acquired = false;

    nfc_protocol_support_on_enter(NfcProtocolSupportSceneSaveName, context);
}

bool nfc_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    return nfc_protocol_support_on_event(NfcProtocolSupportSceneSaveName, context, event);
}

void nfc_scene_save_name_on_exit(void* context) {
    NfcApp* instance = context;
    nfc_protocol_support_on_exit(NfcProtocolSupportSceneSaveName, context);

    if(!furi_hal_nfc_is_mine()) {
        furi_check(furi_hal_nfc_acquire() == FuriHalNfcErrorNone);
    }
    instance->nfc_hal_acquired = true;
}
