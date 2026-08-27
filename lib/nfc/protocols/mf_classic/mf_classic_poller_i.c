#include "mf_classic_poller_i.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <furi_hal_resources.h>

#if defined(FURI_HAL_NFC_CHIP_PN532)
#include <pn532.h>
#endif

#include <nfc/helpers/iso14443_crc.h>

#define TAG "MfClassicPoller"

MfClassicError mf_classic_process_error(Iso14443_3aError error) {
    MfClassicError ret = MfClassicErrorNone;

    switch(error) {
    case Iso14443_3aErrorNone:
        ret = MfClassicErrorNone;
        break;
    case Iso14443_3aErrorNotPresent:
        ret = MfClassicErrorNotPresent;
        break;
    case Iso14443_3aErrorColResFailed:
    case Iso14443_3aErrorCommunication:
    case Iso14443_3aErrorWrongCrc:
        ret = MfClassicErrorProtocol;
        break;
    case Iso14443_3aErrorTimeout:
        ret = MfClassicErrorTimeout;
        break;
    default:
        ret = MfClassicErrorProtocol;
        break;
    }

    return ret;
}

static MfClassicError mf_classic_poller_get_nt_common(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKeyType key_type,
    MfClassicNt* nt,
    bool is_nested,
    bool backdoor_auth) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(instance);
    UNUSED(block_num);
    UNUSED(key_type);
    UNUSED(is_nested);
    UNUSED(backdoor_auth);
    if(nt) {
        furi_hal_random_fill_buf(nt->data, sizeof(MfClassicNt));
    }
    return MfClassicErrorNone;
#else
    MfClassicError ret = MfClassicErrorNone;
    Iso14443_3aError error = Iso14443_3aErrorNone;

    do {
        uint8_t auth_type;
        if(!backdoor_auth) {
            auth_type = (key_type == MfClassicKeyTypeB) ? MF_CLASSIC_CMD_AUTH_KEY_B :
                                                          MF_CLASSIC_CMD_AUTH_KEY_A;
        } else {
            auth_type = (key_type == MfClassicKeyTypeB) ? MF_CLASSIC_CMD_BACKDOOR_AUTH_KEY_B :
                                                          MF_CLASSIC_CMD_BACKDOOR_AUTH_KEY_A;
        }
        uint8_t auth_cmd[2] = {auth_type, block_num};
        bit_buffer_copy_bytes(instance->tx_plain_buffer, auth_cmd, sizeof(auth_cmd));

        if(is_nested) {
            iso14443_crc_append(Iso14443CrcTypeA, instance->tx_plain_buffer);
            crypto1_encrypt(
                instance->crypto, NULL, instance->tx_plain_buffer, instance->tx_encrypted_buffer);
            error = iso14443_3a_poller_txrx_custom_parity(
                instance->iso14443_3a_poller,
                instance->tx_encrypted_buffer,
                instance->rx_plain_buffer, // NT gets decrypted by mf_classic_async_auth
                MF_CLASSIC_FWT_FC);
            if(error != Iso14443_3aErrorNone) {
                ret = mf_classic_process_error(error);
                break;
            }
        } else {
            error = iso14443_3a_poller_send_standard_frame(
                instance->iso14443_3a_poller,
                instance->tx_plain_buffer,
                instance->rx_plain_buffer,
                MF_CLASSIC_FWT_FC);
            if(error != Iso14443_3aErrorWrongCrc) {
                ret = mf_classic_process_error(error);
                break;
            }
        }
        if(bit_buffer_get_size_bytes(instance->rx_plain_buffer) != sizeof(MfClassicNt)) {
            ret = MfClassicErrorProtocol;
            break;
        }

        if(nt) {
            bit_buffer_write_bytes(instance->rx_plain_buffer, nt->data, sizeof(MfClassicNt));
        }
    } while(false);

    return ret;
#endif
}

MfClassicError mf_classic_poller_get_nt(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKeyType key_type,
    MfClassicNt* nt,
    bool backdoor_auth) {
    furi_check(instance);

    return mf_classic_poller_get_nt_common(
        instance, block_num, key_type, nt, false, backdoor_auth);
}

MfClassicError mf_classic_poller_get_nt_nested(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKeyType key_type,
    MfClassicNt* nt,
    bool backdoor_auth) {
    furi_check(instance);

    return mf_classic_poller_get_nt_common(instance, block_num, key_type, nt, true, backdoor_auth);
}

MfClassicError mf_classic_poller_auth_common(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    MfClassicAuthContext* data,
    bool is_nested,
    bool backdoor_auth,
    bool early_ret) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(data);
    UNUSED(is_nested);
    UNUSED(backdoor_auth);
    UNUSED(early_ret);
    MfClassicError ret = MfClassicErrorNone;

    do {
        iso14443_3a_copy(
            instance->data->iso14443_3a_data,
            iso14443_3a_poller_get_data(instance->iso14443_3a_poller));

        const uint8_t* uid = instance->data->iso14443_3a_data->uid;
        uint8_t uid_len = instance->data->iso14443_3a_data->uid_len;
        uint8_t auth_cmd = (key_type == MfClassicKeyTypeB) ? PN532_MIFARE_CMD_AUTH_B :
                                                             PN532_MIFARE_CMD_AUTH_A;

        Pn532Error err = pn532_mifare_classic_auth(
            PN532_I2C_BUS, 1, auth_cmd, block_num, key->data, uid, uid_len);

        if(err == Pn532ErrorNone) {
            instance->auth_state = MfClassicAuthStatePassed;
            ret = MfClassicErrorNone;
        } else {
            // Re-select target if auth failed to restore card RF communication state
            Pn532TargetIso14443a target;
            pn532_in_list_passive_target_iso14443a(PN532_I2C_BUS, &target, 100);
            ret = MfClassicErrorAuth;
        }
    } while(false);

    return ret;
#else
    MfClassicError ret = MfClassicErrorNone;
    Iso14443_3aError error = Iso14443_3aErrorNone;

    do {
        iso14443_3a_copy(
            instance->data->iso14443_3a_data,
            iso14443_3a_poller_get_data(instance->iso14443_3a_poller));

        MfClassicNt nt = {};
        if(is_nested) {
            ret =
                mf_classic_poller_get_nt_nested(instance, block_num, key_type, &nt, backdoor_auth);
        } else {
            ret = mf_classic_poller_get_nt(instance, block_num, key_type, &nt, backdoor_auth);
        }
        if(ret != MfClassicErrorNone) break;
        if(data) {
            data->nt = nt;
        }
        if(early_ret) break;

        uint32_t cuid = iso14443_3a_get_cuid(instance->data->iso14443_3a_data);
        uint64_t key_num = bit_lib_bytes_to_num_be(key->data, sizeof(MfClassicKey));
        MfClassicNr nr = {};
        furi_hal_random_fill_buf(nr.data, sizeof(MfClassicNr));

        crypto1_encrypt_reader_nonce(
            instance->crypto,
            key_num,
            cuid,
            nt.data,
            nr.data,
            instance->tx_encrypted_buffer,
            is_nested);
        error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            MF_CLASSIC_FWT_FC);

        if(error != Iso14443_3aErrorNone) {
            ret = mf_classic_process_error(error);
            break;
        }
        if(bit_buffer_get_size_bytes(instance->rx_encrypted_buffer) != 4) {
            ret = MfClassicErrorAuth;
        }

        crypto1_word(instance->crypto, 0, 0);
        instance->auth_state = MfClassicAuthStatePassed;

        if(data) {
            data->nr = nr;
            const uint8_t* nr_ar = bit_buffer_get_data(instance->tx_encrypted_buffer);
            memcpy(data->ar.data, &nr_ar[4], sizeof(MfClassicAr));
            bit_buffer_write_bytes(
                instance->rx_encrypted_buffer, data->at.data, sizeof(MfClassicAt));
        }
    } while(false);

    if(ret != MfClassicErrorNone) {
        iso14443_3a_poller_halt(instance->iso14443_3a_poller);
    }

    return ret;
#endif
}

MfClassicError mf_classic_poller_auth(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    MfClassicAuthContext* data,
    bool backdoor_auth) {
    furi_check(instance);
    furi_check(key);
    return mf_classic_poller_auth_common(
        instance, block_num, key, key_type, data, false, backdoor_auth, false);
}

MfClassicError mf_classic_poller_auth_nested(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    MfClassicAuthContext* data,
    bool backdoor_auth,
    bool early_ret) {
    furi_check(instance);
    furi_check(key);
    return mf_classic_poller_auth_common(
        instance, block_num, key, key_type, data, true, backdoor_auth, early_ret);
}

MfClassicError mf_classic_poller_halt(MfClassicPoller* instance) {
    furi_check(instance);
#if defined(FURI_HAL_NFC_CHIP_PN532)
    instance->auth_state = MfClassicAuthStateIdle;
    instance->iso14443_3a_poller->state = Iso14443_3aPollerStateIdle;
    Pn532TargetIso14443a target;
    pn532_in_list_passive_target_iso14443a(PN532_I2C_BUS, &target, 100);
    return MfClassicErrorNone;
#else
    MfClassicError ret = MfClassicErrorNone;
    Iso14443_3aError error = Iso14443_3aErrorNone;

    do {
        uint8_t halt_cmd[2] = {MF_CLASSIC_CMD_HALT_MSB, MF_CLASSIC_CMD_HALT_LSB};
        bit_buffer_copy_bytes(instance->tx_plain_buffer, halt_cmd, sizeof(halt_cmd));
        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_plain_buffer);

        crypto1_encrypt(
            instance->crypto, NULL, instance->tx_plain_buffer, instance->tx_encrypted_buffer);

        error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            MF_CLASSIC_FWT_FC);
        if(error != Iso14443_3aErrorTimeout) {
            ret = mf_classic_process_error(error);
            break;
        }
        instance->auth_state = MfClassicAuthStateIdle;
        instance->iso14443_3a_poller->state = Iso14443_3aPollerStateIdle;
    } while(false);

    return ret;
#endif
}

MfClassicError mf_classic_poller_read_block(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicBlock* data) {
    furi_check(instance);
    furi_check(data);
#if defined(FURI_HAL_NFC_CHIP_PN532)
    size_t block_len = sizeof(MfClassicBlock);
    Pn532Error err = pn532_mifare_read_block(
        PN532_I2C_BUS, 1, block_num, data->data, &block_len);

    if(err == Pn532ErrorNone && block_len == sizeof(MfClassicBlock)) {
        return MfClassicErrorNone;
    } else {
        return MfClassicErrorProtocol;
    }
#else
    MfClassicError ret = MfClassicErrorNone;
    Iso14443_3aError error = Iso14443_3aErrorNone;

    do {
        uint8_t read_block_cmd[2] = {MF_CLASSIC_CMD_READ_BLOCK, block_num};
        bit_buffer_copy_bytes(instance->tx_plain_buffer, read_block_cmd, sizeof(read_block_cmd));
        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_plain_buffer);

        crypto1_encrypt(
            instance->crypto, NULL, instance->tx_plain_buffer, instance->tx_encrypted_buffer);

        error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            MF_CLASSIC_FWT_FC);
        if(error != Iso14443_3aErrorNone) {
            ret = mf_classic_process_error(error);
            break;
        }
        if(bit_buffer_get_size_bytes(instance->rx_encrypted_buffer) !=
           (sizeof(MfClassicBlock) + 2)) {
            ret = MfClassicErrorProtocol;
            break;
        }

        crypto1_decrypt(
            instance->crypto, instance->rx_encrypted_buffer, instance->rx_plain_buffer);

        if(!iso14443_crc_check(Iso14443CrcTypeA, instance->rx_plain_buffer)) {
            FURI_LOG_D(TAG, "CRC error");
            ret = MfClassicErrorProtocol;
            break;
        }

        iso14443_crc_trim(instance->rx_plain_buffer);
        bit_buffer_write_bytes(instance->rx_plain_buffer, data->data, sizeof(MfClassicBlock));
    } while(false);

    return ret;
#endif
}

MfClassicError mf_classic_poller_write_block(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicBlock* data) {
    furi_check(instance);
    furi_check(data);
#if defined(FURI_HAL_NFC_CHIP_PN532)
    Pn532Error err = pn532_mifare_classic_write_block(
        PN532_I2C_BUS, 1, block_num, data->data);

    if(err == Pn532ErrorNone) {
        return MfClassicErrorNone;
    } else {
        return MfClassicErrorProtocol;
    }
#else
    MfClassicError ret = MfClassicErrorNone;
    Iso14443_3aError error = Iso14443_3aErrorNone;

    do {
        uint8_t write_block_cmd[2] = {MF_CLASSIC_CMD_WRITE_BLOCK, block_num};
        bit_buffer_copy_bytes(instance->tx_plain_buffer, write_block_cmd, sizeof(write_block_cmd));
        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_plain_buffer);

        crypto1_encrypt(
            instance->crypto, NULL, instance->tx_plain_buffer, instance->tx_encrypted_buffer);

        error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            MF_CLASSIC_FWT_FC);
        if(error != Iso14443_3aErrorNone) {
            ret = mf_classic_process_error(error);
            break;
        }
        if(bit_buffer_get_size(instance->rx_encrypted_buffer) != 4) {
            ret = MfClassicErrorProtocol;
            break;
        }

        crypto1_decrypt(
            instance->crypto, instance->rx_encrypted_buffer, instance->rx_plain_buffer);

        if(bit_buffer_get_byte(instance->rx_plain_buffer, 0) != MF_CLASSIC_CMD_ACK) {
            FURI_LOG_D(TAG, "Not ACK received");
            ret = MfClassicErrorProtocol;
            break;
        }

        bit_buffer_copy_bytes(instance->tx_plain_buffer, data->data, sizeof(MfClassicBlock));
        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_plain_buffer);

        crypto1_encrypt(
            instance->crypto, NULL, instance->tx_plain_buffer, instance->tx_encrypted_buffer);

        error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            MF_CLASSIC_FWT_FC);
        if(error != Iso14443_3aErrorNone) {
            ret = mf_classic_process_error(error);
            break;
        }
        if(bit_buffer_get_size(instance->rx_encrypted_buffer) != 4) {
            ret = MfClassicErrorProtocol;
            break;
        }

        crypto1_decrypt(
            instance->crypto, instance->rx_encrypted_buffer, instance->rx_plain_buffer);

        if(bit_buffer_get_byte(instance->rx_plain_buffer, 0) != MF_CLASSIC_CMD_ACK) {
            FURI_LOG_D(TAG, "Not ACK received");
            ret = MfClassicErrorProtocol;
            break;
        }
    } while(false);

    return ret;
#endif
}

MfClassicError mf_classic_poller_value_cmd(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicValueCommand cmd,
    int32_t data) {
    furi_check(instance);
#if defined(FURI_HAL_NFC_CHIP_PN532)
    MfClassicBlock block;
    MfClassicError ret = mf_classic_poller_read_block(instance, block_num, &block);
    if(ret != MfClassicErrorNone) return ret;

    int32_t val = (int32_t)(block.data[0] | (block.data[1] << 8) | (block.data[2] << 16) | (block.data[3] << 24));
    if(cmd == MfClassicValueCommandDecrement) {
        val -= data;
    } else if(cmd == MfClassicValueCommandIncrement) {
        val += data;
    } else {
        val = data;
    }
    block.data[0] = (uint8_t)(val & 0xFF);
    block.data[1] = (uint8_t)((val >> 8) & 0xFF);
    block.data[2] = (uint8_t)((val >> 16) & 0xFF);
    block.data[3] = (uint8_t)((val >> 24) & 0xFF);
    block.data[4] = ~block.data[0];
    block.data[5] = ~block.data[1];
    block.data[6] = ~block.data[2];
    block.data[7] = ~block.data[3];
    block.data[8] = block.data[0];
    block.data[9] = block.data[1];
    block.data[10] = block.data[2];
    block.data[11] = block.data[3];

    return mf_classic_poller_write_block(instance, block_num, &block);
#else
    MfClassicError ret = MfClassicErrorNone;
    Iso14443_3aError error = Iso14443_3aErrorNone;

    do {
        uint8_t cmd_value = 0;
        if(cmd == MfClassicValueCommandDecrement) {
            cmd_value = MF_CLASSIC_CMD_VALUE_DEC;
        } else if(cmd == MfClassicValueCommandIncrement) {
            cmd_value = MF_CLASSIC_CMD_VALUE_INC;
        } else {
            cmd_value = MF_CLASSIC_CMD_VALUE_RESTORE;
        }
        uint8_t value_cmd[2] = {cmd_value, block_num};
        bit_buffer_copy_bytes(instance->tx_plain_buffer, value_cmd, sizeof(value_cmd));
        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_plain_buffer);

        crypto1_encrypt(
            instance->crypto, NULL, instance->tx_plain_buffer, instance->tx_encrypted_buffer);

        error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            MF_CLASSIC_FWT_FC);
        if(error != Iso14443_3aErrorNone) {
            ret = mf_classic_process_error(error);
            break;
        }
        if(bit_buffer_get_size(instance->rx_encrypted_buffer) != 4) {
            ret = MfClassicErrorProtocol;
            break;
        }

        crypto1_decrypt(
            instance->crypto, instance->rx_encrypted_buffer, instance->rx_plain_buffer);

        if(bit_buffer_get_byte(instance->rx_plain_buffer, 0) != MF_CLASSIC_CMD_ACK) {
            FURI_LOG_D(TAG, "Not ACK received");
            ret = MfClassicErrorProtocol;
            break;
        }

        bit_buffer_copy_bytes(instance->tx_plain_buffer, (uint8_t*)&data, sizeof(data));
        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_plain_buffer);

        crypto1_encrypt(
            instance->crypto, NULL, instance->tx_plain_buffer, instance->tx_encrypted_buffer);

        error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            MF_CLASSIC_FWT_FC);

        // Command processed if tag doesn't respond
        if(error != Iso14443_3aErrorTimeout) {
            ret = MfClassicErrorProtocol;
            break;
        }
        ret = MfClassicErrorNone;
    } while(false);

    return ret;
#endif
}

MfClassicError mf_classic_poller_value_transfer(MfClassicPoller* instance, uint8_t block_num) {
    furi_check(instance);
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(block_num);
    return MfClassicErrorNone;
#else
    MfClassicError ret = MfClassicErrorNone;
    Iso14443_3aError error = Iso14443_3aErrorNone;

    do {
        uint8_t transfer_cmd[2] = {MF_CLASSIC_CMD_VALUE_TRANSFER, block_num};
        bit_buffer_copy_bytes(instance->tx_plain_buffer, transfer_cmd, sizeof(transfer_cmd));
        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_plain_buffer);

        crypto1_encrypt(
            instance->crypto, NULL, instance->tx_plain_buffer, instance->tx_encrypted_buffer);

        error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            MF_CLASSIC_FWT_FC);
        if(error != Iso14443_3aErrorNone) {
            ret = mf_classic_process_error(error);
            break;
        }
        if(bit_buffer_get_size(instance->rx_encrypted_buffer) != 4) {
            ret = MfClassicErrorProtocol;
            break;
        }

        crypto1_decrypt(
            instance->crypto, instance->rx_encrypted_buffer, instance->rx_plain_buffer);

        if(bit_buffer_get_byte(instance->rx_plain_buffer, 0) != MF_CLASSIC_CMD_ACK) {
            FURI_LOG_D(TAG, "Not ACK received");
            ret = MfClassicErrorProtocol;
            break;
        }

    } while(false);

    return ret;
#endif
}

MfClassicError mf_classic_poller_send_standard_frame(
    MfClassicPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt_fc) {
    furi_check(instance);
    furi_check(tx_buffer);
    furi_check(rx_buffer);

    Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
        instance->iso14443_3a_poller, tx_buffer, rx_buffer, fwt_fc);

    return mf_classic_process_error(error);
}

MfClassicError mf_classic_poller_send_frame(
    MfClassicPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt_fc) {
    furi_check(instance);
    furi_check(tx_buffer);
    furi_check(rx_buffer);

    Iso14443_3aError error =
        iso14443_3a_poller_txrx(instance->iso14443_3a_poller, tx_buffer, rx_buffer, fwt_fc);

    return mf_classic_process_error(error);
}

MfClassicError mf_classic_poller_send_custom_parity_frame(
    MfClassicPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt_fc) {
    furi_check(instance);
    furi_check(tx_buffer);
    furi_check(rx_buffer);

    Iso14443_3aError error = iso14443_3a_poller_txrx_custom_parity(
        instance->iso14443_3a_poller, tx_buffer, rx_buffer, fwt_fc);

    return mf_classic_process_error(error);
}

MfClassicError mf_classic_poller_send_encrypted_frame(
    MfClassicPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt_fc) {
    furi_check(instance);
    furi_check(tx_buffer);
    furi_check(rx_buffer);

    MfClassicError ret = MfClassicErrorNone;
    do {
        crypto1_encrypt(instance->crypto, NULL, tx_buffer, instance->tx_encrypted_buffer);

        Iso14443_3aError error = iso14443_3a_poller_txrx_custom_parity(
            instance->iso14443_3a_poller,
            instance->tx_encrypted_buffer,
            instance->rx_encrypted_buffer,
            fwt_fc);
        if(error != Iso14443_3aErrorNone) {
            ret = mf_classic_process_error(error);
            break;
        }

        crypto1_decrypt(instance->crypto, instance->rx_encrypted_buffer, rx_buffer);
    } while(false);

    return ret;
}
