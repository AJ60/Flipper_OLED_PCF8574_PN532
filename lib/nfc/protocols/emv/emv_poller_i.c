#include "emv_poller_i.h"
#include "protocols/emv/emv.h"

#define TAG "EMVPoller"

// "Terminal" parameters, which could be requested by card
const PDOLValue pdol_term_info = {0x9F59, {0xC8, 0x80, 0x00}}; // Terminal transaction information
const PDOLValue pdol_term_type = {0x9F5A, {0x00}}; // Terminal transaction type
const PDOLValue pdol_merchant_type = {0x9F58, {0x01}}; // Merchant type indicator
const PDOLValue pdol_term_trans_qualifies = {
    0x9F66,
    {0x36, 0x00, 0x40, 0x00}}; // Terminal transaction qualifiers (EMV contactless & contact, online capable)
const PDOLValue pdol_addtnl_term_qualifies = {
    0x9F40,
    {0x79, 0x00, 0x40, 0x80}}; // Additional terminal qualifiers
const PDOLValue pdol_amount_authorise = {
    0x9F02,
    {0x00, 0x00, 0x00, 0x10, 0x00, 0x00}}; // Amount, authorised
const PDOLValue pdol_amount = {0x9F03, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // Amount
const PDOLValue pdol_country_code = {0x9F1A, {0x01, 0x24}}; // Terminal country code
const PDOLValue pdol_currency_code = {0x5F2A, {0x01, 0x24}}; // Transaction currency code
const PDOLValue pdol_term_verification = {
    0x95,
    {0x00, 0x00, 0x00, 0x00, 0x00}}; // Terminal verification results
const PDOLValue pdol_transaction_date = {0x9A, {0x19, 0x01, 0x01}}; // Transaction date
const PDOLValue pdol_transaction_type = {0x9C, {0x00}}; // Transaction type
const PDOLValue pdol_transaction_cert = {0x98, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}; // Transaction cert
const PDOLValue pdol_unpredict_number = {0x9F37, {0x82, 0x3D, 0xDE, 0x7A}}; // Unpredictable number
const PDOLValue pdol_term_type_emv = {0x9F35, {0x22}}; // Terminal Type (Attended, Online capable)
const PDOLValue pdol_term_capabilities = {0x9F33, {0xE0, 0xF8, 0xC8}}; // Terminal Capabilities
const PDOLValue pdol_ifd_serial = {0x9F1E, {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38}}; // IFD Serial
const PDOLValue pdol_trans_time = {0x9F21, {0x12, 0x00, 0x00}}; // Transaction Time (HHMMSS)
const PDOLValue pdol_app_version = {0x9F09, {0x00, 0x02}}; // Application Version
const PDOLValue pdol_vlp_support = {0x9F7A, {0x00}}; // VLP / Electronic Cash Terminal Support

const PDOLValue* const pdol_values[] = {
    &pdol_term_info,
    &pdol_term_type,
    &pdol_merchant_type,
    &pdol_term_trans_qualifies,
    &pdol_addtnl_term_qualifies,
    &pdol_amount_authorise,
    &pdol_amount,
    &pdol_country_code,
    &pdol_currency_code,
    &pdol_term_verification,
    &pdol_transaction_date,
    &pdol_transaction_type,
    &pdol_transaction_cert,
    &pdol_unpredict_number,
    &pdol_term_type_emv,
    &pdol_term_capabilities,
    &pdol_ifd_serial,
    &pdol_trans_time,
    &pdol_app_version,
    &pdol_vlp_support,
};

EmvError emv_process_error(Iso14443_4aError error) {
    switch(error) {
    case Iso14443_4aErrorNone:
        return EmvErrorNone;
    case Iso14443_4aErrorNotPresent:
        return EmvErrorNotPresent;
    case Iso14443_4aErrorTimeout:
        return EmvErrorTimeout;
    default:
        return EmvErrorProtocol;
    }
}

static void emv_trace(EmvPoller* instance, const char* message) {
    if(furi_log_get_level() == FuriLogLevelTrace) {
        FURI_LOG_T(TAG, "%s", message);

        printf("TX: ");
        size_t size = bit_buffer_get_size_bytes(instance->tx_buffer);
        for(size_t i = 0; i < size; i++) {
            printf("%02X ", bit_buffer_get_byte(instance->tx_buffer, i));
        }

        printf("\r\nRX: ");
        size = bit_buffer_get_size_bytes(instance->rx_buffer);
        for(size_t i = 0; i < size; i++) {
            printf("%02X ", bit_buffer_get_byte(instance->rx_buffer, i));
        }
        printf("\r\n");
    }
}

static bool
    emv_decode_tlv_tag(const uint8_t* buff, uint16_t tag, uint8_t tlen, EmvApplication* app) {
    uint8_t i = 0;
    bool success = false;

    switch(tag) {
    case EMV_TAG_LOG_FMT: {
        size_t copy_len = MIN(tlen, sizeof(app->log_fmt));
        memcpy(app->log_fmt, &buff[i], copy_len);
        app->log_fmt_len = copy_len;
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_LOG_FMT %X: len %zu", tag, copy_len);
        break;
    }
    case EMV_TAG_GPO_FMT1: {
        // skip AIP
        i += 2;
        if(tlen >= 2) {
            tlen -= 2;
            size_t copy_len = MIN(tlen, sizeof(app->afl.data));
            memcpy(app->afl.data, &buff[i], copy_len);
            app->afl.size = copy_len;
            success = true;
            FURI_LOG_T(TAG, "found EMV_TAG_GPO_FMT1 %X: ", tag);
        }
        break;
    }
    case EMV_TAG_AID: {
        size_t copy_len = MIN(tlen, sizeof(app->aid));
        app->aid_len = copy_len;
        memcpy(app->aid, &buff[i], copy_len);
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_AID %X: ", tag);
        for(size_t x = 0; x < copy_len; x++) {
            FURI_LOG_RAW_T("%02X ", app->aid[x]);
        }
        FURI_LOG_RAW_T("\r\n");
        break;
    }
    case EMV_TAG_PRIORITY:
        if(tlen >= 1) {
            memcpy(&app->priority, &buff[i], 1);
            success = true;
            FURI_LOG_T(TAG, "found EMV_TAG_APP_PRIORITY %X: %d", tag, app->priority);
        }
        break;
    case EMV_TAG_APPL_INTERCHANGE_PROFILE: {
        size_t copy_len = MIN(tlen, sizeof(app->application_interchange_profile));
        memcpy(app->application_interchange_profile, &buff[i], copy_len);
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_APPL_INTERCHANGE_PROFILE %x: ", tag);
        for(size_t x = 0; x < copy_len; x++) {
            FURI_LOG_RAW_T("%02X ", app->application_interchange_profile[x]);
        }
        FURI_LOG_RAW_T("\r\n");
        break;
    }
    case EMV_TAG_APPL_LABEL: {
        size_t copy_len = MIN(tlen, sizeof(app->application_label) - 1);
        memcpy(app->application_label, &buff[i], copy_len);
        app->application_label[copy_len] = '\0';
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_APPL_LABEL %x: %s", tag, app->application_label);
        break;
    }
    case EMV_TAG_APPL_NAME: {
        size_t copy_len = MIN(tlen, sizeof(app->application_name) - 1);
        memcpy(app->application_name, &buff[i], copy_len);
        app->application_name[copy_len] = '\0';
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_APPL_NAME %x: %s", tag, app->application_name);
        break;
    }
    case EMV_TAG_APPL_EFFECTIVE:
        if(tlen >= 3) {
            app->effective_year = buff[i];
            app->effective_month = buff[i + 1];
            app->effective_day = buff[i + 2];
            success = true;
            FURI_LOG_T(TAG, "found EMV_TAG_APPL_ISSUE %x:", tag);
        }
        break;
    case EMV_TAG_PDOL: {
        size_t copy_len = MIN(tlen, sizeof(app->pdol.data));
        memcpy(app->pdol.data, &buff[i], copy_len);
        app->pdol.size = copy_len;
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_PDOL %x (len=%zu)", tag, copy_len);
        break;
    }
    case EMV_TAG_AFL: {
        size_t copy_len = MIN(tlen, sizeof(app->afl.data));
        memcpy(app->afl.data, &buff[i], copy_len);
        app->afl.size = copy_len;
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_AFL %x (len=%zu)", tag, copy_len);
        break;
    }
    // Tracks data https://murdoch.is/papers/defcon20emvdecode.pdf
    case EMV_TAG_TRACK_1_EQUIV: {
        // Contain PAN and expire date
        char track_1_equiv[80];
        size_t copy_len = MIN(tlen, sizeof(track_1_equiv) - 1);
        memcpy(track_1_equiv, &buff[i], copy_len);
        track_1_equiv[copy_len] = '\0';
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_TRACK_1_EQUIV %x (len=%zu, redacted)", tag, copy_len);
        break;
    }
    case EMV_TAG_TRACK_2_DATA:
    case EMV_TAG_TRACK_2_EQUIV: {
        FURI_LOG_T(TAG, "found EMV_TAG_TRACK_2 %X", tag);
        // Robust nibble-based Track 2 parser: search for delimiter 0x0D ('D' / '=')
        int sep_nibble = -1;
        int total_nibbles = (int)tlen * 2;
        for(int n = 0; n < total_nibbles; n++) {
            uint8_t nibble = (n % 2 == 0) ? (buff[i + (n / 2)] >> 4) : (buff[i + (n / 2)] & 0x0F);
            if(nibble == 0x0D) {
                sep_nibble = n;
                break;
            }
        }

        if(sep_nibble > 0 && sep_nibble <= 19) {
            uint8_t pan_bytes = (sep_nibble + 1) / 2;
            size_t pan_copy_len = MIN((size_t)pan_bytes, sizeof(app->pan));
            memset(app->pan, 0, sizeof(app->pan));
            for(int n = 0; n < sep_nibble; n++) {
                uint8_t digit = (n % 2 == 0) ? (buff[i + (n / 2)] >> 4) : (buff[i + (n / 2)] & 0x0F);
                if(n % 2 == 0) {
                    app->pan[n / 2] = (digit << 4);
                } else {
                    app->pan[n / 2] |= (digit & 0x0F);
                }
            }
            if(sep_nibble % 2 != 0) {
                app->pan[sep_nibble / 2] |= 0x0F;
            }
            app->pan_len = pan_copy_len;

            // Extract expiry YYMM from 4 nibbles immediately after 'D'
            if(sep_nibble + 4 < total_nibbles) {
                uint8_t y1 = (buff[i + ((sep_nibble + 1) / 2)] >> (((sep_nibble + 1) % 2 == 0) ? 4 : 0)) & 0x0F;
                uint8_t y2 = (buff[i + ((sep_nibble + 2) / 2)] >> (((sep_nibble + 2) % 2 == 0) ? 4 : 0)) & 0x0F;
                uint8_t m1 = (buff[i + ((sep_nibble + 3) / 2)] >> (((sep_nibble + 3) % 2 == 0) ? 4 : 0)) & 0x0F;
                uint8_t m2 = (buff[i + ((sep_nibble + 4) / 2)] >> (((sep_nibble + 4) % 2 == 0) ? 4 : 0)) & 0x0F;
                app->exp_year = (y1 << 4) | y2;
                app->exp_month = (m1 << 4) | m2;
            }
        }
        FURI_LOG_T(TAG, "found EMV_TAG_TRACK_2 %X (pan_len=%d, redacted)", tag, (int)app->pan_len);
        success = true;
        break;
    }
    case EMV_TAG_CARDHOLDER_NAME: {
        size_t copy_len = MIN(tlen, sizeof(app->cardholder_name) - 1);
        memcpy(app->cardholder_name, &buff[i], copy_len);
        app->cardholder_name[copy_len] = '\0';

        // use space char as terminator
        for(size_t j = 0; j < copy_len; j++) {
            if(app->cardholder_name[j] == 0x20) {
                app->cardholder_name[j] = '\0';
                break;
            }
        }

        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_CARDHOLDER_NAME %x (redacted)", tag);
        break;
    }
    case EMV_TAG_PAN: {
        size_t copy_len = MIN(tlen, sizeof(app->pan));
        memcpy(app->pan, &buff[i], copy_len);
        app->pan_len = copy_len;
        success = true;
        FURI_LOG_T(TAG, "found EMV_TAG_PAN %x", tag);
        break;
    }
    case EMV_TAG_EXP_DATE:
        if(tlen >= 3) {
            app->exp_year = buff[i];
            app->exp_month = buff[i + 1];
            app->exp_day = buff[i + 2];
            success = true;
            FURI_LOG_T(TAG, "found EMV_TAG_EXP_DATE %x", tag);
        }
        break;
    case EMV_TAG_CURRENCY_CODE:
        if(tlen >= 2) {
            app->currency_code = (buff[i] << 8 | buff[i + 1]);
            success = true;
            FURI_LOG_T(TAG, "found EMV_TAG_CURRENCY_CODE %x", tag);
        }
        break;
    case EMV_TAG_COUNTRY_CODE:
        if(tlen >= 2) {
            app->country_code = (buff[i] << 8 | buff[i + 1]);
            success = true;
            FURI_LOG_T(TAG, "found EMV_TAG_COUNTRY_CODE %x", tag);
        }
        break;
    case EMV_TAG_LOG_ENTRY:
        if(tlen >= 2) {
            app->log_sfi = buff[i];
            app->log_records = buff[i + 1];
            success = true;
            FURI_LOG_T(
                TAG,
                "found EMV_TAG_LOG_ENTRY %x: sfi 0x%x, records %d",
                tag,
                app->log_sfi,
                app->log_records);
        }
        break;
    case EMV_TAG_LAST_ONLINE_ATC:
        if(tlen >= 2) {
            app->last_online_atc = (buff[i] << 8 | buff[i + 1]);
            success = true;
        }
        break;
    case EMV_TAG_ATC:
        if(tlen >= 2) {
            if(app->saving_trans_list && app->active_tr < COUNT_OF(app->trans))
                app->trans[app->active_tr].atc = (buff[i] << 8 | buff[i + 1]);
            else
                app->transaction_counter = (buff[i] << 8 | buff[i + 1]);
            success = true;
        }
        break;
    case EMV_TAG_LOG_AMOUNT:
        if(app->active_tr < COUNT_OF(app->trans)) {
            size_t copy_len = MIN(tlen, sizeof(app->trans[app->active_tr].amount));
            memcpy(&app->trans[app->active_tr].amount, &buff[i], copy_len);
            success = true;
        }
        break;
    case EMV_TAG_LOG_COUNTRY:
        if(tlen >= 2 && app->active_tr < COUNT_OF(app->trans)) {
            app->trans[app->active_tr].country = (buff[i] << 8 | buff[i + 1]);
            success = true;
        }
        break;
    case EMV_TAG_LOG_CURRENCY:
        if(tlen >= 2 && app->active_tr < COUNT_OF(app->trans)) {
            app->trans[app->active_tr].currency = (buff[i] << 8 | buff[i + 1]);
            success = true;
        }
        break;
    case EMV_TAG_LOG_DATE:
        if(app->active_tr < COUNT_OF(app->trans)) {
            size_t copy_len = MIN(tlen, sizeof(app->trans[app->active_tr].date));
            memcpy(&app->trans[app->active_tr].date, &buff[i], copy_len);
            success = true;
        }
        break;
    case EMV_TAG_LOG_TIME:
        if(app->active_tr < COUNT_OF(app->trans)) {
            size_t copy_len = MIN(tlen, sizeof(app->trans[app->active_tr].time));
            memcpy(&app->trans[app->active_tr].time, &buff[i], copy_len);
            success = true;
        }
        break;
    case EMV_TAG_PIN_TRY_COUNTER:
        if(tlen >= 1) {
            app->pin_try_counter = buff[i];
            success = true;
            FURI_LOG_T(TAG, "found EMV_TAG_PIN_TRY_COUNTER %x: %d", tag, app->pin_try_counter);
        }
        break;
    }
    return success;
}

static bool emv_response_error(const uint8_t* buff, uint16_t len) {
    uint8_t i = 0;
    uint8_t first_byte = 0;
    bool error = true;

    first_byte = buff[i];

    if((len == 2) && ((first_byte >> 4) == 6)) {
        switch(buff[i]) {
        case EMV_TAG_RESP_BUF_SIZE:
            FURI_LOG_T(TAG, " Wrong length. Read %02X bytes", buff[i + 1]);
            // Need to request SFI again with this length value
            return error;
        case EMV_TAG_RESP_BYTES_AVAILABLE:
            FURI_LOG_T(TAG, " Bytes available: %02X", buff[i + 1]);
            // Need to request one more time
            return error;

        default:
            FURI_LOG_T(TAG, " Error/warning code: %02X %02X", buff[i], buff[i + 1]);
            return error;
        }
    }
    return false;
}

static bool
    emv_parse_tag(const uint8_t* buff, uint16_t len, uint16_t* t, uint8_t* tl, uint8_t* off) {
    if(!buff || !t || !tl || !off) return false;
    uint8_t i = *off;
    uint16_t tag = 0;
    uint8_t first_byte = 0;
    uint8_t tlen = 0;

    if(i >= len) return false;
    if(emv_response_error(buff, len)) return false;

    first_byte = buff[i];

    if((first_byte & 31) == 31) { // 2-byte tag
        if(i + 1 >= len) return false;
        tag = buff[i] << 8 | buff[i + 1];
        i++;
        FURI_LOG_T(TAG, " 2-byte TLV EMV tag: %x", tag);
    } else {
        tag = buff[i];
        FURI_LOG_T(TAG, " 1-byte TLV EMV tag: %x", tag);
    }
    i++;
    if(i >= len) return false;
    tlen = buff[i];
    if((tlen & 128) == 128) { // long length value
        i++;
        if(i >= len) return false;
        tlen = buff[i];
        FURI_LOG_T(TAG, " 2-byte TLV length: %d", tlen);
    } else {
        FURI_LOG_T(TAG, " 1-byte TLV length: %d", tlen);
    }
    i++;

    *off = i;
    *t = tag;
    *tl = tlen;
    return true;
}

static bool emv_decode_tl(
    const uint8_t* buff,
    uint16_t len,
    const uint8_t* fmt,
    uint8_t fmt_len,
    EmvApplication* app) {
    uint8_t i = 0;
    uint8_t f = 0;
    uint16_t tag = 0;
    uint8_t tlen = 0;
    bool success = false;

    if(emv_response_error(buff, len)) return success;

    while(f < fmt_len && i < len) {
        success = emv_parse_tag(fmt, fmt_len, &tag, &tlen, &f);
        if(!success) return success;
        emv_decode_tlv_tag(&buff[i], tag, tlen, app);
        i += tlen;
    }
    success = true;
    return success;
}

static bool emv_decode_response_tlv_internal(
    const uint8_t* buff,
    uint8_t len,
    EmvApplication* app,
    uint8_t depth) {
    if(depth > 4) {
        FURI_LOG_W(TAG, "Exceeded maximum TLV nesting depth (%d)", depth);
        return false;
    }
    uint8_t i = 0;
    uint16_t tag = 0;
    uint8_t first_byte = 0;
    uint8_t tlen = 0;
    bool success = false;

    while(i < len) {
        // If remaining 2 bytes are APDU status word (e.g. 90 00, 62 XX, 63 XX), finish parsing
        if((i + 2 == len) && (buff[i] == 0x90 || (buff[i] >> 4) == 0x6)) {
            break;
        }
        first_byte = buff[i];

        success = emv_parse_tag(buff, len, &tag, &tlen, &i);
        if(!success) return success;

        if((first_byte & 32) == 32) { // "Constructed" -- contains more TLV data to parse
            FURI_LOG_T(TAG, "Constructed TLV %x", tag);
            if(i + tlen > len) {
                FURI_LOG_W(TAG, "Constructed TLV tag length exceeds buffer (%d + %d > %d)", i, tlen, len);
                return false;
            }
            if(!emv_decode_response_tlv_internal(&buff[i], tlen, app, depth + 1)) {
                FURI_LOG_T(TAG, "Failed to decode response for %x", tag);
            } else {
                success = true;
            }
        } else {
            if(i + tlen <= len) {
                if(emv_decode_tlv_tag(&buff[i], tag, tlen, app)) {
                    success = true;
                }
            }
        }
        i += tlen;
    }
    return success;
}

static bool emv_decode_response_tlv(const uint8_t* buff, uint8_t len, EmvApplication* app) {
    return emv_decode_response_tlv_internal(buff, len, app, 0);
}

static void emv_prepare_pdol(APDU* dest, APDU* src) {
    uint16_t tag = 0;
    uint8_t tlen = 0;
    uint8_t i = 0;
    while(i < src->size) {
        bool tag_found = false;
        if(!emv_parse_tag(src->data, src->size, &tag, &tlen, &i)) {
            FURI_LOG_T(TAG, "Parsing PDOL failed at 0x%x", i);
            dest->size = 0;
            return;
        }

        if(dest->size + tlen > sizeof(dest->data)) {
            FURI_LOG_W(TAG, "PDOL buffer full");
            break;
        }
        for(uint8_t j = 0; j < COUNT_OF(pdol_values); j++) {
            if(tag == pdol_values[j]->tag) {
                memcpy(dest->data + dest->size, pdol_values[j]->data, tlen);
                dest->size += tlen;
                tag_found = true;
                break;
            }
        }

        if(!tag_found) {
            // Unknown tag, fill zeros
            memset(dest->data + dest->size, 0, tlen);
            dest->size += tlen;
        }
    }
}

EmvError emv_poller_select_ppse(EmvPoller* instance) {
    EmvError error = EmvErrorNone;

    const uint8_t emv_select_ppse_cmd[] = {
        0x00, 0xA4, // SELECT ppse
        0x04, 0x00, // P1:By name, P2: empty
        0x0e, // Lc: Data length
        0x32, 0x50, 0x41, 0x59, 0x2e, 0x53, 0x59, // Data string:
        0x53, 0x2e, 0x44, 0x44, 0x46, 0x30, 0x31, // 2PAY.SYS.DDF01 (PPSE)
        0x00 // Le
    };

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    bit_buffer_copy_bytes(instance->tx_buffer, emv_select_ppse_cmd, sizeof(emv_select_ppse_cmd));
    do {
        FURI_LOG_D(TAG, "Send select PPSE");

        Iso14443_4aError iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
            instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);

        if(iso14443_4a_error != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "Failed select PPSE, error %d", iso14443_4a_error);
            error = emv_process_error(iso14443_4a_error);
            break;
        }

        emv_trace(instance, "Select PPSE answer:");

        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);

        if(!emv_decode_response_tlv(
               buff,
               bit_buffer_get_size_bytes(instance->rx_buffer),
               &instance->data->emv_application)) {
            error = EmvErrorProtocol;
            FURI_LOG_E(TAG, "Failed to parse application");
        }
    } while(false);

    return error;
}

EmvError emv_poller_select_application(EmvPoller* instance) {
    EmvError error = EmvErrorNone;

    const uint8_t emv_select_header[] = {
        0x00,
        0xA4, // SELECT application
        0x04,
        0x00 // P1:By name, P2:First or only occurence
    };

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    // Copy header
    bit_buffer_copy_bytes(instance->tx_buffer, emv_select_header, sizeof(emv_select_header));

    // Copy AID
    bit_buffer_append_byte(instance->tx_buffer, instance->data->emv_application.aid_len);
    bit_buffer_append_bytes(
        instance->tx_buffer,
        instance->data->emv_application.aid,
        instance->data->emv_application.aid_len);
    bit_buffer_append_byte(instance->tx_buffer, 0x00);

    do {
        FURI_LOG_D(TAG, "Start application");

        Iso14443_4aError iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
            instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);

        emv_trace(instance, "Start application answer:");

        if(iso14443_4a_error != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "Failed to read PAN or PDOL, error %d", iso14443_4a_error);
            error = emv_process_error(iso14443_4a_error);
            break;
        }

        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);

        if(!emv_decode_response_tlv(
               buff,
               bit_buffer_get_size_bytes(instance->rx_buffer),
               &instance->data->emv_application)) {
            error = EmvErrorProtocol;
            FURI_LOG_E(TAG, "Failed to parse application");
            break;
        }

    } while(false);

    return error;
}

EmvError emv_poller_get_processing_options(EmvPoller* instance) {
    EmvError error = EmvErrorNone;

    const uint8_t emv_gpo_header[] = {0x80, 0xA8, 0x00, 0x00};

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    // Copy header
    bit_buffer_copy_bytes(instance->tx_buffer, emv_gpo_header, sizeof(emv_gpo_header));

    // Prepare and copy pdol parameters
    APDU pdol_data = {0, {0}};
    emv_prepare_pdol(&pdol_data, &instance->data->emv_application.pdol);

    bit_buffer_append_byte(instance->tx_buffer, 0x02 + pdol_data.size);
    bit_buffer_append_byte(instance->tx_buffer, 0x83);
    bit_buffer_append_byte(instance->tx_buffer, pdol_data.size);

    bit_buffer_append_bytes(instance->tx_buffer, pdol_data.data, pdol_data.size);
    bit_buffer_append_byte(instance->tx_buffer, 0x00);

    do {
        FURI_LOG_D(TAG, "Get proccessing options");

        Iso14443_4aError iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
            instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);

        emv_trace(instance, "Get processing options answer:");

        if(iso14443_4a_error != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "Failed to get processing options, error %u", iso14443_4a_error);
            error = emv_process_error(iso14443_4a_error);
            break;
        }

        size_t rx_len = bit_buffer_get_size_bytes(instance->rx_buffer);
        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);

        // If card responded with 6C XX (Wrong length), re-issue with exact Le requested by card
        if(rx_len == 2 && buff[0] == EMV_TAG_RESP_BUF_SIZE) {
            uint8_t exact_len = buff[1];
            FURI_LOG_D(TAG, "GPO: card requested Le=0x%02X, reissuing", exact_len);
            size_t tx_size = bit_buffer_get_size_bytes(instance->tx_buffer);
            bit_buffer_set_byte(instance->tx_buffer, tx_size - 1, exact_len);
            bit_buffer_reset(instance->rx_buffer);
            iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
                instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);
            if(iso14443_4a_error != Iso14443_4aErrorNone) {
                FURI_LOG_E(TAG, "Failed to get processing options on retry, error %u", iso14443_4a_error);
                error = emv_process_error(iso14443_4a_error);
                break;
            }
            buff = bit_buffer_get_data(instance->rx_buffer);
            rx_len = bit_buffer_get_size_bytes(instance->rx_buffer);
        }

        if(!emv_decode_response_tlv(
               buff,
               rx_len,
               &instance->data->emv_application)) {
            error = EmvErrorProtocol;
            FURI_LOG_E(TAG, "Failed to parse processing options");
        }
    } while(false);

    return error;
}

EmvError emv_poller_read_sfi_record(EmvPoller* instance, uint8_t sfi, uint8_t record_num) {
    EmvError error = EmvErrorNone;
    FuriString* text = furi_string_alloc();

    uint8_t sfi_param = (sfi << 3) | (1 << 2);
    uint8_t emv_sfi_header[] = {
        0x00,
        0xB2, // READ RECORD
        record_num, // P1:record_number
        sfi_param, // P2:SFI
        0x00 // Le
    };

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    bit_buffer_copy_bytes(instance->tx_buffer, emv_sfi_header, sizeof(emv_sfi_header));

    do {
        Iso14443_4aError iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
            instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);

        furi_string_printf(text, "SFI 0x%X record %d:", sfi, record_num);
        emv_trace(instance, furi_string_get_cstr(text));

        if(iso14443_4a_error != Iso14443_4aErrorNone) {
            FURI_LOG_E(
                TAG,
                "Failed to read SFI 0x%X record %d, error %d",
                sfi,
                record_num,
                iso14443_4a_error);
            error = emv_process_error(iso14443_4a_error);
            break;
        }

        // If card responded with 6C XX (Wrong length), re-issue with exact Le requested by card
        size_t rx_len = bit_buffer_get_size_bytes(instance->rx_buffer);
        const uint8_t* rx_data = bit_buffer_get_data(instance->rx_buffer);
        if(rx_len == 2 && rx_data[0] == EMV_TAG_RESP_BUF_SIZE) {
            uint8_t exact_len = rx_data[1];
            FURI_LOG_D(TAG, "SFI 0x%X record %d: card requested Le=0x%02X, reissuing", sfi, record_num, exact_len);
            emv_sfi_header[4] = exact_len;
            bit_buffer_reset(instance->tx_buffer);
            bit_buffer_reset(instance->rx_buffer);
            bit_buffer_copy_bytes(instance->tx_buffer, emv_sfi_header, sizeof(emv_sfi_header));
            iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
                instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);
            if(iso14443_4a_error != Iso14443_4aErrorNone) {
                error = emv_process_error(iso14443_4a_error);
                break;
            }
        }
    } while(false);

    furi_string_free(text);

    return error;
}

EmvError emv_poller_read_afl(EmvPoller* instance, bool bruteforce_sfi, uint16_t* readed_mask) {
    EmvError error = EmvErrorNone;
    bool pan_fetched = (instance->data->emv_application.pan_len);
    bool cardholder_name_fetched = strlen(instance->data->emv_application.cardholder_name);

    if(!bruteforce_sfi) {
        // SEARCH PAN, RETURN WHEN FOUND
        APDU* afl = &instance->data->emv_application.afl;

        if(afl->size > 0) {
            FURI_LOG_D(TAG, "Search PAN in SFI");

            // Iterate through all files
            for(size_t i = 0; i < instance->data->emv_application.afl.size; i += 4) {
                uint8_t sfi = afl->data[i] >> 3;
                uint8_t record_start = afl->data[i + 1];
                uint8_t record_end = afl->data[i + 2];
                // Iterate through all records in file
                for(uint8_t record = record_start; record <= record_end; ++record) {
                    if((sfi >= 2) && (sfi <= 3) && (record <= 5))
                        FURI_BIT_SET(
                            *readed_mask,
                            record + ((sfi - 2) * 8));

                    error = emv_poller_read_sfi_record(instance, sfi, record);
                    if(error != EmvErrorNone) break;

                    if(!emv_decode_response_tlv(
                           bit_buffer_get_data(instance->rx_buffer),
                           bit_buffer_get_size_bytes(instance->rx_buffer),
                           &instance->data->emv_application)) {
                        error = EmvErrorProtocol;
                        FURI_LOG_T(TAG, "Failed to parse SFI 0x%X record %d", sfi, record);
                    }

                    if(instance->data->emv_application.pan_len) {
                        pan_fetched = true;
                        break;
                    } // Card number fetched
                }
                if(pan_fetched) break;
            }
        }

        // Fallback for RuPay / qSPARC / cards where PAN is stored in SFI 2 or omitted from AFL
        if(!pan_fetched) {
            FURI_LOG_D(TAG, "PAN not found in AFL, scanning SFIs 1-4 for PAN/Track2");
            for(uint8_t sfi = 1; sfi <= 4; sfi++) {
                for(uint8_t record = 1; record <= 5; record++) {
                    error = emv_poller_read_sfi_record(instance, sfi, record);
                    if(error != EmvErrorNone) break;

                    emv_decode_response_tlv(
                        bit_buffer_get_data(instance->rx_buffer),
                        bit_buffer_get_size_bytes(instance->rx_buffer),
                        &instance->data->emv_application);

                    if(instance->data->emv_application.pan_len) {
                        pan_fetched = true;
                        FURI_LOG_I(TAG, "PAN found in SFI 0x%X record %d", sfi, record);
                        break;
                    }
                }
                if(pan_fetched) break;
            }
        }
    } else { // BRUTFORCE FILES 2-3. SEARCH CARDHOLDER NAME
        FURI_LOG_T(TAG, "Bruteforce files 2-3");
        for(size_t sfi = 2; sfi <= 3; sfi++) {
            // Iterate through records 1-5 in file
            for(size_t record = 1; record <= 5; record++) {
                // Skip previously readed sfi
                if((*readed_mask >> (record + ((sfi - 2) * 8))) & (0b1)) continue;

                error = emv_poller_read_sfi_record(instance, sfi, record);
                if(error != EmvErrorNone) break;

                if(!emv_decode_response_tlv(
                       bit_buffer_get_data(instance->rx_buffer),
                       bit_buffer_get_size_bytes(instance->rx_buffer),
                       &instance->data->emv_application)) {
                    error = EmvErrorProtocol;
                    FURI_LOG_T(TAG, "Failed to parse SFI 0x%X record %d", sfi, record);
                }

                if(strlen(instance->data->emv_application.cardholder_name))
                    cardholder_name_fetched = true;
            }
        }
    }

    if((pan_fetched && (!bruteforce_sfi)) || (cardholder_name_fetched && bruteforce_sfi))
        return EmvErrorNone;
    else
        return error;
}

static EmvError emv_poller_req_get_data(EmvPoller* instance, uint16_t tag) {
    EmvError error = EmvErrorNone;

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    bit_buffer_append_byte(instance->tx_buffer, EMV_REQ_GET_DATA >> 8);
    bit_buffer_append_byte(instance->tx_buffer, EMV_REQ_GET_DATA & 0xFF);
    bit_buffer_append_byte(instance->tx_buffer, tag >> 8);
    bit_buffer_append_byte(instance->tx_buffer, tag & 0xFF);
    bit_buffer_append_byte(instance->tx_buffer, 0x00); //Length

    do {
        FURI_LOG_D(TAG, "Get data for tag 0x%x", tag);

        Iso14443_4aError iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
            instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);

        emv_trace(instance, "Get log data answer:");

        if(iso14443_4a_error != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "Failed to get data, error %u", iso14443_4a_error);
            error = emv_process_error(iso14443_4a_error);
            break;
        }

        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);

        if(!emv_decode_response_tlv(
               buff,
               bit_buffer_get_size_bytes(instance->rx_buffer),
               &instance->data->emv_application)) {
            error = EmvErrorProtocol;
            FURI_LOG_E(TAG, "Failed to parse get data");
        }
    } while(false);

    return error;
}

EmvError emv_poller_get_pin_try_counter(EmvPoller* instance) {
    return emv_poller_req_get_data(instance, EMV_TAG_PIN_TRY_COUNTER);
}

EmvError emv_poller_get_last_online_atc(EmvPoller* instance) {
    return emv_poller_req_get_data(instance, EMV_TAG_LAST_ONLINE_ATC);
}

static EmvError emv_poller_get_log_format(EmvPoller* instance) {
    return emv_poller_req_get_data(instance, EMV_TAG_LOG_FMT);
}

EmvError emv_poller_read_log_entry(EmvPoller* instance) {
    EmvError error = EmvErrorProtocol;

    if(!instance->data->emv_application.log_sfi) return error;
    uint8_t records = instance->data->emv_application.log_records;
    if(records == 0) {
        return error;
    }

    instance->data->emv_application.saving_trans_list = true;
    error = emv_poller_get_log_format(instance);
    if(error != EmvErrorNone) return error;

    FURI_LOG_D(TAG, "Read Transaction logs");

    uint8_t sfi = instance->data->emv_application.log_sfi;
    uint8_t record_start = 1;
    uint8_t record_end = records;
    // Iterate through all records in file
    for(uint8_t record = record_start; record <= record_end; ++record) {
        error = emv_poller_read_sfi_record(instance, sfi, record);
        if(error != EmvErrorNone) break;
        if(!emv_decode_tl(
               bit_buffer_get_data(instance->rx_buffer),
               bit_buffer_get_size_bytes(instance->rx_buffer),
               instance->data->emv_application.log_fmt,
               instance->data->emv_application.log_fmt_len,
               &instance->data->emv_application)) {
            error = EmvErrorProtocol;
            FURI_LOG_T(TAG, "Failed to parse log SFI 0x%X record %d", sfi, record);
            break;
        }

        instance->data->emv_application.active_tr++;
        if(instance->data->emv_application.active_tr >=
           COUNT_OF(instance->data->emv_application.trans)) {
            break;
        }
    }

    instance->data->emv_application.saving_trans_list = false;
    return error;
}
