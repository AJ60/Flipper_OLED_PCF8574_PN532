#include "furi_hal_nfc_i.h"
#include "furi_hal_nfc_tech_i.h"

#include <furi.h>
#include <furi_hal_resources.h>



#define TAG "FuriHalIso14443a"

// Prevent FDT timer from starting
#define FURI_HAL_NFC_ISO14443A_LISTENER_FDT_COMP_FC (INT32_MAX)

static FuriHalNfcError furi_hal_nfc_iso14443a_poller_init(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso14443a_poller_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso14443a_listener_init(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_iso14443a_listener_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    furi_hal_pn532_abort();
    furi_hal_pn532_ctx.target_activated = false;
    furi_hal_pn532_ctx.rx_len = 0;
    return FuriHalNfcErrorNone;
}

static FuriHalNfcEvent furi_hal_nfc_iso14443_3a_listener_wait_event(uint32_t timeout_ms) {
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
            if(rx_cmd_len > 1 && rx_cmd[0] == 0x00) {
                size_t payload_len = rx_cmd_len - 1;
                if(payload_len > sizeof(furi_hal_pn532_ctx.rx_buf)) {
                    payload_len = sizeof(furi_hal_pn532_ctx.rx_buf);
                }
                memcpy(furi_hal_pn532_ctx.rx_buf, &rx_cmd[1], payload_len);
                furi_hal_pn532_ctx.rx_len = payload_len;
                return FuriHalNfcEventRxEnd;
            } else if(rx_cmd_len > 0 && rx_cmd[0] != 0x00) {
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

FuriHalNfcError furi_hal_nfc_iso14443a_poller_trx_short_frame(FuriHalNfcaShortFrame frame) {
    FuriHalNfcError error = FuriHalNfcErrorNone;

    UNUSED(frame);
    error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    Pn532Error pn_err =
        pn532_in_list_passive_target_iso14443a(PN532_I2C_BUS, &furi_hal_pn532_ctx.target, 100);
    if(pn_err == Pn532ErrorNone) {
        furi_hal_pn532_ctx.target_detected = true;
        // The ATQA (SENS_RES) is 2 bytes:
        furi_hal_pn532_ctx.rx_buf[0] = (uint8_t)(furi_hal_pn532_ctx.target.sens_res & 0xFF);
        furi_hal_pn532_ctx.rx_buf[1] = (uint8_t)(furi_hal_pn532_ctx.target.sens_res >> 8);
        furi_hal_pn532_ctx.rx_len = 2;
        furi_hal_pn532_ctx.sdd_cascade_level = 0;
        furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
        error = FuriHalNfcErrorNone;
    } else if(pn_err == Pn532ErrorTimeout) {
        furi_hal_pn532_ctx.target_detected = false;
        error = FuriHalNfcErrorCommunicationTimeout;
    } else {
        furi_hal_pn532_ctx.target_detected = false;
        error = FuriHalNfcErrorCommunication;
    }

    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_iso14443a_tx_sdd_frame(const uint8_t* tx_data, size_t tx_bits) {
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    if(furi_hal_pn532_ctx.target_detected && tx_data != NULL) {
        if(tx_data[0] == 0x93) {
            // Cascade level 1
            if(furi_hal_pn532_ctx.target.uid_len == 4) {
                furi_hal_pn532_ctx.rx_buf[0] = furi_hal_pn532_ctx.target.uid[0];
                furi_hal_pn532_ctx.rx_buf[1] = furi_hal_pn532_ctx.target.uid[1];
                furi_hal_pn532_ctx.rx_buf[2] = furi_hal_pn532_ctx.target.uid[2];
                furi_hal_pn532_ctx.rx_buf[3] = furi_hal_pn532_ctx.target.uid[3];
                furi_hal_pn532_ctx.rx_buf[4] =
                    furi_hal_pn532_ctx.target.uid[0] ^ furi_hal_pn532_ctx.target.uid[1] ^
                    furi_hal_pn532_ctx.target.uid[2] ^ furi_hal_pn532_ctx.target.uid[3];
            } else {
                // 7-byte UID: Cascade tag CT (0x88) + 3 bytes
                furi_hal_pn532_ctx.rx_buf[0] = 0x88;
                furi_hal_pn532_ctx.rx_buf[1] = furi_hal_pn532_ctx.target.uid[0];
                furi_hal_pn532_ctx.rx_buf[2] = furi_hal_pn532_ctx.target.uid[1];
                furi_hal_pn532_ctx.rx_buf[3] = furi_hal_pn532_ctx.target.uid[2];
                furi_hal_pn532_ctx.rx_buf[4] = 0x88 ^ furi_hal_pn532_ctx.target.uid[0] ^
                                               furi_hal_pn532_ctx.target.uid[1] ^
                                               furi_hal_pn532_ctx.target.uid[2];
            }
            furi_hal_pn532_ctx.rx_len = 5;
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
        } else if(tx_data[0] == 0x95) {
            // Cascade level 2
            if(furi_hal_pn532_ctx.target.uid_len == 10) {
                // 10-byte UID: Cascade tag CT (0x88) + next 3 bytes
                furi_hal_pn532_ctx.rx_buf[0] = 0x88;
                furi_hal_pn532_ctx.rx_buf[1] = furi_hal_pn532_ctx.target.uid[3];
                furi_hal_pn532_ctx.rx_buf[2] = furi_hal_pn532_ctx.target.uid[4];
                furi_hal_pn532_ctx.rx_buf[3] = furi_hal_pn532_ctx.target.uid[5];
                furi_hal_pn532_ctx.rx_buf[4] = 0x88 ^ furi_hal_pn532_ctx.target.uid[3] ^
                                               furi_hal_pn532_ctx.target.uid[4] ^
                                               furi_hal_pn532_ctx.target.uid[5];
            } else {
                // 7-byte UID: Remaining 4 bytes
                furi_hal_pn532_ctx.rx_buf[0] = furi_hal_pn532_ctx.target.uid[3];
                furi_hal_pn532_ctx.rx_buf[1] = furi_hal_pn532_ctx.target.uid[4];
                furi_hal_pn532_ctx.rx_buf[2] = furi_hal_pn532_ctx.target.uid[5];
                furi_hal_pn532_ctx.rx_buf[3] = furi_hal_pn532_ctx.target.uid[6];
                furi_hal_pn532_ctx.rx_buf[4] =
                    furi_hal_pn532_ctx.target.uid[3] ^ furi_hal_pn532_ctx.target.uid[4] ^
                    furi_hal_pn532_ctx.target.uid[5] ^ furi_hal_pn532_ctx.target.uid[6];
            }
            furi_hal_pn532_ctx.rx_len = 5;
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
        } else if(tx_data[0] == 0x97) {
            // Cascade level 3 (10-byte UID)
            furi_hal_pn532_ctx.rx_buf[0] = furi_hal_pn532_ctx.target.uid[6];
            furi_hal_pn532_ctx.rx_buf[1] = furi_hal_pn532_ctx.target.uid[7];
            furi_hal_pn532_ctx.rx_buf[2] = furi_hal_pn532_ctx.target.uid[8];
            furi_hal_pn532_ctx.rx_buf[3] = furi_hal_pn532_ctx.target.uid[9];
            furi_hal_pn532_ctx.rx_buf[4] =
                furi_hal_pn532_ctx.target.uid[6] ^ furi_hal_pn532_ctx.target.uid[7] ^
                furi_hal_pn532_ctx.target.uid[8] ^ furi_hal_pn532_ctx.target.uid[9];
            furi_hal_pn532_ctx.rx_len = 5;
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
        }
        furi_hal_nfc_release();
        return FuriHalNfcErrorNone;
    }
    furi_hal_nfc_release();
    return furi_hal_nfc_poller_tx(tx_data, tx_bits);
}

FuriHalNfcError
    furi_hal_nfc_iso14443a_rx_sdd_frame(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits) {
    FuriHalNfcError error = FuriHalNfcErrorNone;
    error = furi_hal_nfc_poller_rx(rx_data, rx_data_size, rx_bits);
    return error;
}

FuriHalNfcError
    furi_hal_nfc_iso14443a_poller_tx_custom_parity(const uint8_t* tx_data, size_t tx_bits) {
    furi_check(tx_data);
    return furi_hal_nfc_poller_tx(tx_data, tx_bits);
}

FuriHalNfcError furi_hal_nfc_iso14443a_listener_set_col_res_data(
    uint8_t* uid,
    uint8_t uid_len,
    uint8_t* atqa,
    uint8_t sak) {
    furi_check(uid);
    furi_check(atqa);

    // SENS_RES (ATQA) - 2 bytes
    furi_hal_pn532_ctx.target_params[0] = atqa[0];
    furi_hal_pn532_ctx.target_params[1] = atqa[1];
    // NFCID1t (UID) - 3 bytes
    // In PN532 target mode, the first byte emitted to the reader is 0x08 (mandated by PN532 ROM).
    // The 3 bytes programmed into target_params[2..4] form bytes 1..3 of the 4-byte response.
    if(uid_len == 4) {
        furi_hal_pn532_ctx.target_params[2] = uid[1];
        furi_hal_pn532_ctx.target_params[3] = uid[2];
        furi_hal_pn532_ctx.target_params[4] = uid[3];
    } else {
        furi_hal_pn532_ctx.target_params[2] = uid[0];
        furi_hal_pn532_ctx.target_params[3] = uid[1];
        furi_hal_pn532_ctx.target_params[4] = uid[2];
    }
    // SEL_RES (SAK) - 1 byte
    furi_hal_pn532_ctx.target_params[5] = sak;
    // Pad remaining 32 bytes (FeliCaParams, NFCID3t, etc.) with 0
    memset(&furi_hal_pn532_ctx.target_params[6], 0, 32);
    furi_hal_pn532_ctx.target_params_len = 38;
    furi_hal_pn532_ctx.target_activated = false;
    furi_hal_pn532_ctx.rx_len = 0;

    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_iso4443a_listener_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    UNUSED(handle);
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    Pn532Error pn_err = pn532_tg_set_data(PN532_I2C_BUS, tx_data, (tx_bits + 7) / 8, 300);
    error = (pn_err == Pn532ErrorNone) ? FuriHalNfcErrorNone : FuriHalNfcErrorCommunication;

    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_iso14443a_listener_tx_custom_parity(
    const uint8_t* tx_data,
    const uint8_t* tx_parity,
    size_t tx_bits) {
    UNUSED(tx_parity);
    return furi_hal_nfc_iso4443a_listener_tx(NULL, tx_data, tx_bits);
}

FuriHalNfcError furi_hal_nfc_iso14443_3a_listener_sleep(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_iso14443_3a_listener_idle(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

const FuriHalNfcTechBase furi_hal_nfc_iso14443a = {
    .poller =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_POLLER_FDT_COMP_FC,
                    .fwt = FURI_HAL_NFC_POLLER_FWT_COMP_FC,
                },
            .init = furi_hal_nfc_iso14443a_poller_init,
            .deinit = furi_hal_nfc_iso14443a_poller_deinit,
            .wait_event = furi_hal_nfc_wait_event_common,
            .tx = furi_hal_nfc_poller_tx_common,
            .rx = furi_hal_nfc_common_fifo_rx,
        },

    .listener =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_ISO14443A_LISTENER_FDT_COMP_FC,
                },
            .init = furi_hal_nfc_iso14443a_listener_init,
            .deinit = furi_hal_nfc_iso14443a_listener_deinit,
            .wait_event = furi_hal_nfc_iso14443_3a_listener_wait_event,
            .tx = furi_hal_nfc_iso4443a_listener_tx,
            .rx = furi_hal_nfc_common_fifo_rx,
            .sleep = furi_hal_nfc_iso14443_3a_listener_sleep,
            .idle = furi_hal_nfc_iso14443_3a_listener_idle,
        },
};
