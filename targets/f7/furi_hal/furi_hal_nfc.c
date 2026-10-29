#include "furi_hal_nfc_i.h"
#include "furi_hal_nfc_tech_i.h"

#include <pn532.h>
#include <pn532_reg.h>

#include <furi.h>
#include <furi_hal_spi.h>
#include <furi_hal_random.h>

#define TAG "FuriHalNfc"

// PicoPass protocol commands (ISO14443-B proprietary, require InCommunicateThru)
#define PICOPASS_CMD_ACTALL           0x0A
#define PICOPASS_CMD_READ_OR_IDENTIFY 0x0C
#define PICOPASS_CMD_SELECT           0x81
#define PICOPASS_CMD_READCHECK_KD     0x48
#define PICOPASS_CMD_READCHECK_KC     0x18
#define PICOPASS_CMD_CHECK            0x88
#define PICOPASS_CMD_UPDATE           0x87
#define PICOPASS_CMD_ADDITIONAL_FRAME 0xAF

const FuriHalNfcTechBase* const furi_hal_nfc_tech[FuriHalNfcTechNum] = {
    [FuriHalNfcTechIso14443a] = &furi_hal_nfc_iso14443a,
    [FuriHalNfcTechIso14443b] = &furi_hal_nfc_iso14443b,
    [FuriHalNfcTechIso15693] = &furi_hal_nfc_iso15693,
    [FuriHalNfcTechFelica] = &furi_hal_nfc_felica,
    // Add new technologies here
};

FuriHalNfc furi_hal_nfc;

FuriHalPn532Ctx furi_hal_pn532_ctx = {0};

FuriHalNfcError furi_hal_nfc_is_hal_ready(void) {
    FURI_LOG_D(TAG, "Checking if HAL is ready");
    FuriHalNfcError error = FuriHalNfcErrorNone;

    do {
        error = furi_hal_nfc_acquire();
        if(error != FuriHalNfcErrorNone) break;

        uint8_t ic = 0, ver = 0, rev = 0, support = 0;
        Pn532Error pn_err = Pn532ErrorTimeout;

        for(int attempt = 0; attempt < 3; attempt++) {
            if(attempt > 0) {
                FURI_LOG_W(TAG, "PN532 retry %d", attempt);
                furi_delay_ms(10);
            }
            pn532_wake_up(PN532_I2C_BUS);
            pn_err = pn532_get_firmware_version(PN532_I2C_BUS, &ic, &ver, &rev, &support);
            if(pn_err == Pn532ErrorNone && ic == 0x32) {
                FURI_LOG_I(
                    TAG,
                    "PN532 Ready (IC: 0x%02X, Ver: %d, Rev: %d, Support: 0x%02X)",
                    ic,
                    ver,
                    rev,
                    support);
                break;
            }
        }

        if(pn_err != Pn532ErrorNone || ic != 0x32) {
            FURI_LOG_E(TAG, "PN532 not ready: err=%d, ic=0x%02X", pn_err, ic);
            error = FuriHalNfcErrorCommunication;
        }

        furi_hal_nfc_release();
    } while(false);

    FURI_LOG_D(TAG, "HAL ready check result: %d", error);
    return error;
}

FuriHalNfcError furi_hal_nfc_init(void) {
    furi_check(furi_hal_nfc.mutex == NULL);
    FURI_LOG_D(TAG, "Initializing Furi HAL NFC");

    furi_hal_nfc.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_hal_nfc.lock_count = 0;
    FuriHalNfcError error = FuriHalNfcErrorNone;

    furi_hal_nfc_event_init();
    furi_hal_nfc_event_start();

    do {
        error = furi_hal_nfc_acquire();
        if(error != FuriHalNfcErrorNone) {
            // Hardware was never touched — nothing to power down.
            FURI_LOG_E(TAG, "Failed to acquire NFC mutex during init");
            break;
        }

        furi_hal_nfc_init_gpio_isr();
        furi_hal_nfc_timers_init();

        Pn532Error pn_err = pn532_init(PN532_I2C_BUS);
        if(pn_err != Pn532ErrorNone) {
            FURI_LOG_E(TAG, "PN532 init failed: %d", pn_err);
            error = FuriHalNfcErrorCommunication;
        } else {
            FURI_LOG_I(TAG, "PN532 initialized successfully on I2C3");
        }

        furi_hal_nfc_low_power_mode_start();
        furi_hal_nfc_release();
    } while(false);

    return error;
}

bool furi_hal_nfc_is_mine(void) {
    return furi_mutex_get_owner(furi_hal_nfc.mutex) == furi_thread_get_current_id();
}

FuriHalNfcError furi_hal_nfc_acquire(void) {
    FuriHalNfcError error = FuriHalNfcErrorNone;
    furi_check(furi_hal_nfc.mutex);
    if(!furi_hal_nfc_is_mine()) {
        if(furi_mutex_acquire(furi_hal_nfc.mutex, 1000) != FuriStatusOk) {
            error = FuriHalNfcErrorBusy;
        } else {
            furi_hal_nfc.lock_count = 1;
        }
    } else {
        furi_hal_nfc.lock_count++;
    }
    return error;
}

FuriHalNfcError furi_hal_nfc_release(void) {
    furi_check(furi_hal_nfc.mutex);
    if(furi_hal_nfc_is_mine()) {
        furi_check(furi_hal_nfc.lock_count > 0);
        furi_hal_nfc.lock_count--;
        if(furi_hal_nfc.lock_count == 0) {
            furi_check(furi_mutex_release(furi_hal_nfc.mutex) == FuriStatusOk);
        }
    }
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_low_power_mode_start(void) {
    FuriHalNfcError error = FuriHalNfcErrorNone;

    bool own_lock = !furi_hal_nfc_is_mine();

    if(own_lock) {
        error = furi_hal_nfc_acquire();
        if(error != FuriHalNfcErrorNone) return error;
    }

    pn532_set_rf_field(PN532_I2C_BUS, false);
    furi_hal_nfc_deinit_gpio_isr();
    furi_hal_nfc_timers_deinit();
    furi_hal_nfc_event_stop();

    if(own_lock) {
        furi_hal_nfc_release();
    }

    return error;
}

FuriHalNfcError furi_hal_nfc_low_power_mode_stop(void) {
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    furi_hal_nfc_init_gpio_isr();
    furi_hal_nfc_timers_init();
    pn532_wake_up(PN532_I2C_BUS);

    furi_hal_nfc_event_start();

    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_set_mode(FuriHalNfcMode mode, FuriHalNfcTech tech) {
    furi_check(mode < FuriHalNfcModeNum);
    furi_check(tech < FuriHalNfcTechNum);

    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    if(mode == FuriHalNfcModePoller) {
        pn532_set_rf_field(PN532_I2C_BUS, true);
        if(furi_hal_nfc_tech[tech]->poller.init) {
            error = furi_hal_nfc_tech[tech]->poller.init(NULL);
        }
    } else if(mode == FuriHalNfcModeListener) {
        pn532_set_rf_field(PN532_I2C_BUS, false);
        if(furi_hal_nfc_tech[tech]->listener.init) {
            error = furi_hal_nfc_tech[tech]->listener.init(NULL);
        }
    }

    if(error != FuriHalNfcErrorNone) {
        FURI_LOG_E(TAG, "Failed to set mode %d, tech %d: error %d", mode, tech, error);
    }

    furi_hal_nfc.mode = mode;
    furi_hal_nfc.tech = tech;
    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_reset_mode(void) {
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    pn532_set_rf_field(PN532_I2C_BUS, false);
    furi_hal_pn532_ctx.target_detected = false;

    const FuriHalNfcMode mode = furi_hal_nfc.mode;
    const FuriHalNfcTech tech = furi_hal_nfc.tech;
    if(mode == FuriHalNfcModePoller) {
        if(furi_hal_nfc_tech[tech]->poller.deinit) {
            error = furi_hal_nfc_tech[tech]->poller.deinit(NULL);
        }
    } else if(mode == FuriHalNfcModeListener) {
        if(furi_hal_nfc_tech[tech]->listener.deinit) {
            error = furi_hal_nfc_tech[tech]->listener.deinit(NULL);
        }
    }

    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_field_detect_start(void) {
    FURI_LOG_D(TAG, "Starting field detection");
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    // PN532 automatically detects carrier in target/listener mode
    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_field_detect_stop(void) {
    FURI_LOG_D(TAG, "Stopping field detection");
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    furi_hal_nfc_release();
    return error;
}

bool furi_hal_nfc_field_is_present(void) {
    FURI_LOG_T(TAG, "Checking for external field presence");
    bool is_present = false;
    if(furi_hal_nfc_acquire() != FuriHalNfcErrorNone) return false;

    FURI_LOG_T(TAG, "Field is present: %s", is_present ? "true" : "false");
    furi_hal_nfc_release();
    return is_present;
}

FuriHalNfcError furi_hal_nfc_poller_field_on(void) {
    FURI_LOG_T(TAG, "Turning poller field on");
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    Pn532Error pn_err = pn532_set_rf_field(PN532_I2C_BUS, true);
    error = (pn_err == Pn532ErrorNone) ? FuriHalNfcErrorNone : FuriHalNfcErrorCommunication;

    furi_hal_nfc_release();
    return error;
}

static uint16_t furi_hal_nfc_crc_a_calc(const uint8_t* data, size_t len) {
    uint16_t crc = 0x6363;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        byte ^= (uint8_t)(crc & 0xFF);
        byte ^= (byte << 4);
        crc = (crc >> 8) ^ (((uint16_t)byte) << 8) ^ (((uint16_t)byte) << 3) ^ (byte >> 4);
    }
    return crc;
}

static uint16_t furi_hal_nfc_crc_b_calc(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        byte ^= (uint8_t)(crc & 0xFF);
        byte ^= (byte << 4);
        crc = (crc >> 8) ^ (((uint16_t)byte) << 8) ^ (((uint16_t)byte) << 3) ^ (byte >> 4);
    }
    return ~crc;
}

FuriHalNfcError furi_hal_nfc_poller_tx_common(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    furi_check(tx_data);
    // Callers (furi_hal_nfc_poller_tx, furi_hal_nfc_common_fifo_tx) already
    // hold the NFC mutex. Do NOT acquire again — that would cause a
    // lock_count imbalance on every early-return path and eventually
    // trigger furi_check(lock_count > 0) → crash.
    furi_check(furi_hal_nfc_is_mine());
    FURI_LOG_T(TAG, "Poller common TX, %zu bits", tx_bits);

    FuriHalNfcError err = FuriHalNfcErrorNone;

    UNUSED(handle);
    size_t tx_bytes = (tx_bits + 7) / 8;

    // PicoPass protocol: raw ISO14443-B commands via PN532 InCommunicateThru
    if(furi_hal_nfc.tech == FuriHalNfcTechIso14443b && tx_bytes >= 1) {
        uint8_t cmd = tx_data[0];
        bool is_picopass_cmd =
            (cmd == PICOPASS_CMD_ACTALL || cmd == PICOPASS_CMD_READ_OR_IDENTIFY ||
             cmd == PICOPASS_CMD_SELECT || cmd == PICOPASS_CMD_READCHECK_KD ||
             cmd == PICOPASS_CMD_READCHECK_KC || cmd == PICOPASS_CMD_CHECK ||
             cmd == PICOPASS_CMD_UPDATE || cmd == PICOPASS_CMD_ADDITIONAL_FRAME);
        if(is_picopass_cmd) {
            size_t send_len = tx_bytes;
            furi_hal_pn532_ctx.rx_len = sizeof(furi_hal_pn532_ctx.rx_buf);
            Pn532Error pn_err = pn532_in_communicate_thru(
                PN532_I2C_BUS,
                tx_data,
                send_len,
                furi_hal_pn532_ctx.rx_buf,
                &furi_hal_pn532_ctx.rx_len,
                50);
            if(pn_err == Pn532ErrorNone && furi_hal_pn532_ctx.rx_len > 0) {
                furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
                return FuriHalNfcErrorNone;
            } else if(pn_err == Pn532ErrorTimeout) {
                return FuriHalNfcErrorCommunicationTimeout;
            } else {
                return FuriHalNfcErrorCommunication;
            }
        }
    }

    // Check if this is ISO14443-3B technology
    if(furi_hal_nfc.tech == FuriHalNfcTechIso14443b) {
        if(tx_bytes >= 1 && tx_data[0] == 0x05) {
            // REQB command: activate Type B target using PN532
            Pn532Error pn_err = pn532_in_list_passive_target_iso14443b(
                PN532_I2C_BUS, &furi_hal_pn532_ctx.target_b, 300);
            if(pn_err == Pn532ErrorNone) {
                furi_hal_pn532_ctx.target_detected = true;
                // Synthesize 12-byte ATQB layout + 2-byte CRC-B
                furi_hal_pn532_ctx.rx_buf[0] = 0x50; // ATQB flag
                memcpy(&furi_hal_pn532_ctx.rx_buf[1], furi_hal_pn532_ctx.target_b.uid, 4);
                memcpy(&furi_hal_pn532_ctx.rx_buf[5], furi_hal_pn532_ctx.target_b.app_data, 4);
                memcpy(
                    &furi_hal_pn532_ctx.rx_buf[9], furi_hal_pn532_ctx.target_b.protocol_info, 3);
                uint16_t crc = furi_hal_nfc_crc_b_calc(furi_hal_pn532_ctx.rx_buf, 12);
                furi_hal_pn532_ctx.rx_buf[12] = (uint8_t)(crc & 0xFF);
                furi_hal_pn532_ctx.rx_buf[13] = (uint8_t)(crc >> 8);
                furi_hal_pn532_ctx.rx_len = 14;
                furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
                return FuriHalNfcErrorNone;
            } else {
                furi_hal_pn532_ctx.target_detected = false;
                return (pn_err == Pn532ErrorTimeout) ? FuriHalNfcErrorCommunicationTimeout :
                                                       FuriHalNfcErrorCommunication;
            }
        }
    }

    if(!furi_hal_pn532_ctx.target_detected) {
        return FuriHalNfcErrorCommunication;
    }

    // Check if this is an ISO14443A SELECT / SDD command sequence
    if(tx_bytes >= 2) {
        if(tx_data[0] == 0x93 && tx_data[1] == 0x70) {
            // Select Cascade 1
            uint8_t sak = (furi_hal_pn532_ctx.target.uid_len == 4) ?
                              furi_hal_pn532_ctx.target.sel_res :
                              0x04;
            uint16_t crc = furi_hal_nfc_crc_a_calc(&sak, 1);
            furi_hal_pn532_ctx.rx_buf[0] = sak;
            furi_hal_pn532_ctx.rx_buf[1] = (uint8_t)(crc & 0xFF);
            furi_hal_pn532_ctx.rx_buf[2] = (uint8_t)(crc >> 8);
            furi_hal_pn532_ctx.rx_len = 3;
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
            return FuriHalNfcErrorNone;
        } else if(tx_data[0] == 0x95 && tx_data[1] == 0x70) {
            // Select Cascade 2
            uint8_t sak = (furi_hal_pn532_ctx.target.uid_len == 10) ?
                              0x04 :
                              furi_hal_pn532_ctx.target.sel_res;
            uint16_t crc = furi_hal_nfc_crc_a_calc(&sak, 1);
            furi_hal_pn532_ctx.rx_buf[0] = sak;
            furi_hal_pn532_ctx.rx_buf[1] = (uint8_t)(crc & 0xFF);
            furi_hal_pn532_ctx.rx_buf[2] = (uint8_t)(crc >> 8);
            furi_hal_pn532_ctx.rx_len = 3;
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
            return FuriHalNfcErrorNone;
        } else if(tx_data[0] == 0x97 && tx_data[1] == 0x70) {
            // Select Cascade 3 (10-byte UID)
            uint8_t sak = furi_hal_pn532_ctx.target.sel_res;
            uint16_t crc = furi_hal_nfc_crc_a_calc(&sak, 1);
            furi_hal_pn532_ctx.rx_buf[0] = sak;
            furi_hal_pn532_ctx.rx_buf[1] = (uint8_t)(crc & 0xFF);
            furi_hal_pn532_ctx.rx_buf[2] = (uint8_t)(crc >> 8);
            furi_hal_pn532_ctx.rx_len = 3;
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
            return FuriHalNfcErrorNone;
        } else if(tx_data[0] == 0xE0) {
            // RATS command: PN532 already activated Layer 4 during InListPassiveTarget
            size_t ats_len = furi_hal_pn532_ctx.target.ats_len;
            if(ats_len > 0) {
                if(ats_len > 32) ats_len = 32;
                if(furi_hal_pn532_ctx.target.ats[0] == ats_len) {
                    memcpy(furi_hal_pn532_ctx.rx_buf, furi_hal_pn532_ctx.target.ats, ats_len);
                } else {
                    furi_hal_pn532_ctx.rx_buf[0] = (uint8_t)ats_len;
                    memcpy(
                        &furi_hal_pn532_ctx.rx_buf[1], furi_hal_pn532_ctx.target.ats, ats_len - 1);
                }
            } else {
                // Default minimal ATS (1 byte length TL = 1) for Layer 4 tags
                ats_len = 1;
                furi_hal_pn532_ctx.rx_buf[0] = 0x01;
            }
            uint16_t crc = furi_hal_nfc_crc_a_calc(furi_hal_pn532_ctx.rx_buf, ats_len);
            furi_hal_pn532_ctx.rx_buf[ats_len] = (uint8_t)(crc & 0xFF);
            furi_hal_pn532_ctx.rx_buf[ats_len + 1] = (uint8_t)(crc >> 8);
            furi_hal_pn532_ctx.rx_len = ats_len + 2;
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
            return FuriHalNfcErrorNone;
        }
    }

    // Standard card data exchange via InDataExchange
    // Strip the 2-byte CRC if present (Flipper stack appends it, but PN532 adds its own RF CRC)
    size_t send_len = (tx_bytes > 2) ? (tx_bytes - 2) : tx_bytes;

    if(send_len == 2 &&
       (tx_data[0] == 0x60 || tx_data[0] == 0x61 || tx_data[0] == 0x68 || tx_data[0] == 0x69)) {
        uint8_t sak = furi_hal_pn532_ctx.target.sel_res;
        bool is_mfc_sak =
            (sak == 0x08 || sak == 0x18 || sak == 0x09 || sak == 0x28 || sak == 0x38 ||
             sak == 0x88 || sak == 0x10 || sak == 0x11 || sak == 0x01);
        if(is_mfc_sak) {
            // Synthesize 4-byte random nonce (nt) without CRC for MIFARE Classic detection / Step 1
            uint32_t rand_val = furi_hal_random_get();
            furi_hal_pn532_ctx.rx_buf[0] = (uint8_t)(rand_val & 0xFF);
            furi_hal_pn532_ctx.rx_buf[1] = (uint8_t)((rand_val >> 8) & 0xFF);
            furi_hal_pn532_ctx.rx_buf[2] = (uint8_t)((rand_val >> 16) & 0xFF);
            furi_hal_pn532_ctx.rx_buf[3] = (uint8_t)((rand_val >> 24) & 0xFF);
            furi_hal_pn532_ctx.rx_len = 4;
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
            return FuriHalNfcErrorNone;
        }
    }

    // Check if card is activated in ISO 14443-4 (ISO-DEP / T=CL) mode
    bool is_iso_dep = ((furi_hal_pn532_ctx.target.sel_res & 0x20) != 0) ||
                      (furi_hal_pn532_ctx.target.ats_len > 0) ||
                      (furi_hal_nfc.tech == FuriHalNfcTechIso14443b);

    const uint8_t* actual_tx = tx_data;
    size_t actual_len = send_len;
    bool is_iso_dep_i_block = false;
    uint8_t iso_dep_pcb = 0;

    if(is_iso_dep && send_len > 1 && ((tx_data[0] & 0xC2) == 0x02)) {
        // ISO 14443-4 Layer I-Block from Flipper software:
        // tx_data[0] is PCB (0x02 / 0x03). PN532 hardware generates its own PCB over RF in ISO-DEP mode.
        // Strip the software PCB so PN532 sends the pure ISO 7816 APDU (e.g. 00 A4 04 00 ...) to the card.
        is_iso_dep_i_block = true;
        iso_dep_pcb = tx_data[0];
        actual_tx = &tx_data[1];
        actual_len = send_len - 1;
    }

    furi_hal_pn532_ctx.rx_len = sizeof(furi_hal_pn532_ctx.rx_buf) - 4;
    FURI_LOG_D(
        TAG,
        "InDataExchange: tg=1, tx_data[0]=0x%02X, actual_len=%zu, is_iso_dep=%d, target_detected=%d",
        actual_tx[0],
        actual_len,
        is_iso_dep_i_block,
        furi_hal_pn532_ctx.target_detected);

    uint32_t trx_timeout = (actual_tx[0] == 0x50 && actual_len == 2) ? 15 : 500;
    Pn532Error pn_err = pn532_in_data_exchange(
        PN532_I2C_BUS,
        1,
        actual_tx,
        actual_len,
        furi_hal_pn532_ctx.rx_buf,
        &furi_hal_pn532_ctx.rx_len,
        trx_timeout);

    if(pn_err == Pn532ErrorNone) {
        size_t payload_len = furi_hal_pn532_ctx.rx_len;

        if(is_iso_dep_i_block) {
            // Re-wrap response with expected ISO 14443-4 PCB byte for Flipper's Iso14443_4Layer decoder
            furi_check(payload_len + 3 <= sizeof(furi_hal_pn532_ctx.rx_buf));
            memmove(&furi_hal_pn532_ctx.rx_buf[1], furi_hal_pn532_ctx.rx_buf, payload_len);
            furi_hal_pn532_ctx.rx_buf[0] = iso_dep_pcb;
            payload_len += 1;
        }

        bool is_auth_cmd =
            (tx_data[0] == 0x60 || tx_data[0] == 0x61 || tx_data[0] == 0x68 || tx_data[0] == 0x69);
        bool is_mfc_crypto =
            (send_len == 8) || (payload_len == 1) ||
            (furi_hal_nfc.tech == FuriHalNfcTechIso14443a && send_len == 4 && payload_len == 4);

        if(!is_auth_cmd && !is_mfc_crypto) {
            // Guard against buffer overflow before appending 2 CRC bytes
            furi_check(payload_len + 2 <= sizeof(furi_hal_pn532_ctx.rx_buf));
            uint16_t crc;
            if(furi_hal_nfc.tech == FuriHalNfcTechIso14443b) {
                crc = furi_hal_nfc_crc_b_calc(furi_hal_pn532_ctx.rx_buf, payload_len);
            } else {
                crc = furi_hal_nfc_crc_a_calc(furi_hal_pn532_ctx.rx_buf, payload_len);
            }
            furi_hal_pn532_ctx.rx_buf[payload_len] = (uint8_t)(crc & 0xFF);
            furi_hal_pn532_ctx.rx_buf[payload_len + 1] = (uint8_t)(crc >> 8);
            furi_hal_pn532_ctx.rx_len = payload_len + 2;
        }

        err = FuriHalNfcErrorNone;
        furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
    } else {
        if(pn_err == Pn532ErrorTimeout) {
            err = FuriHalNfcErrorCommunicationTimeout;
        } else {
            err = FuriHalNfcErrorCommunication;
        }
    }
    return err;
}

FuriHalNfcError furi_hal_nfc_common_fifo_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    FURI_LOG_T(TAG, "Common FIFO TX, %zu bits", tx_bits);
    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) return err;
    err = furi_hal_nfc_poller_tx_common(handle, tx_data, tx_bits);
    furi_hal_nfc_release();
    return err;
}

FuriHalNfcError furi_hal_nfc_poller_tx(const uint8_t* tx_data, size_t tx_bits) {
    furi_check(furi_hal_nfc.mode == FuriHalNfcModePoller);

    furi_check(furi_hal_nfc.tech < FuriHalNfcTechNum);
    FURI_LOG_T(TAG, "Poller TX for tech %d", furi_hal_nfc.tech);

    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) return err;

    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
    err = furi_hal_nfc_tech[furi_hal_nfc.tech]->poller.tx(handle, tx_data, tx_bits);

    furi_hal_nfc_release();

    return err;
}

FuriHalNfcError furi_hal_nfc_poller_rx(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits) {
    furi_check(furi_hal_nfc.mode == FuriHalNfcModePoller);
    furi_check(furi_hal_nfc.tech < FuriHalNfcTechNum);

    FURI_LOG_T(TAG, "Poller RX for tech %d", furi_hal_nfc.tech);

    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) return err;

    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    err = furi_hal_nfc_tech[furi_hal_nfc.tech]->poller.rx(handle, rx_data, rx_data_size, rx_bits);

    furi_hal_nfc_release();

    return err;
}

FuriHalNfcEvent furi_hal_nfc_poller_wait_event(uint32_t timeout_ms) {
    furi_check(furi_hal_nfc.mode == FuriHalNfcModePoller);
    furi_check(furi_hal_nfc.tech < FuriHalNfcTechNum);
    FURI_LOG_T(TAG, "Poller wait event for tech %d", furi_hal_nfc.tech);
    return furi_hal_nfc_tech[furi_hal_nfc.tech]->poller.wait_event(timeout_ms);
}

FuriHalNfcEvent furi_hal_nfc_listener_wait_event(uint32_t timeout_ms) {
    furi_check(furi_hal_nfc.mode == FuriHalNfcModeListener);
    furi_check(furi_hal_nfc.tech < FuriHalNfcTechNum);
    FURI_LOG_T(TAG, "Listener wait event for tech %d", furi_hal_nfc.tech);

    return furi_hal_nfc_tech[furi_hal_nfc.tech]->listener.wait_event(timeout_ms);
}

FuriHalNfcError furi_hal_nfc_listener_tx(const uint8_t* tx_data, size_t tx_bits) {
    furi_check(tx_data);

    furi_check(furi_hal_nfc.mode == FuriHalNfcModeListener);

    furi_check(furi_hal_nfc.tech < FuriHalNfcTechNum);

    FURI_LOG_T(TAG, "Listener TX for tech %d", furi_hal_nfc.tech);

    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) return err;

    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    err = furi_hal_nfc_tech[furi_hal_nfc.tech]->listener.tx(handle, tx_data, tx_bits);

    furi_hal_nfc_release();

    return err;
}

FuriHalNfcError furi_hal_nfc_common_fifo_rx(
    const FuriHalSpiBusHandle* handle,
    uint8_t* rx_data,
    size_t rx_data_size,
    size_t* rx_bits) {
    FURI_LOG_T(TAG, "Common FIFO RX");
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    UNUSED(handle);
    if(furi_hal_pn532_ctx.rx_len > rx_data_size) {
        FURI_LOG_W(TAG, "FIFO RX buffer overflow");
        error = FuriHalNfcErrorBufferOverflow;
    } else {
        memcpy(rx_data, furi_hal_pn532_ctx.rx_buf, furi_hal_pn532_ctx.rx_len);
        *rx_bits = furi_hal_pn532_ctx.rx_len * 8;
        error = FuriHalNfcErrorNone;
    }
    FURI_LOG_T(TAG, "Read %zu bits from FIFO", *rx_bits);
    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_listener_rx(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits) {
    furi_check(rx_data);
    furi_check(rx_bits);

    furi_check(furi_hal_nfc.mode == FuriHalNfcModeListener);
    furi_check(furi_hal_nfc.tech < FuriHalNfcTechNum);
    FURI_LOG_T(TAG, "Listener RX for tech %d", furi_hal_nfc.tech);

    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) return err;

    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    err =
        furi_hal_nfc_tech[furi_hal_nfc.tech]->listener.rx(handle, rx_data, rx_data_size, rx_bits);

    furi_hal_nfc_release();

    return err;
}

FuriHalNfcError furi_hal_nfc_trx_reset(void) {
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    furi_hal_pn532_ctx.rx_len = 0;
    furi_hal_nfc_release();
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_listener_sleep(void) {
    furi_check(furi_hal_nfc.mode == FuriHalNfcModeListener);
    furi_check(furi_hal_nfc.tech < FuriHalNfcTechNum);
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
    error = furi_hal_nfc_tech[furi_hal_nfc.tech]->listener.sleep(handle);
    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_listener_idle(void) {
    furi_check(furi_hal_nfc.mode == FuriHalNfcModeListener);
    furi_check(furi_hal_nfc.tech < FuriHalNfcTechNum);
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
    error = furi_hal_nfc_tech[furi_hal_nfc.tech]->listener.idle(handle);
    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_listener_enable_rx(void) {
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;
    furi_hal_nfc_release();
    return FuriHalNfcErrorNone;
}
