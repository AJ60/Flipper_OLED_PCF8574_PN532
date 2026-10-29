#include "furi_hal_nfc_i.h"
#include "furi_hal_nfc_tech_i.h"

static FuriHalNfcError furi_hal_nfc_iso14443b_listener_init(const FuriHalSpiBusHandle* handle);
static FuriHalNfcError furi_hal_nfc_iso14443b_listener_deinit(const FuriHalSpiBusHandle* handle);
static FuriHalNfcError furi_hal_nfc_iso14443b_listener_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits);
static FuriHalNfcError furi_hal_nfc_iso14443b_listener_rx(
    const FuriHalSpiBusHandle* handle,
    uint8_t* rx_data,
    size_t rx_data_size,
    size_t* rx_bits);
static FuriHalNfcError furi_hal_nfc_iso14443b_listener_sleep(const FuriHalSpiBusHandle* handle);
static FuriHalNfcError furi_hal_nfc_iso14443b_listener_idle(const FuriHalSpiBusHandle* handle);

static FuriHalNfcError furi_hal_nfc_iso14443b_poller_init(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso14443b_poller_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso14443b_listener_init(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso14443b_listener_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso14443b_listener_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    UNUSED(handle);
    size_t tx_bytes = (tx_bits + 7) / 8;
    Pn532Error pn_err = pn532_tg_set_data(PN532_I2C_BUS, tx_data, tx_bytes, 300);
    return (pn_err == Pn532ErrorNone) ? FuriHalNfcErrorNone : FuriHalNfcErrorCommunication;
}

static FuriHalNfcError furi_hal_nfc_iso14443b_listener_rx(
    const FuriHalSpiBusHandle* handle,
    uint8_t* rx_data,
    size_t rx_data_size,
    size_t* rx_bits) {
    UNUSED(handle);
    size_t rx_len = rx_data_size;
    Pn532Error pn_err = pn532_tg_get_data(PN532_I2C_BUS, rx_data, &rx_len, 500);
    if(pn_err == Pn532ErrorNone) {
        *rx_bits = rx_len * 8;
        return FuriHalNfcErrorNone;
    }
    return FuriHalNfcErrorCommunication;
}

static FuriHalNfcError furi_hal_nfc_iso14443b_listener_sleep(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso14443b_listener_idle(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

const FuriHalNfcTechBase furi_hal_nfc_iso14443b = {
    .poller =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_POLLER_FDT_COMP_FC,
                    .fwt = FURI_HAL_NFC_POLLER_FWT_COMP_FC,
                },
            .init = furi_hal_nfc_iso14443b_poller_init,
            .deinit = furi_hal_nfc_iso14443b_poller_deinit,
            .wait_event = furi_hal_nfc_wait_event_common,
            .tx = furi_hal_nfc_poller_tx_common,
            .rx = furi_hal_nfc_common_fifo_rx,
        },

    .listener =
        {
            .init = furi_hal_nfc_iso14443b_listener_init,
            .deinit = furi_hal_nfc_iso14443b_listener_deinit,
            .tx = furi_hal_nfc_iso14443b_listener_tx,
            .rx = furi_hal_nfc_iso14443b_listener_rx,
            .sleep = furi_hal_nfc_iso14443b_listener_sleep,
            .idle = furi_hal_nfc_iso14443b_listener_idle,
        },
};
