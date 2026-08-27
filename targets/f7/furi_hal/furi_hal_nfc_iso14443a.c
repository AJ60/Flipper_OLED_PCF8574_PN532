#include "furi_hal_nfc_i.h"
#include "furi_hal_nfc_tech_i.h"

#include <furi.h>
#include <furi_hal_resources.h>

#include <digital_signal/presets/nfc/iso14443_3a_signal.h>

#define TAG "FuriHalIso14443a"

// Prevent FDT timer from starting
#define FURI_HAL_NFC_ISO14443A_LISTENER_FDT_COMP_FC (INT32_MAX)

static Iso14443_3aSignal* iso14443_3a_signal = NULL;

#if !defined(FURI_HAL_NFC_CHIP_PN532)
static FuriHalNfcError furi_hal_nfc_iso14443a_common_init(const FuriHalSpiBusHandle* handle) {
    // Common NFC-A settings, 106 kbps

    // 1st stage zero = 600kHz, 3rd stage zero = 200 kHz
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF1, ST25R3916_REG_RX_CONF1_z600k);
    // AGC enabled, ratio 6:1, squelch after TX
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF2,
        ST25R3916_REG_RX_CONF2_agc6_3 | ST25R3916_REG_RX_CONF2_agc_m |
            ST25R3916_REG_RX_CONF2_agc_en | ST25R3916_REG_RX_CONF2_sqm_dyn);
    // HF operation, full gain on AM and PM channels
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF3, 0x00);
    // No gain reduction on AM and PM channels
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF4, 0x00);
    // Correlator config
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_CORR_CONF1,
        ST25R3916_REG_CORR_CONF1_corr_s0 | ST25R3916_REG_CORR_CONF1_corr_s4 |
            ST25R3916_REG_CORR_CONF1_corr_s6);
    // Sleep mode disable, 424kHz mode off
    st25r3916_write_reg(handle, ST25R3916_REG_CORR_CONF2, 0x00);

    return FuriHalNfcErrorNone;
}
#endif

static FuriHalNfcError furi_hal_nfc_iso14443a_poller_init(const FuriHalSpiBusHandle* handle) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    return FuriHalNfcErrorNone;
#else
    // Enable ISO14443A mode, OOK modulation
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_MODE,
        ST25R3916_REG_MODE_om_mask | ST25R3916_REG_MODE_tr_am,
        ST25R3916_REG_MODE_om_iso14443a | ST25R3916_REG_MODE_tr_am_ook);

    // Overshoot protection - disabled (0x00) for custom/DIY antennas
    st25r3916_change_reg_bits(handle, ST25R3916_REG_OVERSHOOT_CONF1, 0xff, 0x00);
    st25r3916_change_reg_bits(handle, ST25R3916_REG_OVERSHOOT_CONF2, 0xff, 0x00);
    st25r3916_change_reg_bits(handle, ST25R3916_REG_UNDERSHOOT_CONF1, 0xff, 0x00);
    st25r3916_change_reg_bits(handle, ST25R3916_REG_UNDERSHOOT_CONF2, 0xff, 0x00);

    return furi_hal_nfc_iso14443a_common_init(handle);
#endif
}

static FuriHalNfcError furi_hal_nfc_iso14443a_poller_deinit(const FuriHalSpiBusHandle* handle) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    return FuriHalNfcErrorNone;
#else
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));

    return FuriHalNfcErrorNone;
#endif
}

static FuriHalNfcError furi_hal_nfc_iso14443a_listener_init(const FuriHalSpiBusHandle* handle) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    return FuriHalNfcErrorNone;
#else
    furi_check(iso14443_3a_signal == NULL);
    iso14443_3a_signal = iso14443_3a_signal_alloc(&gpio_spi_r_mosi);

    st25r3916_write_reg(
        handle,
        ST25R3916_REG_OP_CONTROL,
        ST25R3916_REG_OP_CONTROL_en | ST25R3916_REG_OP_CONTROL_rx_en |
            ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);
    st25r3916_write_reg(
        handle, ST25R3916_REG_MODE, ST25R3916_REG_MODE_targ_targ | ST25R3916_REG_MODE_om0);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_PASSIVE_TARGET,
        ST25R3916_REG_PASSIVE_TARGET_fdel_2 | ST25R3916_REG_PASSIVE_TARGET_fdel_0 |
            ST25R3916_REG_PASSIVE_TARGET_d_ac_ap2p | ST25R3916_REG_PASSIVE_TARGET_d_212_424_1r);

    st25r3916_write_reg(handle, ST25R3916_REG_MASK_RX_TIMER, 0x02);

    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS |
         ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
         ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_NRE |
         ST25R3916_IRQ_MASK_EON | ST25R3916_IRQ_MASK_EOF | ST25R3916_IRQ_MASK_WU_A_X |
         ST25R3916_IRQ_MASK_WU_A);
    // Clear interrupts
    st25r3916_get_irq(handle);
    // Enable interrupts
    st25r3916_mask_irq(handle, ~interrupts);
    // Enable auto collision resolution
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_GOTO_SENSE);

    return furi_hal_nfc_iso14443a_common_init(handle);
#endif
}

static FuriHalNfcError furi_hal_nfc_iso14443a_listener_deinit(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);

    if(iso14443_3a_signal) {
        iso14443_3a_signal_free(iso14443_3a_signal);
        iso14443_3a_signal = NULL;
    }

    return FuriHalNfcErrorNone;
}

static FuriHalNfcEvent furi_hal_nfc_iso14443_3a_listener_wait_event(uint32_t timeout_ms) {
    FuriHalNfcEvent event = furi_hal_nfc_wait_event_common(timeout_ms);
#if !defined(FURI_HAL_NFC_CHIP_PN532)
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    if(event & FuriHalNfcEventListenerActive) {
        if(furi_hal_nfc_acquire() == FuriHalNfcErrorNone) {
            st25r3916_set_reg_bits(
                handle, ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
            furi_hal_nfc_release();
        }
    }
#endif
    return event;
}

FuriHalNfcError furi_hal_nfc_iso14443a_poller_trx_short_frame(FuriHalNfcaShortFrame frame) {
    FuriHalNfcError error = FuriHalNfcErrorNone;

#if defined(FURI_HAL_NFC_CHIP_PN532)
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
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    // Disable crc check
    st25r3916_set_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));

    st25r3916_write_reg(handle, ST25R3916_REG_NUM_TX_BYTES2, 0);
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS |
         ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
         ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_NRE);
    // Clear interrupts
    st25r3916_get_irq(handle);
    // Enable interrupts
    st25r3916_mask_irq(handle, ~interrupts);
    if(frame == FuriHalNfcaShortFrameAllReq) {
        st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_REQA);
    } else {
        st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WUPA);
    }

    furi_hal_nfc_release();

    return error;
#endif
}

FuriHalNfcError furi_hal_nfc_iso14443a_tx_sdd_frame(const uint8_t* tx_data, size_t tx_bits) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
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
#else
    FuriHalNfcError error = FuriHalNfcErrorNone;
    error = furi_hal_nfc_poller_tx(tx_data, tx_bits);
    return error;
#endif
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

#if defined(FURI_HAL_NFC_CHIP_PN532)
    return furi_hal_nfc_poller_tx(tx_data, tx_bits);
#else
    FuriHalNfcError err = FuriHalNfcErrorNone;
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) return err;

    // Prepare tx
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_TIMER_EMV_CONTROL, ST25R3916_REG_TIMER_EMV_CONTROL_nrt_emv);
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par));
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS |
         ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
         ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_NRE);
    // Clear interrupts
    st25r3916_get_irq(handle);
    // Enable interrupts
    st25r3916_mask_irq(handle, ~interrupts);

    st25r3916_write_fifo(handle, tx_data, tx_bits);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);

    furi_hal_nfc_release();

    return err;
#endif
}

FuriHalNfcError furi_hal_nfc_iso14443a_listener_set_col_res_data(
    uint8_t* uid,
    uint8_t uid_len,
    uint8_t* atqa,
    uint8_t sak) {
    furi_check(uid);
    furi_check(atqa);

    FuriHalNfcError error = FuriHalNfcErrorNone;

    error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

#if defined(FURI_HAL_NFC_CHIP_PN532)
    uint8_t params[36] = {0};
    params[0] = atqa[0];
    params[1] = atqa[1];
    params[2] = uid[0];
    params[3] = uid[1];
    params[4] = uid[2];
    params[5] = (uid_len > 3) ? uid[3] : 0x00;
    params[6] = sak;

    uint8_t rx_cmd[64];
    size_t rx_cmd_len = sizeof(rx_cmd);
    Pn532Error pn_err =
        pn532_tg_init_as_target(PN532_I2C_BUS, params, 7, rx_cmd, &rx_cmd_len, 500);
    if(pn_err == Pn532ErrorNone) {
        error = FuriHalNfcErrorNone;
        if(rx_cmd_len > 0) {
            furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
        }
    } else {
        error = FuriHalNfcErrorCommunication;
    }

    furi_hal_nfc_release();
    return error;
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    // Set 4 or 7 bytes UID
    if(uid_len == 4) {
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_AUX,
            ST25R3916_REG_AUX_nfc_id_mask,
            ST25R3916_REG_AUX_nfc_id_4bytes);
    } else {
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_AUX,
            ST25R3916_REG_AUX_nfc_id_mask,
            ST25R3916_REG_AUX_nfc_id_7bytes);
    }
    // Write PT Memory
    uint8_t pt_memory[15] = {};
    memcpy(pt_memory, uid, uid_len);
    pt_memory[10] = atqa[0];
    pt_memory[11] = atqa[1];
    if(uid_len == 4) {
        pt_memory[12] = sak & ~0x04;
    } else {
        pt_memory[12] = 0x04;
    }
    pt_memory[13] = sak & ~0x04;
    pt_memory[14] = sak & ~0x04;

    st25r3916_write_pta_mem(handle, pt_memory, sizeof(pt_memory));

    furi_hal_nfc_release();

    return error;
#endif
}

FuriHalNfcError furi_hal_nfc_iso4443a_listener_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    Pn532Error pn_err = pn532_tg_set_data(PN532_I2C_BUS, tx_data, (tx_bits + 7) / 8, 300);
    error = (pn_err == Pn532ErrorNone) ? FuriHalNfcErrorNone : FuriHalNfcErrorCommunication;

    furi_hal_nfc_release();
    return error;
#else
    FuriHalNfcError error = FuriHalNfcErrorNone;

    do {
        error = furi_hal_nfc_common_fifo_tx(handle, tx_data, tx_bits);
        if(error != FuriHalNfcErrorNone) break;

        bool tx_end = furi_hal_nfc_event_wait_for_specific_irq(handle, ST25R3916_IRQ_MASK_TXE, 10);
        if(!tx_end) {
            error = FuriHalNfcErrorCommunicationTimeout;
            break;
        }

    } while(false);

    return error;
#endif
}

FuriHalNfcError furi_hal_nfc_iso14443a_listener_tx_custom_parity(
    const uint8_t* tx_data,
    const uint8_t* tx_parity,
    size_t tx_bits) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(tx_parity);
    return furi_hal_nfc_iso4443a_listener_tx(NULL, tx_data, tx_bits);
#else
    furi_check(tx_data);
    furi_check(tx_parity);

    furi_check(iso14443_3a_signal);

    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSPARENT_MODE);
    // Reconfigure gpio for Transparent mode
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_nfc);

    // Send signal
    iso14443_3a_signal_tx(iso14443_3a_signal, tx_data, tx_parity, tx_bits);

    // Exit transparent mode
    furi_hal_gpio_write(&gpio_spi_r_mosi, false);

    // Configure gpio back to SPI and exit transparent
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_nfc);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_UNMASK_RECEIVE_DATA);

    furi_hal_nfc_release();

    return FuriHalNfcErrorNone;
#endif
}

FuriHalNfcError furi_hal_nfc_iso14443_3a_listener_sleep(const FuriHalSpiBusHandle* handle) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    return FuriHalNfcErrorNone;
#else
    // Enable auto collision resolution
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_GOTO_SLEEP);

    return FuriHalNfcErrorNone;
#endif
}

FuriHalNfcError furi_hal_nfc_iso14443_3a_listener_idle(const FuriHalSpiBusHandle* handle) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    return FuriHalNfcErrorNone;
#else
    // Enable auto collision resolution
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_GOTO_SENSE);

    return FuriHalNfcErrorNone;
#endif
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
