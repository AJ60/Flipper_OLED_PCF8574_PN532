#include "furi_hal_nfc_i.h"
#include "furi_hal_nfc_tech_i.h"

#if defined(FURI_HAL_NFC_CHIP_PN532)
#include <pn532.h>
#include <pn532_reg.h>
#else
#include <lib/drivers/st25r3916.h>
#endif

#include <furi.h>
#include <furi_hal_spi.h>
#include <furi_hal_random.h>

#define TAG "FuriHalNfc"

const FuriHalNfcTechBase* const furi_hal_nfc_tech[FuriHalNfcTechNum] = {
    [FuriHalNfcTechIso14443a] = &furi_hal_nfc_iso14443a,
    [FuriHalNfcTechIso14443b] = &furi_hal_nfc_iso14443b,
    [FuriHalNfcTechIso15693] = &furi_hal_nfc_iso15693,
    [FuriHalNfcTechFelica] = &furi_hal_nfc_felica,
    // Add new technologies here
};

FuriHalNfc furi_hal_nfc;

#if defined(FURI_HAL_NFC_CHIP_PN532)
FuriHalPn532Ctx furi_hal_pn532_ctx = {0};
#endif

// void //furi_hal_nfc_log_irq(const char* action, uint32_t irq_mask) {
//     if(irq_mask == ST25R3916_IRQ_MASK_NONE) {
//         FURI_LOG_D(TAG, "%s: ST25R3916_IRQ_MASK_NONE (0x00000000)", action);
//         return;
//     }
//     if(irq_mask == ST25R3916_IRQ_MASK_ALL) {
//         FURI_LOG_D(TAG, "%s: ST25R3916_IRQ_MASK_ALL (0xFFFFFFFF)", action);
//         return;
//     }

//     FURI_LOG_D(TAG, "%s with mask 0x%08lX:", action, irq_mask);
//     if(irq_mask & ST25R3916_IRQ_MASK_OSC) FURI_LOG_D(TAG, "  - OSC: Oscillator stable");
//     if(irq_mask & ST25R3916_IRQ_MASK_FWL) FURI_LOG_D(TAG, "  - FWL: FIFO water level");
//     if(irq_mask & ST25R3916_IRQ_MASK_RXS) FURI_LOG_D(TAG, "  - RXS: Start of receive");
//     if(irq_mask & ST25R3916_IRQ_MASK_RXE) FURI_LOG_D(TAG, "  - RXE: End of receive");
//     if(irq_mask & ST25R3916_IRQ_MASK_TXE) FURI_LOG_D(TAG, "  - TXE: End of transmission");
//     if(irq_mask & ST25R3916_IRQ_MASK_COL) FURI_LOG_D(TAG, "  - COL: Bit collision");
//     if(irq_mask & ST25R3916_IRQ_MASK_RX_REST)
//         FURI_LOG_D(TAG, "  - RX_REST: Automatic reception restart");
//     if(irq_mask & ST25R3916_IRQ_MASK_DCT)
//         FURI_LOG_D(TAG, "  - DCT: Termination of direct command");
//     if(irq_mask & ST25R3916_IRQ_MASK_NRE) FURI_LOG_D(TAG, "  - NRE: No-response timer expired");
//     if(irq_mask & ST25R3916_IRQ_MASK_GPE)
//         FURI_LOG_D(TAG, "  - GPE: General purpose timer expired");
//     if(irq_mask & ST25R3916_IRQ_MASK_EON) FURI_LOG_D(TAG, "  - EON: External field on");
//     if(irq_mask & ST25R3916_IRQ_MASK_EOF) FURI_LOG_D(TAG, "  - EOF: External field off");
//     if(irq_mask & ST25R3916_IRQ_MASK_CAC)
//         FURI_LOG_D(TAG, "  - CAC: Collision during RF collision avoidance");
//     if(irq_mask & ST25R3916_IRQ_MASK_CAT) FURI_LOG_D(TAG, "  - CAT: Minimum guard time expired");
//     if(irq_mask & ST25R3916_IRQ_MASK_NFCT)
//         FURI_LOG_D(TAG, "  - NFCT: Initiator bit rate recognised");
//     if(irq_mask & ST25R3916_IRQ_MASK_CRC) FURI_LOG_D(TAG, "  - CRC: CRC error");
//     if(irq_mask & ST25R3916_IRQ_MASK_PAR) FURI_LOG_D(TAG, "  - PAR: Parity error");
//     if(irq_mask & ST25R3916_IRQ_MASK_ERR2) FURI_LOG_D(TAG, "  - ERR2: Soft framing error");
//     if(irq_mask & ST25R3916_IRQ_MASK_ERR1) FURI_LOG_D(TAG, "  - ERR1: Hard framing error");
//     if(irq_mask & ST25R3916_IRQ_MASK_WT) FURI_LOG_D(TAG, "  - WT: Wake-up interrupt");
//     if(irq_mask & ST25R3916_IRQ_MASK_WAM) FURI_LOG_D(TAG, "  - WAM: Wake-up due to amplitude");
//     if(irq_mask & ST25R3916_IRQ_MASK_WPH) FURI_LOG_D(TAG, "  - WPH: Wake-up due to phase");
//     if(irq_mask & ST25R3916_IRQ_MASK_WCAP)
//         FURI_LOG_D(TAG, "  - WCAP: Wake-up due to capacitance measurement");
//     if(irq_mask & ST25R3916_IRQ_MASK_PPON2) FURI_LOG_D(TAG, "  - PPON2: Field on waiting Timer");
//     if(irq_mask & ST25R3916_IRQ_MASK_SL_WL)
//         FURI_LOG_D(TAG, "  - SL_WL: Passive target slot number water level");
//     if(irq_mask & ST25R3916_IRQ_MASK_APON)
//         FURI_LOG_D(TAG, "  - APON: Anticollision done and Field On");
//     if(irq_mask & ST25R3916_IRQ_MASK_RXE_PTA)
//         FURI_LOG_D(TAG, "  - RXE_PTA: RXE with an automatic response");
//     if(irq_mask & ST25R3916_IRQ_MASK_WU_F)
//         FURI_LOG_D(TAG, "  - WU_F: 212/424b/s Passive target Active");
//     if(irq_mask & ST25R3916_IRQ_MASK_WU_A_X)
//         FURI_LOG_D(TAG, "  - WU_A_X: 106kb/s Passive target state Active*");
//     if(irq_mask & ST25R3916_IRQ_MASK_WU_A)
//         FURI_LOG_D(TAG, "  - WU_A: 106kb/s Passive target state Active");
// }

#if !defined(FURI_HAL_NFC_CHIP_PN532)
static FuriHalNfcError furi_hal_nfc_turn_on_osc(const FuriHalSpiBusHandle* handle) {
    FuriHalNfcError error = FuriHalNfcErrorNone;
    furi_hal_nfc_event_start();
    if(!st25r3916_check_reg(
           handle,
           ST25R3916_REG_OP_CONTROL,
           ST25R3916_REG_OP_CONTROL_en,
           ST25R3916_REG_OP_CONTROL_en)) {
        uint32_t irq_to_enable = ST25R3916_IRQ_MASK_OSC;
        st25r3916_mask_irq(handle, ~irq_to_enable);
        st25r3916_set_reg_bits(handle, ST25R3916_REG_OP_CONTROL, ST25R3916_REG_OP_CONTROL_en);
        if(!furi_hal_nfc_event_wait_for_specific_irq(handle, ST25R3916_IRQ_MASK_OSC, 10)) {
            FURI_LOG_W(TAG, "Oscillator IRQ timeout");
        }
    }
    st25r3916_mask_irq(handle, ST25R3916_IRQ_MASK_ALL);

    bool osc_on = st25r3916_check_reg(
        handle,
        ST25R3916_REG_AUX_DISPLAY,
        ST25R3916_REG_AUX_DISPLAY_osc_ok,
        ST25R3916_REG_AUX_DISPLAY_osc_ok);
    if(!osc_on) {
        FURI_LOG_E(TAG, "Oscillator failed to turn on");
        error = FuriHalNfcErrorOscillator;
    }

    return error;
}
#endif

FuriHalNfcError furi_hal_nfc_is_hal_ready(void) {
    FURI_LOG_D(TAG, "Checking if HAL is ready");
    FuriHalNfcError error = FuriHalNfcErrorNone;

#if defined(FURI_HAL_NFC_CHIP_PN532)
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
#else
    do {
        error = furi_hal_nfc_acquire();
        if(error != FuriHalNfcErrorNone) break;

        const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
        uint8_t chip_id = 0;
        st25r3916_read_reg(handle, ST25R3916_REG_IC_IDENTITY, &chip_id);
        FURI_LOG_D(TAG, "Read Chip ID: 0x%02X", chip_id);
        if((chip_id & ST25R3916_REG_IC_IDENTITY_ic_type_mask) !=
           ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916) {
            FURI_LOG_E(TAG, "Wrong chip id");
            error = FuriHalNfcErrorCommunication;
        }

        furi_hal_nfc_release();
    } while(false);
#endif

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

#if defined(FURI_HAL_NFC_CHIP_PN532)
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
#else
    do {
        error = furi_hal_nfc_acquire();
        if(error != FuriHalNfcErrorNone) {
            // Hardware was never touched — nothing to power down.
            FURI_LOG_E(TAG, "Failed to acquire NFC mutex during init");
            break;
        }

        const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
        FURI_LOG_D(TAG, "Setting default state");
        st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
        FURI_LOG_D(TAG, "Increasing IO driver strength");
        st25r3916_write_reg(handle, ST25R3916_REG_IO_CONF2, ST25R3916_REG_IO_CONF2_io_drv_lvl);
        uint8_t chip_id = 0;
        st25r3916_read_reg(handle, ST25R3916_REG_IC_IDENTITY, &chip_id);
        FURI_LOG_D(TAG, "Read Chip ID: 0x%02X", chip_id);
        if((chip_id & ST25R3916_REG_IC_IDENTITY_ic_type_mask) !=
           ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916) {
            FURI_LOG_E(TAG, "Wrong chip id");
            error = FuriHalNfcErrorCommunication;
            furi_hal_nfc_low_power_mode_start();
            furi_hal_nfc_release();
            break;
        }
        FURI_LOG_D(TAG, "Clearing and masking interrupts");
        // Mask all interrupts
        st25r3916_mask_irq(handle, ST25R3916_IRQ_MASK_ALL);
        FURI_LOG_D(TAG, "Initializing GPIO ISR");
        // Enable interrupts
        furi_hal_nfc_init_gpio_isr();
        FURI_LOG_D(TAG, "Disabling internal overheat protection");
        // Disable internal overheat protection
        st25r3916_change_test_reg_bits(handle, 0x04, 0x10, 0x10);

        FURI_LOG_D(TAG, "Turning on oscillator");
        error = furi_hal_nfc_turn_on_osc(handle);
        if(error != FuriHalNfcErrorNone) {
            FURI_LOG_E(TAG, "Failed to turn on oscillator");
            furi_hal_nfc_low_power_mode_start();
            furi_hal_nfc_release();
            break;
        }

        FURI_LOG_D(TAG, "Measuring voltage");
        // Measure voltage
        // Set measure power supply voltage source
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_REGULATOR_CONTROL,
            ST25R3916_REG_REGULATOR_CONTROL_mpsv_mask,
            ST25R3916_REG_REGULATOR_CONTROL_mpsv_vdd);
        // Enable timer and interrupt register
        FURI_LOG_D(TAG, "Enable timer and interrupt register");
        uint32_t irq_to_enable = ST25R3916_IRQ_MASK_DCT;
        //furi_hal_nfc_log_irq("Enabling IRQ for measurement", irq_to_enable);
        st25r3916_mask_irq(handle, ~irq_to_enable);
        st25r3916_direct_cmd(handle, ST25R3916_CMD_MEASURE_VDD);
        FURI_LOG_D(TAG, "furi_hal_nfc_event_wait_for_specific_irq");
        //furi_hal_nfc_log_irq("Waiting for specific IRQ", ST25R3916_IRQ_MASK_DCT);
        if(furi_hal_nfc_event_wait_for_specific_irq(handle, ST25R3916_IRQ_MASK_DCT, 100)) {
            FURI_LOG_D(TAG, "IRQ wait successful: Direct command finished.");
        } else {
            FURI_LOG_W(TAG, "IRQ wait TIMED OUT for direct command.");
        }
        //furi_hal_nfc_log_irq("Masking all IRQs", ST25R3916_IRQ_MASK_ALL);
        st25r3916_mask_irq(handle, ST25R3916_IRQ_MASK_ALL);
        uint8_t ad_res = 0;
        st25r3916_read_reg(handle, ST25R3916_REG_AD_RESULT, &ad_res);
        uint16_t mV = ((uint16_t)ad_res) * 23U;
        mV += (((((uint16_t)ad_res) * 4U) + 5U) / 10U);
        FURI_LOG_D(TAG, "Measured voltage: %u mV", mV);

        if(mV < 3600) {
            FURI_LOG_D(TAG, "Setting supply voltage to 3V mode");
            st25r3916_change_reg_bits(
                handle,
                ST25R3916_REG_IO_CONF2,
                ST25R3916_REG_IO_CONF2_sup3V,
                ST25R3916_REG_IO_CONF2_sup3V_3V);
        } else {
            FURI_LOG_D(TAG, "Setting supply voltage to 5V mode");
            st25r3916_change_reg_bits(
                handle,
                ST25R3916_REG_IO_CONF2,
                ST25R3916_REG_IO_CONF2_sup3V,
                ST25R3916_REG_IO_CONF2_sup3V_5V);
        }

        FURI_LOG_D(TAG, "Applying misc config settings");
        // Disable MCU CLK
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_IO_CONF1,
            ST25R3916_REG_IO_CONF1_out_cl_mask | ST25R3916_REG_IO_CONF1_lf_clk_off,
            0x07);
        // Disable MISO pull-down
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_IO_CONF2,
            ST25R3916_REG_IO_CONF2_miso_pd1 | ST25R3916_REG_IO_CONF2_miso_pd2,
            0x00);
        // Set tx driver resistance to 1 Om
        st25r3916_change_reg_bits(
            handle, ST25R3916_REG_TX_DRIVER, ST25R3916_REG_TX_DRIVER_d_res_mask, 0x00);
        // Use minimum non-overlap
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_RES_AM_MOD,
            ST25R3916_REG_RES_AM_MOD_fa3_f,
            ST25R3916_REG_RES_AM_MOD_fa3_f);

        // Set activation threashold
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_trg_mask,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_trg_105mV);
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_rfe_mask,
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_rfe_105mV);
        // Set deactivation threashold
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_trg_mask,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_trg_75mV);
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_rfe_mask,
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_rfe_75mV);
        // Disable external load modulation (not present on ELECHOUSE module)
        st25r3916_change_reg_bits(
            handle, ST25R3916_REG_AUX_MOD, ST25R3916_REG_AUX_MOD_lm_ext, 0x00);
        // Enable internal load modulation
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_AUX_MOD,
            ST25R3916_REG_AUX_MOD_lm_dri,
            ST25R3916_REG_AUX_MOD_lm_dri);
        // Adjust FDT
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_PASSIVE_TARGET,
            ST25R3916_REG_PASSIVE_TARGET_fdel_mask,
            (5U << ST25R3916_REG_PASSIVE_TARGET_fdel_shift));
        // Reduce RFO resistance in Modulated state
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_PT_MOD,
            ST25R3916_REG_PT_MOD_ptm_res_mask | ST25R3916_REG_PT_MOD_pt_res_mask,
            0x0f);
        // Enable RX start on first 4 bits
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_EMD_SUP_CONF,
            ST25R3916_REG_EMD_SUP_CONF_rx_start_emv,
            ST25R3916_REG_EMD_SUP_CONF_rx_start_emv_on);
        // Set antena tunning
        st25r3916_change_reg_bits(handle, ST25R3916_REG_ANT_TUNE_A, 0xff, 0x82);
        st25r3916_change_reg_bits(handle, ST25R3916_REG_ANT_TUNE_B, 0xff, 0x82);
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_OP_CONTROL,
            ST25R3916_REG_OP_CONTROL_en_fd_mask,
            ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);

        // Perform calibration
        if(st25r3916_check_reg(
               handle,
               ST25R3916_REG_REGULATOR_CONTROL,
               ST25R3916_REG_REGULATOR_CONTROL_reg_s,
               0x00)) {
            FURI_LOG_D(TAG, "Adjusting regulators");
            // Reset logic
            st25r3916_set_reg_bits(
                handle, ST25R3916_REG_REGULATOR_CONTROL, ST25R3916_REG_REGULATOR_CONTROL_reg_s);
            st25r3916_clear_reg_bits(
                handle, ST25R3916_REG_REGULATOR_CONTROL, ST25R3916_REG_REGULATOR_CONTROL_reg_s);
            st25r3916_direct_cmd(handle, ST25R3916_CMD_ADJUST_REGULATORS);
            furi_delay_ms(6);
        }

        FURI_LOG_D(TAG, "Initialization successful, entering low power mode");

        furi_hal_nfc_low_power_mode_start();
        furi_hal_nfc_release();

    } while(false);
#endif

    return error;
}

bool furi_hal_nfc_is_mine(void) {
    return furi_mutex_get_owner(furi_hal_nfc.mutex) == furi_thread_get_current_id();
}

FuriHalNfcError furi_hal_nfc_acquire(void) {
    FuriHalNfcError error = FuriHalNfcErrorNone;
    furi_check(furi_hal_nfc.mutex);
    if(!furi_hal_nfc_is_mine()) {
#if !defined(FURI_HAL_NFC_CHIP_PN532)
        furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
#endif
        if(furi_mutex_acquire(furi_hal_nfc.mutex, 1000) != FuriStatusOk) {
#if !defined(FURI_HAL_NFC_CHIP_PN532)
            furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
#endif
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
#if !defined(FURI_HAL_NFC_CHIP_PN532)
            furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
#endif
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

#if defined(FURI_HAL_NFC_CHIP_PN532)
    pn532_set_rf_field(PN532_I2C_BUS, false);
    furi_hal_nfc_deinit_gpio_isr();
    furi_hal_nfc_timers_deinit();
    furi_hal_nfc_event_stop();
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
    st25r3916_clear_reg_bits(
        handle,
        ST25R3916_REG_OP_CONTROL,
        (ST25R3916_REG_OP_CONTROL_en | ST25R3916_REG_OP_CONTROL_rx_en |
         ST25R3916_REG_OP_CONTROL_wu | ST25R3916_REG_OP_CONTROL_tx_en |
         ST25R3916_REG_OP_CONTROL_en_fd_mask));
    furi_hal_nfc_deinit_gpio_isr();
    furi_hal_nfc_timers_deinit();
    furi_hal_nfc_event_stop();
#endif

    if(own_lock) {
        furi_hal_nfc_release();
    }

    return error;
}

FuriHalNfcError furi_hal_nfc_low_power_mode_stop(void) {
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

#if defined(FURI_HAL_NFC_CHIP_PN532)
    furi_hal_nfc_init_gpio_isr();
    furi_hal_nfc_timers_init();
    pn532_wake_up(PN532_I2C_BUS);
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    do {
        furi_hal_nfc_init_gpio_isr();
        furi_hal_nfc_timers_init();
        error = furi_hal_nfc_turn_on_osc(handle);
        if(error != FuriHalNfcErrorNone) break;
        st25r3916_change_reg_bits(
            handle,
            ST25R3916_REG_OP_CONTROL,
            ST25R3916_REG_OP_CONTROL_en_fd_mask,
            ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);
    } while(false);
#endif

    furi_hal_nfc_event_start();

    furi_hal_nfc_release();
    return error;
}

#if !defined(FURI_HAL_NFC_CHIP_PN532)
static FuriHalNfcError furi_hal_nfc_poller_init_common(const FuriHalSpiBusHandle* handle) {
    // Disable wake up
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_OP_CONTROL, ST25R3916_REG_OP_CONTROL_wu);

    // Enable correlator
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_AUX,
        ST25R3916_REG_AUX_dis_corr,
        ST25R3916_REG_AUX_dis_corr_correlator);

    st25r3916_change_reg_bits(handle, ST25R3916_REG_ANT_TUNE_A, 0xff, 0x82);
    st25r3916_change_reg_bits(handle, ST25R3916_REG_ANT_TUNE_B, 0xFF, 0x82);

    // Disable overshoot/undershoot protection (not needed for DIY board antenna)
    st25r3916_write_reg(handle, ST25R3916_REG_OVERSHOOT_CONF1, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_OVERSHOOT_CONF2, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_UNDERSHOOT_CONF1, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_UNDERSHOOT_CONF2, 0x00);

    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_listener_init_common(const FuriHalSpiBusHandle* handle) {
    FURI_LOG_D(TAG, "Common listener initialization");
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}
#endif

FuriHalNfcError furi_hal_nfc_set_mode(FuriHalNfcMode mode, FuriHalNfcTech tech) {
    furi_check(mode < FuriHalNfcModeNum);
    furi_check(tech < FuriHalNfcTechNum);

    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

#if defined(FURI_HAL_NFC_CHIP_PN532)
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
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
    error = furi_hal_nfc_poller_init_common(handle);
    if(mode == FuriHalNfcModePoller) {
        do {
            if(error != FuriHalNfcErrorNone) break;
            error = furi_hal_nfc_tech[tech]->poller.init(handle);
        } while(false);
    } else if(mode == FuriHalNfcModeListener) {
        do {
            error = furi_hal_nfc_listener_init_common(handle);
            if(error != FuriHalNfcErrorNone) break;
            error = furi_hal_nfc_tech[tech]->listener.init(handle);
        } while(false);
    }
#endif

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

#if defined(FURI_HAL_NFC_CHIP_PN532)
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
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);

    const FuriHalNfcMode mode = furi_hal_nfc.mode;
    const FuriHalNfcTech tech = furi_hal_nfc.tech;
    if(mode == FuriHalNfcModePoller) {
        error = furi_hal_nfc_tech[tech]->poller.deinit(handle);
    } else if(mode == FuriHalNfcModeListener) {
        error = furi_hal_nfc_tech[tech]->listener.deinit(handle);
    }
    // Set default value in mode register
    st25r3916_write_reg(handle, ST25R3916_REG_MODE, ST25R3916_REG_MODE_om0);
    st25r3916_write_reg(handle, ST25R3916_REG_STREAM_MODE, 0);
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
    st25r3916_clear_reg_bits(
        handle,
        ST25R3916_REG_BIT_RATE,
        ST25R3916_REG_BIT_RATE_txrate_mask | ST25R3916_REG_BIT_RATE_rxrate_mask);

    // Write default values
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF1, 0);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF2,
        ST25R3916_REG_RX_CONF2_sqm_dyn | ST25R3916_REG_RX_CONF2_agc_en |
            ST25R3916_REG_RX_CONF2_agc_m);

    st25r3916_write_reg(
        handle,
        ST25R3916_REG_CORR_CONF1,
        ST25R3916_REG_CORR_CONF1_corr_s7 | ST25R3916_REG_CORR_CONF1_corr_s4 |
            ST25R3916_REG_CORR_CONF1_corr_s1 | ST25R3916_REG_CORR_CONF1_corr_s0);
    st25r3916_write_reg(handle, ST25R3916_REG_CORR_CONF2, 0);
#endif
    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_field_detect_start(void) {
    FURI_LOG_D(TAG, "Starting field detection");
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

#if defined(FURI_HAL_NFC_CHIP_PN532)
        // PN532 automatically detects carrier in target/listener mode
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_write_reg(
        handle,
        ST25R3916_REG_OP_CONTROL,
        ST25R3916_REG_OP_CONTROL_en | ST25R3916_REG_OP_CONTROL_en_fd_mask);
    st25r3916_write_reg(
        handle, ST25R3916_REG_MODE, ST25R3916_REG_MODE_targ | ST25R3916_REG_MODE_om0);
#endif
    furi_hal_nfc_release();
    return error;
}

FuriHalNfcError furi_hal_nfc_field_detect_stop(void) {
    FURI_LOG_D(TAG, "Stopping field detection");
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

#if !defined(FURI_HAL_NFC_CHIP_PN532)
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    st25r3916_clear_reg_bits(
        handle,
        ST25R3916_REG_OP_CONTROL,
        (ST25R3916_REG_OP_CONTROL_en | ST25R3916_REG_OP_CONTROL_en_fd_mask));
#endif
    furi_hal_nfc_release();
    return error;
}

bool furi_hal_nfc_field_is_present(void) {
    FURI_LOG_T(TAG, "Checking for external field presence");
    bool is_present = false;
    if(furi_hal_nfc_acquire() != FuriHalNfcErrorNone) return false;

#if defined(FURI_HAL_NFC_CHIP_PN532)
    is_present = false;
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    if(st25r3916_check_reg(
           handle,
           ST25R3916_REG_AUX_DISPLAY,
           ST25R3916_REG_AUX_DISPLAY_efd_o,
           ST25R3916_REG_AUX_DISPLAY_efd_o)) {
        is_present = true;
    }
#endif

    FURI_LOG_T(TAG, "Field is present: %s", is_present ? "true" : "false");
    furi_hal_nfc_release();
    return is_present;
}

FuriHalNfcError furi_hal_nfc_poller_field_on(void) {
    FURI_LOG_T(TAG, "Turning poller field on");
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error != FuriHalNfcErrorNone) return error;

#if defined(FURI_HAL_NFC_CHIP_PN532)
    Pn532Error pn_err = pn532_set_rf_field(PN532_I2C_BUS, true);
    error = (pn_err == Pn532ErrorNone) ? FuriHalNfcErrorNone : FuriHalNfcErrorCommunication;
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;

    if(!st25r3916_check_reg(
           handle,
           ST25R3916_REG_OP_CONTROL,
           ST25R3916_REG_OP_CONTROL_tx_en,
           ST25R3916_REG_OP_CONTROL_tx_en)) {
        // Set min guard time
        st25r3916_write_reg(handle, ST25R3916_REG_FIELD_ON_GT, 0);
        // Enable tx rx
        st25r3916_set_reg_bits(
            handle,
            ST25R3916_REG_OP_CONTROL,
            (ST25R3916_REG_OP_CONTROL_rx_en | ST25R3916_REG_OP_CONTROL_tx_en));
    }
#endif

    furi_hal_nfc_release();
    return error;
}

#if defined(FURI_HAL_NFC_CHIP_PN532)
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
#endif

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

#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    size_t tx_bytes = (tx_bits + 7) / 8;

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

    Pn532Error pn_err = pn532_in_data_exchange(
        PN532_I2C_BUS,
        1,
        actual_tx,
        actual_len,
        furi_hal_pn532_ctx.rx_buf,
        &furi_hal_pn532_ctx.rx_len,
        500);

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
#else
    // Prepare tx
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_clear_reg_bits(
        handle, ST25R3916_REG_TIMER_EMV_CONTROL, ST25R3916_REG_TIMER_EMV_CONTROL_nrt_emv);
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS |
         ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
         ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_NRE);
    st25r3916_mask_irq(handle, ~interrupts);

    st25r3916_write_fifo(handle, tx_data, tx_bits);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);
#endif
    return err;
}

FuriHalNfcError furi_hal_nfc_common_fifo_tx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    FURI_LOG_T(TAG, "Common FIFO TX, %zu bits", tx_bits);
    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) return err;
#if defined(FURI_HAL_NFC_CHIP_PN532)
    err = furi_hal_nfc_poller_tx_common(handle, tx_data, tx_bits);
#else
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_write_fifo(handle, tx_data, tx_bits);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);
#endif
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

#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    if(furi_hal_pn532_ctx.rx_len > rx_data_size) {
        FURI_LOG_W(TAG, "FIFO RX buffer overflow");
        error = FuriHalNfcErrorBufferOverflow;
    } else {
        memcpy(rx_data, furi_hal_pn532_ctx.rx_buf, furi_hal_pn532_ctx.rx_len);
        *rx_bits = furi_hal_pn532_ctx.rx_len * 8;
        error = FuriHalNfcErrorNone;
    }
#else
    if(!st25r3916_read_fifo(handle, rx_data, rx_data_size, rx_bits)) {
        FURI_LOG_W(TAG, "FIFO RX buffer overflow");
        error = FuriHalNfcErrorBufferOverflow;
    }
#endif
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

#if defined(FURI_HAL_NFC_CHIP_PN532)
    furi_hal_pn532_ctx.rx_len = 0;
#else
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
    st25r3916_direct_cmd(handle, ST25R3916_CMD_STOP);
#endif
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
#if !defined(FURI_HAL_NFC_CHIP_PN532)
    const FuriHalSpiBusHandle* handle = &furi_hal_spi_bus_handle_nfc;
    st25r3916_direct_cmd(handle, ST25R3916_CMD_UNMASK_RECEIVE_DATA);
#endif
    furi_hal_nfc_release();
    return FuriHalNfcErrorNone;
}
