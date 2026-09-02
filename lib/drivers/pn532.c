#include "pn532.h"
#include <furi.h>
#include <string.h>

#define TAG "Pn532"

static const uint8_t pn532_ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};

static bool pn532_is_ready(const FuriHalI2cBusHandle* handle) {
    uint8_t status = 0;
    furi_hal_i2c_acquire(handle);
    bool ok = furi_hal_i2c_rx(handle, PN532_I2C_ADDR_8BIT, &status, 1, 20);
    furi_hal_i2c_release(handle);
    return ok && ((status & 0x01) == 0x01);
}

static Pn532Error pn532_wait_ready(const FuriHalI2cBusHandle* handle, uint32_t timeout_ms) {
    uint32_t start = furi_get_tick();
    while((furi_get_tick() - start) < timeout_ms) {
        if(pn532_is_ready(handle)) {
            return Pn532ErrorNone;
        }
        furi_delay_ms(2);
    }
    return Pn532ErrorTimeout;
}

static Pn532Error pn532_read_ack(const FuriHalI2cBusHandle* handle, uint32_t timeout_ms) {
    Pn532Error err = pn532_wait_ready(handle, timeout_ms);
    if(err != Pn532ErrorNone) return err;

    furi_delay_ms(1);

    uint8_t buf[7] = {0};
    furi_hal_i2c_acquire(handle);
    bool ok = furi_hal_i2c_rx(handle, PN532_I2C_ADDR_8BIT, buf, sizeof(buf), 50);
    furi_hal_i2c_release(handle);

    if(!ok) return Pn532ErrorBus;
    if((buf[0] & 0x01) == 0) return Pn532ErrorTimeout;

    if(memcmp(&buf[1], pn532_ack, sizeof(pn532_ack)) != 0) {
        return Pn532ErrorAckMismatch;
    }

    return Pn532ErrorNone;
}

static Pn532Error
    pn532_write_command(const FuriHalI2cBusHandle* handle, const uint8_t* cmd, size_t cmd_len) {
    if(cmd_len > 250) return Pn532ErrorBufferOverflow;

    uint8_t frame[260];
    uint8_t len = (uint8_t)(cmd_len + 1); // including TFI 0xD4

    frame[0] = PN532_PREAMBLE;
    frame[1] = PN532_STARTCODE1;
    frame[2] = PN532_STARTCODE2;
    frame[3] = len;
    frame[4] = (uint8_t)(~len + 1);
    frame[5] = PN532_HOSTTOPN532;

    uint8_t sum = PN532_HOSTTOPN532;
    for(size_t i = 0; i < cmd_len; i++) {
        frame[6 + i] = cmd[i];
        sum += cmd[i];
    }

    frame[6 + cmd_len] = (uint8_t)(~sum + 1);
    frame[7 + cmd_len] = PN532_POSTAMBLE;

    size_t total_len = 8 + cmd_len;

    furi_hal_i2c_acquire(handle);
    bool ok = furi_hal_i2c_tx(handle, PN532_I2C_ADDR_8BIT, frame, total_len, 100);
    furi_hal_i2c_release(handle);

    if(!ok) return Pn532ErrorBus;

    return pn532_read_ack(handle, 100);
}

static Pn532Error pn532_read_response(
    const FuriHalI2cBusHandle* handle,
    uint8_t expected_cmd,
    uint8_t* rx_data,
    size_t* rx_len,
    uint32_t timeout_ms) {
    Pn532Error err = pn532_wait_ready(handle, timeout_ms);
    if(err != Pn532ErrorNone) return err;

    furi_delay_ms(1);

    uint8_t raw[264] = {0};
    furi_hal_i2c_acquire(handle);
    bool ok = furi_hal_i2c_rx(
        handle, PN532_I2C_ADDR_8BIT, raw, sizeof(raw), timeout_ms > 100 ? timeout_ms : 100);
    furi_hal_i2c_release(handle);
    if(!ok) return Pn532ErrorBus;

    // Scan for preamble (0x00) and start code (0x00 0xFF)
    size_t start_idx = 0;
    bool found = false;
    for(size_t i = 0; i < 16; i++) {
        if(raw[i] == PN532_PREAMBLE && raw[i + 1] == PN532_STARTCODE1 &&
           raw[i + 2] == PN532_STARTCODE2) {
            start_idx = i;
            found = true;
            break;
        }
    }
    if(!found) {
        return Pn532ErrorAckMismatch;
    }

    size_t len_idx = start_idx + 3;
    uint8_t len = raw[len_idx];
    uint8_t lcs = raw[len_idx + 1];
    if((uint8_t)(len + lcs) != 0 || len < 2) {
        return Pn532ErrorChecksum;
    }

    size_t tfi_idx = len_idx + 2;
    if(tfi_idx >= sizeof(raw) || raw[tfi_idx] != PN532_PN532TOHOST) {
        return Pn532ErrorAckMismatch;
    }

    uint8_t cmd_code = raw[tfi_idx + 1];
    if(cmd_code != (expected_cmd + 1)) {
        return Pn532ErrorAckMismatch;
    }

    // Verify DCS checksum: sum of bytes from TFI through DCS (at tfi_idx + len)
    if(tfi_idx + len >= sizeof(raw)) {
        return Pn532ErrorBufferOverflow;
    }
    uint8_t dcs_sum = 0;
    for(size_t i = tfi_idx; i <= (tfi_idx + len); i++) {
        dcs_sum += raw[i];
    }
    if(dcs_sum != 0) {
        return Pn532ErrorChecksum;
    }

    size_t payload_len = len - 2; // Subtract TFI and cmd_code
    if(rx_data && rx_len) {
        if(*rx_len < payload_len) {
            return Pn532ErrorBufferOverflow;
        }
        memcpy(rx_data, &raw[tfi_idx + 2], payload_len);
        *rx_len = payload_len;
    }

    return Pn532ErrorNone;
}

Pn532Error pn532_wake_up(const FuriHalI2cBusHandle* handle) {
    // Send a dummy write on I2C to wake up from High-Speed / sleep mode
    uint8_t dummy[1] = {0x00};
    furi_hal_i2c_acquire(handle);
    furi_hal_i2c_tx(handle, PN532_I2C_ADDR_8BIT, dummy, 1, 20);
    furi_hal_i2c_release(handle);
    furi_delay_ms(15);
    return Pn532ErrorNone;
}

Pn532Error pn532_init(const FuriHalI2cBusHandle* handle) {
    pn532_wake_up(handle);
    furi_delay_ms(10);

    // Test communication with SAMConfiguration
    Pn532Error err = pn532_sam_config(handle, 0x01, 0x14, false);
    if(err != Pn532ErrorNone) {
        FURI_LOG_E(TAG, "PN532 SAM config failed: %d", err);
        return err;
    }

    // Set passive activation retries to finite count to prevent indefinite blocking
    pn532_set_passive_activation_retries(handle, 0x05);

    FURI_LOG_I(TAG, "PN532 initialized successfully over I2C");
    return Pn532ErrorNone;
}

Pn532Error pn532_get_firmware_version(
    const FuriHalI2cBusHandle* handle,
    uint8_t* ic,
    uint8_t* ver,
    uint8_t* rev,
    uint8_t* support) {
    uint8_t cmd[1] = {PN532_CMD_GETFIRMWAREVERSION};
    Pn532Error err = pn532_write_command(handle, cmd, sizeof(cmd));
    if(err != Pn532ErrorNone) return err;

    uint8_t resp[4] = {0};
    size_t resp_len = sizeof(resp);
    err = pn532_read_response(handle, PN532_CMD_GETFIRMWAREVERSION, resp, &resp_len, 100);
    if(err != Pn532ErrorNone) return err;

    if(resp_len >= 4) {
        if(ic) *ic = resp[0];
        if(ver) *ver = resp[1];
        if(rev) *rev = resp[2];
        if(support) *support = resp[3];
        return Pn532ErrorNone;
    }

    return Pn532ErrorInternal;
}

Pn532Error pn532_sam_config(
    const FuriHalI2cBusHandle* handle,
    uint8_t mode,
    uint8_t timeout,
    bool use_irq) {
    uint8_t cmd[4];
    cmd[0] = PN532_CMD_SAMCONFIGURATION;
    cmd[1] = mode;
    cmd[2] = timeout;
    cmd[3] = use_irq ? 0x01 : 0x00;

    Pn532Error err = pn532_write_command(handle, cmd, sizeof(cmd));
    if(err != Pn532ErrorNone) return err;

    return pn532_read_response(handle, PN532_CMD_SAMCONFIGURATION, NULL, NULL, 100);
}

Pn532Error
    pn532_set_passive_activation_retries(const FuriHalI2cBusHandle* handle, uint8_t max_retries) {
    uint8_t cmd[4];
    cmd[0] = PN532_CMD_RFCONFIGURATION;
    cmd[1] = 0x05; // Config item 5 (MaxRetries)
    cmd[2] = 0xFF; // MxRtyATR
    cmd[3] = max_retries; // MxRtyPassiveActivation

    Pn532Error err = pn532_write_command(handle, cmd, sizeof(cmd));
    if(err != Pn532ErrorNone) return err;

    return pn532_read_response(handle, PN532_CMD_RFCONFIGURATION, NULL, NULL, 100);
}

Pn532Error pn532_set_rf_field(const FuriHalI2cBusHandle* handle, bool state) {
    uint8_t cmd[3];
    cmd[0] = PN532_CMD_RFCONFIGURATION;
    cmd[1] = 0x01; // RF field configuration item
    // Configuration byte: bit 0 = Auto RFCA (1), bit 1 = RF field (1=on, 0=off)
    cmd[2] = state ? 0x03 : 0x00;

    Pn532Error err = pn532_write_command(handle, cmd, sizeof(cmd));
    if(err != Pn532ErrorNone) return err;

    return pn532_read_response(handle, PN532_CMD_RFCONFIGURATION, NULL, NULL, 100);
}

Pn532Error pn532_in_list_passive_target_iso14443a(
    const FuriHalI2cBusHandle* handle,
    Pn532TargetIso14443a* target,
    uint32_t timeout_ms) {
    if(!target) return Pn532ErrorInternal;
    memset(target, 0, sizeof(Pn532TargetIso14443a));

    uint8_t cmd[3];
    cmd[0] = PN532_CMD_INLISTPASSIVETARGET;
    cmd[1] = 0x01; // Max 1 target
    cmd[2] = PN532_BAUD_ISO14443A; // 106 kbps Type A

    Pn532Error err = pn532_write_command(handle, cmd, sizeof(cmd));
    if(err != Pn532ErrorNone) return err;

    uint8_t resp[64] = {0};
    size_t resp_len = sizeof(resp);
    err = pn532_read_response(handle, PN532_CMD_INLISTPASSIVETARGET, resp, &resp_len, timeout_ms);
    if(err != Pn532ErrorNone) return err;

    if(resp_len < 1 || resp[0] == 0) {
        return Pn532ErrorTimeout; // No target found
    }

    // Response structure:
    // [0]: NbTg (1)
    // [1]: Tg (1)
    // [2..3]: SENS_RES (ATQA)
    // [4]: SEL_RES (SAK)
    // [5]: NFCID Length
    // [6..6+NFCIDLen-1]: NFCID (UID)
    // [Optional]: ATS Length + ATS Data
    target->tg = resp[1];
    target->sens_res = (uint16_t)resp[2] | ((uint16_t)resp[3] << 8);
    target->sel_res = resp[4];
    target->uid_len = resp[5];
    if(target->uid_len > 10) target->uid_len = 10;
    memcpy(target->uid, &resp[6], target->uid_len);

    size_t offset = 6 + target->uid_len;
    if(offset < resp_len) {
        target->ats_len = resp[offset];
        if(target->ats_len > 0 && (offset + 1 + target->ats_len) <= resp_len) {
            size_t copy_len = target->ats_len > 32 ? 32 : target->ats_len;
            memcpy(target->ats, &resp[offset + 1], copy_len);
        }
    }

    return Pn532ErrorNone;
}

Pn532Error pn532_in_list_passive_target_iso14443b(
    const FuriHalI2cBusHandle* handle,
    Pn532TargetIso14443b* target,
    uint32_t timeout_ms) {
    if(!target) return Pn532ErrorInternal;
    memset(target, 0, sizeof(Pn532TargetIso14443b));

    uint8_t cmd[3];
    cmd[0] = PN532_CMD_INLISTPASSIVETARGET;
    cmd[1] = 0x01; // Max 1 target
    cmd[2] = PN532_BAUD_ISO14443B; // 106 kbps Type B

    Pn532Error err = pn532_write_command(handle, cmd, sizeof(cmd));
    if(err != Pn532ErrorNone) return err;

    uint8_t resp[64] = {0};
    size_t resp_len = sizeof(resp);
    err = pn532_read_response(handle, PN532_CMD_INLISTPASSIVETARGET, resp, &resp_len, timeout_ms);
    if(err != Pn532ErrorNone) return err;

    if(resp_len < 1 || resp[0] == 0) {
        return Pn532ErrorTimeout; // No target found
    }

    // Response structure:
    // [0]: NbTg (1)
    // [1]: Tg (1)
    // [2..13]: ATQB (12 bytes)
    // [14..17]: Application Data (4 bytes)
    // [18..20]: Protocol Info (3 bytes)
    // [21]: NFCID Length (UID Length)
    // [22..22+NFCIDLen-1]: NFCID (UID)
    // [Optional]: ATS Length + ATS Data
    target->tg = resp[1];
    memcpy(target->atqb, &resp[2], 12);
    memcpy(target->app_data, &resp[14], 4);
    memcpy(target->protocol_info, &resp[18], 3);

    size_t offset = 21;
    if(offset < resp_len) {
        target->uid_len = resp[offset];
        if(target->uid_len > 10) target->uid_len = 10;
        if(target->uid_len > 0 && (offset + 1 + target->uid_len) <= resp_len) {
            memcpy(target->uid, &resp[offset + 1], target->uid_len);
        }
        offset += 1 + target->uid_len;
    }

    if(offset < resp_len) {
        target->ats_len = resp[offset];
        if(target->ats_len > 0 && (offset + 1 + target->ats_len) <= resp_len) {
            size_t copy_len = target->ats_len > 32 ? 32 : target->ats_len;
            memcpy(target->ats, &resp[offset + 1], copy_len);
        }
    }

    return Pn532ErrorNone;
}

Pn532Error pn532_in_communicate_thru(
    const FuriHalI2cBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_len,
    uint8_t* rx_data,
    size_t* rx_len,
    uint32_t timeout_ms) {
    if(tx_len > 240) return Pn532ErrorBufferOverflow;

    uint8_t cmd[256];
    cmd[0] = PN532_CMD_INCOMMUNICATETHRU;
    memcpy(&cmd[1], tx_data, tx_len);

    Pn532Error err = pn532_write_command(handle, cmd, 1 + tx_len);
    if(err != Pn532ErrorNone) return err;

    uint8_t resp[260];
    size_t resp_sz = sizeof(resp);
    err = pn532_read_response(handle, PN532_CMD_INCOMMUNICATETHRU, resp, &resp_sz, timeout_ms);
    if(err != Pn532ErrorNone) return err;

    if(resp_sz < 1) return Pn532ErrorInternal;

    uint8_t status = resp[0];
    if((status & 0x3F) != 0x00) {
        return Pn532ErrorInternal; // Error returned from card
    }

    size_t data_len = resp_sz - 1;
    if(rx_data && rx_len) {
        if(*rx_len < data_len) return Pn532ErrorBufferOverflow;
        memcpy(rx_data, &resp[1], data_len);
        *rx_len = data_len;
    }

    return Pn532ErrorNone;
}

Pn532Error pn532_in_data_exchange(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    const uint8_t* tx_data,
    size_t tx_len,
    uint8_t* rx_data,
    size_t* rx_len,
    uint32_t timeout_ms) {
    if(tx_len > 240) return Pn532ErrorBufferOverflow;

    uint8_t cmd[256];
    cmd[0] = PN532_CMD_INDATAEXCHANGE;
    cmd[1] = tg;
    memcpy(&cmd[2], tx_data, tx_len);

    uint8_t resp[260];
    size_t resp_sz = sizeof(resp);
    Pn532Error err = Pn532ErrorInternal;

    int max_retries = (tx_len == 2 && tx_data[0] == 0x50) ? 1 : 2;
    for(int retry = 0; retry < max_retries; retry++) {
        if(retry > 0) {
            FURI_LOG_D(TAG, "InDataExchange retry %d", retry);
            furi_delay_ms(3);
        }
        err = pn532_write_command(handle, cmd, 2 + tx_len);
        if(err != Pn532ErrorNone) {
            FURI_LOG_D(TAG, "InDataExchange write failed: %d", err);
            return err;
        }
        resp_sz = sizeof(resp);
        err = pn532_read_response(handle, PN532_CMD_INDATAEXCHANGE, resp, &resp_sz, timeout_ms);
        if(err == Pn532ErrorNone) break;
        if(err != Pn532ErrorChecksum) {
            FURI_LOG_D(TAG, "InDataExchange read failed: %d", err);
            return err;
        }
    }
    if(err != Pn532ErrorNone) {
        FURI_LOG_D(TAG, "InDataExchange read failed after retries: %d", err);
        return err;
    }

    if(resp_sz < 1) {
        FURI_LOG_D(TAG, "InDataExchange resp_sz=%zu", resp_sz);
        return Pn532ErrorInternal;
    }

    uint8_t status = resp[0];
    if((status & 0x3F) != 0x00) {
        FURI_LOG_D(TAG, "InDataExchange card error: status=0x%02X", status);
        return Pn532ErrorInternal; // Error returned from card
    }

    size_t data_len = resp_sz - 1;
    if(rx_data && rx_len) {
        if(*rx_len < data_len) return Pn532ErrorBufferOverflow;
        memcpy(rx_data, &resp[1], data_len);
        *rx_len = data_len;
    }

    return Pn532ErrorNone;
}

Pn532Error pn532_mifare_classic_auth(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    uint8_t auth_cmd,
    uint8_t block,
    const uint8_t* key,
    const uint8_t* uid,
    uint8_t uid_len) {
    if(!key || !uid) return Pn532ErrorInternal;

    uint8_t auth_payload[16];
    auth_payload[0] = auth_cmd; // 0x60 or 0x61
    auth_payload[1] = block;
    memcpy(&auth_payload[2], key, 6);
    // For 7-byte UID MIFARE Classic cards, Crypto-1 uses the 4-byte NUID (bytes 3..6)
    const uint8_t* auth_uid = (uid_len == 7) ? &uid[3] : uid;
    uint8_t copy_len = uid_len > 4 ? 4 : uid_len;
    memcpy(&auth_payload[8], auth_uid, copy_len);

    uint8_t rx[4];
    size_t rx_len = sizeof(rx);
    return pn532_in_data_exchange(
        handle, tg, auth_payload, 8 + copy_len, rx, &rx_len, 200);
}

Pn532Error pn532_mifare_read_block(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    uint8_t block,
    uint8_t* block_data,
    size_t* block_len) {
    if(!block_data || !block_len) return Pn532ErrorInternal;

    uint8_t tx[2] = {PN532_MIFARE_CMD_READ, block};
    return pn532_in_data_exchange(handle, tg, tx, sizeof(tx), block_data, block_len, 200);
}

Pn532Error pn532_mifare_classic_write_block(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    uint8_t block,
    const uint8_t* block_data) {
    if(!block_data) return Pn532ErrorInternal;

    uint8_t tx[18];
    tx[0] = PN532_MIFARE_CMD_WRITE;
    tx[1] = block;
    memcpy(&tx[2], block_data, 16);

    uint8_t rx[4];
    size_t rx_len = sizeof(rx);
    return pn532_in_data_exchange(handle, tg, tx, sizeof(tx), rx, &rx_len, 200);
}

Pn532Error pn532_mifare_ultralight_write_page(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    uint8_t page,
    const uint8_t* page_data) {
    if(!page_data) return Pn532ErrorInternal;

    uint8_t tx[6];
    tx[0] = PN532_MIFARE_ULTRALIGHT_WRITE;
    tx[1] = page;
    memcpy(&tx[2], page_data, 4);

    uint8_t rx[4];
    size_t rx_len = sizeof(rx);
    return pn532_in_data_exchange(handle, tg, tx, sizeof(tx), rx, &rx_len, 200);
}

Pn532Error pn532_in_release(const FuriHalI2cBusHandle* handle, uint8_t tg) {
    uint8_t cmd[2] = {PN532_CMD_INRELEASE, tg};
    Pn532Error err = pn532_write_command(handle, cmd, sizeof(cmd));
    if(err != Pn532ErrorNone) return err;

    return pn532_read_response(handle, PN532_CMD_INRELEASE, NULL, NULL, 100);
}

Pn532Error pn532_tg_init_as_target(
    const FuriHalI2cBusHandle* handle,
    const uint8_t* mifare_params,
    size_t params_len,
    uint8_t* rx_cmd,
    size_t* rx_len,
    uint32_t timeout_ms) {
    uint8_t cmd[64];
    cmd[0] = PN532_CMD_TGINITASTARGET;
    cmd[1] = 0x01; // Mode: Passive only (Bit 0 = 1)

    size_t copy_sz = params_len > 36 ? 36 : params_len;
    if(mifare_params && copy_sz > 0) {
        memcpy(&cmd[2], mifare_params, copy_sz);
    } else {
        // Default Mifare 1K target params: SENS_RES (0x0004), NFCID (4 bytes), SEL_RES (0x08)
        cmd[2] = 0x04;
        cmd[3] = 0x00; // SENS_RES
        cmd[4] = 0x12;
        cmd[5] = 0x34;
        cmd[6] = 0x56; // NFCID1t
        cmd[7] = 0x08; // SEL_RES (Mifare 1K)
        copy_sz = 6;
    }

    Pn532Error err = pn532_write_command(handle, cmd, 2 + copy_sz);
    if(err != Pn532ErrorNone) return err;

    return pn532_read_response(handle, PN532_CMD_TGINITASTARGET, rx_cmd, rx_len, timeout_ms);
}

Pn532Error pn532_tg_get_data(
    const FuriHalI2cBusHandle* handle,
    uint8_t* rx_data,
    size_t* rx_len,
    uint32_t timeout_ms) {
    uint8_t cmd[1] = {PN532_CMD_TGGETDATA};
    Pn532Error err = pn532_write_command(handle, cmd, sizeof(cmd));
    if(err != Pn532ErrorNone) return err;

    return pn532_read_response(handle, PN532_CMD_TGGETDATA, rx_data, rx_len, timeout_ms);
}

Pn532Error pn532_tg_set_data(
    const FuriHalI2cBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_len,
    uint32_t timeout_ms) {
    if(tx_len > 250) return Pn532ErrorBufferOverflow;

    uint8_t cmd[260];
    cmd[0] = PN532_CMD_TGSETDATA;
    memcpy(&cmd[1], tx_data, tx_len);

    Pn532Error err = pn532_write_command(handle, cmd, 1 + tx_len);
    if(err != Pn532ErrorNone) return err;

    return pn532_read_response(handle, PN532_CMD_TGSETDATA, NULL, NULL, timeout_ms);
}
