#include <furi_hal_nfc_i.h>

FuriHalNfcEventInternal* furi_hal_nfc_event = NULL;

void furi_hal_nfc_event_init(void) {
    furi_hal_nfc_event = malloc(sizeof(FuriHalNfcEventInternal));
    furi_check(furi_hal_nfc_event);
}

FuriHalNfcError furi_hal_nfc_event_start(void) {
    furi_check(furi_hal_nfc_event);

    furi_hal_nfc_event->thread = furi_thread_get_current_id();
    furi_thread_flags_clear(FURI_HAL_NFC_EVENT_INTERNAL_ALL);

    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_event_stop(void) {
    furi_check(furi_hal_nfc_event);

    furi_hal_nfc_event->thread = NULL;

    return FuriHalNfcErrorNone;
}

void furi_hal_nfc_event_set(FuriHalNfcEventInternalType event) {
    furi_check(furi_hal_nfc_event);

    if(furi_hal_nfc_event->thread) {
        furi_thread_flags_set(furi_hal_nfc_event->thread, event);
    }
}

FuriHalNfcError furi_hal_nfc_abort(void) {
    furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeAbort);
    return FuriHalNfcErrorNone;
}

FuriHalNfcEvent furi_hal_nfc_wait_event_common(uint32_t timeout_ms) {
    furi_check(furi_hal_nfc_event);
    furi_check(furi_hal_nfc_event->thread);

    FuriHalNfcEvent event = 0;
    uint32_t event_timeout = timeout_ms == FURI_HAL_NFC_EVENT_WAIT_FOREVER ? FuriWaitForever :
                                                                             timeout_ms;
    uint32_t event_flag =
        furi_thread_flags_wait(FURI_HAL_NFC_EVENT_INTERNAL_ALL, FuriFlagWaitAny, event_timeout);
    if(event_flag != (unsigned)FuriFlagErrorTimeout) {
        if(event_flag & FuriHalNfcEventInternalTypeIrq) {
            furi_thread_flags_clear(FuriHalNfcEventInternalTypeIrq);
            event |= FuriHalNfcEventTxEnd | FuriHalNfcEventRxStart | FuriHalNfcEventRxEnd |
                     FuriHalNfcEventListenerActive;
        }
        if(event_flag & FuriHalNfcEventInternalTypeTimerFwtExpired) {
            event |= FuriHalNfcEventTimerFwtExpired;
            furi_thread_flags_clear(FuriHalNfcEventInternalTypeTimerFwtExpired);
        }
        if(event_flag & FuriHalNfcEventInternalTypeTimerBlockTxExpired) {
            event |= FuriHalNfcEventTimerBlockTxExpired;
            furi_thread_flags_clear(FuriHalNfcEventInternalTypeTimerBlockTxExpired);
        }
        if(event_flag & FuriHalNfcEventInternalTypeAbort) {
            event |= FuriHalNfcEventAbortRequest;
            furi_thread_flags_clear(FuriHalNfcEventInternalTypeAbort);
        }
    } else {
        event = FuriHalNfcEventTimeout;
    }

    return event;
}

bool furi_hal_nfc_event_wait_for_specific_irq(
    const FuriHalSpiBusHandle* handle,
    uint32_t mask,
    uint32_t timeout_ms) {
    UNUSED(handle);
    UNUSED(mask);
    UNUSED(timeout_ms);
    return false;
}
