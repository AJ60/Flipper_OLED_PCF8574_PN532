#include "furi_hal_nfc_i.h"
#include "furi_hal_nfc_tech_i.h"
#include <furi_hal_resources.h>

#define TAG "FuriHalIso15693"

// Derived experimentally
#define FURI_HAL_NFC_ISO15693_POLLER_FWT_COMP_FC   (-1300)
#define FURI_HAL_NFC_ISO15693_LISTENER_FDT_COMP_FC (2850)

static FuriHalNfcError furi_hal_nfc_iso15693_poller_init(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    FURI_LOG_W(TAG, "ISO15693 poller not supported on PN532 hardware");
    return FuriHalNfcErrorCommunication;
}

static FuriHalNfcError furi_hal_nfc_iso15693_poller_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso15693_poller_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    UNUSED(handle);
    UNUSED(tx_data);
    UNUSED(tx_bits);
    return FuriHalNfcErrorCommunication;
}

static FuriHalNfcError furi_hal_nfc_iso15693_poller_rx(
    const FuriHalSpiBusHandle* handle,
    uint8_t* rx_data,
    size_t rx_data_size,
    size_t* rx_bits) {
    UNUSED(handle);
    UNUSED(rx_data);
    UNUSED(rx_data_size);
    UNUSED(rx_bits);
    return FuriHalNfcErrorCommunication;
}

static FuriHalNfcError furi_hal_nfc_iso15693_listener_init(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    FURI_LOG_W(TAG, "ISO15693 listener not supported on PN532");
    return FuriHalNfcErrorCommunication;
}

static FuriHalNfcError furi_hal_nfc_iso15693_listener_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcEvent furi_hal_nfc_iso15693_wait_event(uint32_t timeout_ms) {
    UNUSED(timeout_ms);
    return FuriHalNfcEventTimeout;
}

static FuriHalNfcError furi_hal_nfc_iso15693_listener_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    UNUSED(handle);
    UNUSED(tx_data);
    UNUSED(tx_bits);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso15693_listener_tx_sof(void) {
    return FuriHalNfcErrorCommunication;
}

static FuriHalNfcError furi_hal_nfc_iso15693_listener_rx(
    const FuriHalSpiBusHandle* handle,
    uint8_t* rx_data,
    size_t rx_data_size,
    size_t* rx_bits) {
    UNUSED(handle);
    UNUSED(rx_data);
    UNUSED(rx_data_size);
    UNUSED(rx_bits);
    return FuriHalNfcErrorCommunication;
}

static FuriHalNfcError furi_hal_nfc_iso15693_listener_sleep(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

const FuriHalNfcTechBase furi_hal_nfc_iso15693 = {
    .poller =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_POLLER_FDT_COMP_FC,
                    .fwt = FURI_HAL_NFC_ISO15693_POLLER_FWT_COMP_FC,
                },
            .init = furi_hal_nfc_iso15693_poller_init,
            .deinit = furi_hal_nfc_iso15693_poller_deinit,
            .wait_event = furi_hal_nfc_wait_event_common,
            .tx = furi_hal_nfc_iso15693_poller_tx,
            .rx = furi_hal_nfc_iso15693_poller_rx,
        },

    .listener =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_ISO15693_LISTENER_FDT_COMP_FC,
                },
            .init = furi_hal_nfc_iso15693_listener_init,
            .deinit = furi_hal_nfc_iso15693_listener_deinit,
            .wait_event = furi_hal_nfc_iso15693_wait_event,
            .tx = furi_hal_nfc_iso15693_listener_tx,
            .rx = furi_hal_nfc_iso15693_listener_rx,
            .sleep = furi_hal_nfc_iso15693_listener_sleep,
            .idle = furi_hal_nfc_iso15693_listener_sleep,
        },
};
