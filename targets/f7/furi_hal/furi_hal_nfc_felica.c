#include "furi_hal_nfc_i.h"
#include "furi_hal_nfc_tech_i.h"

#define FURI_HAL_NFC_FELICA_LISTENER_FDT_COMP_FC (0)

#define FURI_HAL_FELICA_COMMUNICATION_PERFORMANCE (0x0083U)
#define FURI_HAL_FELICA_RESPONSE_CODE             (0x01)
#define FURI_HAL_FELICA_IDM_PMM_LENGTH            (8)

static FuriHalNfcError furi_hal_nfc_felica_poller_init(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_felica_poller_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_felica_listener_init(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    furi_hal_pn532_ctx.target_activated = false;
    furi_hal_pn532_ctx.rx_len = 0;
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_felica_listener_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    furi_hal_pn532_abort();
    furi_hal_pn532_ctx.target_activated = false;
    furi_hal_pn532_ctx.rx_len = 0;
    return FuriHalNfcErrorNone;
}

static FuriHalNfcEvent furi_hal_nfc_felica_listener_wait_event(uint32_t timeout_ms) {
    UNUSED(timeout_ms);
    uint32_t flags = furi_thread_flags_get();
    if(flags & FuriHalNfcEventInternalTypeAbort) {
        furi_thread_flags_clear(FuriHalNfcEventInternalTypeAbort);
        furi_hal_pn532_abort();
        return FuriHalNfcEventAbortRequest;
    }

    if(furi_hal_nfc_acquire() != FuriHalNfcErrorNone) {
        return FuriHalNfcEventTimeout;
    }

    uint8_t rx_cmd[260];
    size_t rx_cmd_len = sizeof(rx_cmd);

    if(!furi_hal_pn532_ctx.target_activated) {
        Pn532Error pn_err = pn532_tg_init_as_target(
            PN532_I2C_BUS,
            furi_hal_pn532_ctx.target_params,
            furi_hal_pn532_ctx.target_params_len,
            rx_cmd,
            &rx_cmd_len,
            150);

        if(pn_err == Pn532ErrorTimeout) {
            furi_hal_pn532_abort();
        }
        furi_hal_nfc_release();

        flags = furi_thread_flags_get();
        if(flags & FuriHalNfcEventInternalTypeAbort) {
            furi_thread_flags_clear(FuriHalNfcEventInternalTypeAbort);
            furi_hal_pn532_abort();
            return FuriHalNfcEventAbortRequest;
        }

        if(pn_err == Pn532ErrorNone) {
            furi_hal_pn532_ctx.target_activated = true;
            if(rx_cmd_len > 1) {
                size_t payload_len = rx_cmd_len - 1;
                if(payload_len > sizeof(furi_hal_pn532_ctx.rx_buf)) {
                    payload_len = sizeof(furi_hal_pn532_ctx.rx_buf);
                }
                memcpy(furi_hal_pn532_ctx.rx_buf, &rx_cmd[1], payload_len);
                furi_hal_pn532_ctx.rx_len = payload_len;
                return FuriHalNfcEventListenerActive | FuriHalNfcEventRxEnd;
            } else {
                furi_hal_pn532_ctx.rx_len = 0;
                return FuriHalNfcEventListenerActive;
            }
        }
        return FuriHalNfcEventTimeout;
    } else {
        Pn532Error pn_err = pn532_tg_get_data(PN532_I2C_BUS, rx_cmd, &rx_cmd_len, 150);

        if(pn_err == Pn532ErrorTimeout) {
            furi_hal_pn532_abort();
        }
        furi_hal_nfc_release();

        flags = furi_thread_flags_get();
        if(flags & FuriHalNfcEventInternalTypeAbort) {
            furi_thread_flags_clear(FuriHalNfcEventInternalTypeAbort);
            furi_hal_pn532_abort();
            return FuriHalNfcEventAbortRequest;
        }

        if(pn_err == Pn532ErrorNone) {
            if(rx_cmd[0] == 0x00 && rx_cmd_len > 1) {
                size_t payload_len = rx_cmd_len - 1;
                if(payload_len > sizeof(furi_hal_pn532_ctx.rx_buf)) {
                    payload_len = sizeof(furi_hal_pn532_ctx.rx_buf);
                }
                memcpy(furi_hal_pn532_ctx.rx_buf, &rx_cmd[1], payload_len);
                furi_hal_pn532_ctx.rx_len = payload_len;
                return FuriHalNfcEventRxEnd;
            } else if(rx_cmd[0] != 0x00) {
                furi_hal_pn532_ctx.target_activated = false;
                furi_hal_pn532_ctx.rx_len = 0;
                return FuriHalNfcEventFieldOff;
            }
        } else if(pn_err != Pn532ErrorTimeout) {
            furi_hal_pn532_ctx.target_activated = false;
            furi_hal_pn532_ctx.rx_len = 0;
            return FuriHalNfcEventFieldOff;
        }
        return FuriHalNfcEventTimeout;
    }
}

FuriHalNfcError furi_hal_nfc_felica_listener_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    UNUSED(handle);
    if(tx_bits == 0 || !tx_data) return FuriHalNfcErrorNone;
    size_t tx_bytes = (tx_bits + 7) / 8;
    furi_hal_nfc_acquire();
    Pn532Error err = pn532_tg_set_data(PN532_I2C_BUS, tx_data, tx_bytes, 200);
    furi_hal_nfc_release();
    return (err == Pn532ErrorNone) ? FuriHalNfcErrorNone : FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_felica_listener_sleep(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_felica_listener_idle(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_felica_listener_set_sensf_res_data(
    const uint8_t* idm,
    const uint8_t idm_len,
    const uint8_t* pmm,
    const uint8_t pmm_len,
    const uint16_t sys_code) {
    furi_check(idm);
    furi_check(pmm);
    furi_check(idm_len == FURI_HAL_FELICA_IDM_PMM_LENGTH);
    furi_check(pmm_len == FURI_HAL_FELICA_IDM_PMM_LENGTH);

    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    // Setup 38-byte target parameters for TgInitAsTarget
    // Bytes 0..5: MifareParams dummy header
    furi_hal_pn532_ctx.target_params[0] = 0x01;
    furi_hal_pn532_ctx.target_params[1] = 0x01;
    furi_hal_pn532_ctx.target_params[2] = 0x01;
    furi_hal_pn532_ctx.target_params[3] = 0x02;
    furi_hal_pn532_ctx.target_params[4] = 0x03;
    furi_hal_pn532_ctx.target_params[5] = 0x20;
    // Bytes 6..13: FeliCaParams NFCID2t (IDm, 8 bytes)
    memcpy(&furi_hal_pn532_ctx.target_params[6], idm, 8);
    // Bytes 14..21: FeliCaParams PAD (PMm, 8 bytes)
    memcpy(&furi_hal_pn532_ctx.target_params[14], pmm, 8);
    // Bytes 22..23: SystemCode (2 bytes, big-endian)
    furi_hal_pn532_ctx.target_params[22] = (uint8_t)((sys_code >> 8) & 0xFF);
    furi_hal_pn532_ctx.target_params[23] = (uint8_t)(sys_code & 0xFF);
    // Bytes 24..37: NFCID3t (10 bytes) + General bytes (0)
    memset(&furi_hal_pn532_ctx.target_params[24], 0, 14);
    furi_hal_pn532_ctx.target_params_len = 38;
    furi_hal_pn532_ctx.target_activated = false;
    furi_hal_pn532_ctx.rx_len = 0;

    furi_hal_nfc_release();

    return FuriHalNfcErrorNone;
}

const FuriHalNfcTechBase furi_hal_nfc_felica = {
    .poller =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_POLLER_FDT_COMP_FC,
                    .fwt = FURI_HAL_NFC_POLLER_FWT_COMP_FC,
                },
            .init = furi_hal_nfc_felica_poller_init,
            .deinit = furi_hal_nfc_felica_poller_deinit,
            .wait_event = furi_hal_nfc_wait_event_common,
            .tx = furi_hal_nfc_poller_tx_common,
            .rx = furi_hal_nfc_common_fifo_rx,
        },

    .listener =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_FELICA_LISTENER_FDT_COMP_FC,
                },
            .init = furi_hal_nfc_felica_listener_init,
            .deinit = furi_hal_nfc_felica_listener_deinit,
            .wait_event = furi_hal_nfc_felica_listener_wait_event,
            .tx = furi_hal_nfc_felica_listener_tx,
            .rx = furi_hal_nfc_common_fifo_rx,
            .sleep = furi_hal_nfc_felica_listener_sleep,
            .idle = furi_hal_nfc_felica_listener_idle,
        },
};
