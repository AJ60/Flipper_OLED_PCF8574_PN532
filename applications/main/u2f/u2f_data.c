#include <furi.h>
#include "u2f_data.h"
#include <furi_hal.h>
#include <storage/storage.h>
#include <furi_hal_random.h>
#include <flipper_format/flipper_format.h>

#define TAG "U2f"

#include <mbedtls/sha256.h>

// Fallback U2F attestation certificate (self-signed)
static const uint8_t u2f_fallback_cert[495] = {
    0x30, 0x82, 0x01, 0xeb, 0x30, 0x82, 0x01, 0x92, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x14, 0x6e,
    0x1d, 0x73, 0x08, 0xbd, 0x13, 0x70, 0xac, 0x35, 0x7e, 0xb8, 0x3e, 0x54, 0x9f, 0x60, 0x3e, 0x7a,
    0x28, 0x55, 0x0a, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x30,
    0x76, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x13,
    0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66, 0x6f, 0x72,
    0x6e, 0x69, 0x61, 0x31, 0x16, 0x30, 0x14, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0c, 0x0d, 0x4d, 0x6f,
    0x75, 0x6e, 0x74, 0x61, 0x69, 0x6e, 0x20, 0x56, 0x69, 0x65, 0x77, 0x31, 0x14, 0x30, 0x12, 0x06,
    0x03, 0x55, 0x04, 0x0a, 0x0c, 0x0b, 0x44, 0x49, 0x59, 0x20, 0x46, 0x6c, 0x69, 0x70, 0x70, 0x65,
    0x72, 0x31, 0x24, 0x30, 0x22, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x1b, 0x44, 0x49, 0x59, 0x20,
    0x46, 0x6c, 0x69, 0x70, 0x70, 0x65, 0x72, 0x20, 0x55, 0x32, 0x46, 0x20, 0x41, 0x74, 0x74, 0x65,
    0x73, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x30, 0x1e, 0x17, 0x0d, 0x32, 0x36, 0x30, 0x36, 0x31,
    0x39, 0x31, 0x37, 0x32, 0x37, 0x35, 0x36, 0x5a, 0x17, 0x0d, 0x34, 0x36, 0x30, 0x36, 0x31, 0x35,
    0x31, 0x37, 0x32, 0x37, 0x35, 0x36, 0x5a, 0x30, 0x76, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
    0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c,
    0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66, 0x6f, 0x72, 0x6e, 0x69, 0x61, 0x31, 0x16, 0x30, 0x14, 0x06,
    0x03, 0x55, 0x04, 0x07, 0x0c, 0x0d, 0x4d, 0x6f, 0x75, 0x6e, 0x74, 0x61, 0x69, 0x6e, 0x20, 0x56,
    0x69, 0x65, 0x77, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x0b, 0x44, 0x49,
    0x59, 0x20, 0x46, 0x6c, 0x69, 0x70, 0x70, 0x65, 0x72, 0x31, 0x24, 0x30, 0x22, 0x06, 0x03, 0x55,
    0x04, 0x03, 0x0c, 0x1b, 0x44, 0x49, 0x59, 0x20, 0x46, 0x6c, 0x69, 0x70, 0x70, 0x65, 0x72, 0x20,
    0x55, 0x32, 0x46, 0x20, 0x41, 0x74, 0x74, 0x65, 0x73, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x30,
    0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0xed, 0xdb, 0xd7, 0x79, 0xb5, 0xc2,
    0x40, 0x50, 0x3a, 0x85, 0x76, 0x0b, 0x69, 0x31, 0xf7, 0xed, 0x87, 0xa6, 0x3f, 0x80, 0x55, 0x26,
    0xff, 0xad, 0x6a, 0xb7, 0xc5, 0x30, 0x40, 0xf2, 0x99, 0x29, 0x6b, 0x03, 0x0a, 0x5a, 0xdf, 0xfd,
    0x91, 0x43, 0x75, 0xa7, 0x4d, 0xe5, 0x5e, 0x52, 0x3c, 0x8f, 0xc2, 0x42, 0xef, 0x6e, 0x5f, 0x80,
    0x11, 0x60, 0x91, 0xbc, 0x55, 0xdd, 0xa8, 0x4f, 0xdc, 0x4a, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x47, 0x00, 0x30, 0x44, 0x02, 0x20, 0x2a, 0xe4, 0x83,
    0x6a, 0xc5, 0x32, 0x26, 0x8c, 0x14, 0xa2, 0x44, 0x45, 0x1f, 0x37, 0xd1, 0x51, 0xe7, 0x01, 0x8c,
    0x15, 0x9b, 0x1f, 0xfc, 0x75, 0xdc, 0xcc, 0x1b, 0x70, 0x84, 0x64, 0x71, 0x99, 0x02, 0x20, 0x6a,
    0xd5, 0xe8, 0x0e, 0x31, 0x21, 0x27, 0xc1, 0x24, 0xac, 0x60, 0x3f, 0xe7, 0x7d, 0x08, 0x85, 0x20,
    0x09, 0x39, 0x74, 0x68, 0x1e, 0x48, 0xb4, 0x7d, 0xe2, 0xf5, 0xcf, 0xe5, 0x27, 0x84, 0x94,
};

// Fallback U2F attestation private key (32 bytes)
static const uint8_t u2f_fallback_key[32] = {0xbc, 0x6e, 0x81, 0xa3, 0xae, 0x3e, 0xb6, 0xa5,
                                             0xe6, 0xbb, 0x26, 0x70, 0x0e, 0x20, 0x1b, 0xee,
                                             0x98, 0xa7, 0x6f, 0xf6, 0x81, 0x82, 0xc2, 0x0a,
                                             0xda, 0xda, 0xa8, 0xae, 0xf7, 0xe7, 0x2b, 0x14};

static void u2f_data_derive_key(uint8_t* key_out) {
    const uint8_t* uid = furi_hal_version_uid();
    size_t uid_size = furi_hal_version_uid_size();
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);
    mbedtls_sha256_update(&sha_ctx, uid, uid_size);
    const uint8_t salt[] = "DIY_Flipper_Zero_U2F_Salt";
    mbedtls_sha256_update(&sha_ctx, salt, sizeof(salt));
    mbedtls_sha256_finish(&sha_ctx, key_out);
    mbedtls_sha256_free(&sha_ctx);
}

static bool u2f_data_sw_encrypt(
    const uint8_t* plaintext,
    uint8_t* ciphertext,
    size_t size,
    const uint8_t* iv) {
    uint8_t key[32];
    u2f_data_derive_key(key);
    if(!furi_hal_crypto_load_key(key, iv)) {
        return false;
    }
    bool result = furi_hal_crypto_encrypt(plaintext, ciphertext, size);
    furi_hal_crypto_unload_key();
    return result;
}

static bool u2f_data_sw_decrypt(
    const uint8_t* ciphertext,
    uint8_t* plaintext,
    size_t size,
    const uint8_t* iv) {
    uint8_t key[32];
    u2f_data_derive_key(key);
    if(!furi_hal_crypto_load_key(key, iv)) {
        return false;
    }
    bool result = furi_hal_crypto_decrypt(ciphertext, plaintext, size);
    furi_hal_crypto_unload_key();
    return result;
}

#define U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_FACTORY 2
#define U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE  FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT

#define U2F_CERT_STOCK 0 // Stock certificate, private key is encrypted with factory key
#define U2F_CERT_USER  1 // User certificate, private key is encrypted with unique key
#define U2F_CERT_USER_UNENCRYPTED \
    2 // Unencrypted user certificate, will be encrypted after first load

#define U2F_CERT_KEY_FILE_TYPE "Flipper U2F Certificate Key File"
#define U2F_CERT_KEY_VERSION   1

#define U2F_DEVICE_KEY_FILE_TYPE "Flipper U2F Device Key File"
#define U2F_DEVICE_KEY_VERSION   1

#define U2F_COUNTER_FILE_TYPE   "Flipper U2F Counter File"
#define U2F_COUNTER_VERSION     2
#define U2F_COUNTER_VERSION_OLD 1

#define U2F_COUNTER_CONTROL_VAL 0xAA5500FF

typedef struct {
    uint32_t counter;
    uint8_t random_salt[24];
    uint32_t control;
} FURI_PACKED U2fCounterData;

bool u2f_data_check(bool cert_only) {
    bool state = false;
    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(fs_api);

    do {
        if(!storage_file_open(file, U2F_CERT_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        storage_file_close(file);
        if(!storage_file_open(file, U2F_CERT_KEY_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        if(cert_only) {
            state = true;
            break;
        }
        storage_file_close(file);
        if(!storage_file_open(file, U2F_KEY_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        storage_file_close(file);
        if(!storage_file_open(file, U2F_CNT_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        state = true;
    } while(0);

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);

    return state;
}

bool u2f_data_cert_check(void) {
    bool state = false;
    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(fs_api);
    uint8_t file_buf[8];

    if(storage_file_open(file, U2F_CERT_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        do {
            // Read header to check certificate size
            size_t file_size = storage_file_size(file);
            size_t len_cur = storage_file_read(file, file_buf, 4);
            if(len_cur != 4) break;

            if(file_buf[0] != 0x30) {
                FURI_LOG_E(TAG, "Wrong certificate header");
                break;
            }

            size_t temp_len = ((file_buf[2] << 8) | (file_buf[3])) + 4;
            if(temp_len != file_size) {
                FURI_LOG_E(TAG, "Wrong certificate length");
                break;
            }
            state = true;
        } while(0);
    } else {
        FURI_LOG_I(TAG, "U2F cert file missing, using fallback cert");
        state = true;
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);

    return state;
}

uint32_t u2f_data_cert_load(uint8_t* cert) {
    furi_assert(cert);

    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(fs_api);
    uint32_t file_size = 0;
    uint32_t len_cur = 0;

    if(storage_file_open(file, U2F_CERT_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        file_size = storage_file_size(file);
        len_cur = storage_file_read(file, cert, file_size);
        if(len_cur != file_size) len_cur = 0;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(len_cur == 0) {
        FURI_LOG_I(TAG, "Loading fallback certificate");
        memcpy(cert, u2f_fallback_cert, sizeof(u2f_fallback_cert));
        len_cur = sizeof(u2f_fallback_cert);
    }

    return len_cur;
}

static bool u2f_data_cert_key_encrypt(uint8_t* cert_key) {
    furi_assert(cert_key);

    bool state = false;
    uint8_t iv[16];
    uint8_t key[48];
    uint32_t cert_type = U2F_CERT_USER;

    FURI_LOG_I(TAG, "Encrypting user cert key");

    // Generate random IV
    furi_hal_random_fill_buf(iv, 16);

    if(!furi_hal_crypto_enclave_load_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE, iv)) {
        FURI_LOG_W(TAG, "Enclave load key failed, using SW encryption");
        if(!u2f_data_sw_encrypt(cert_key, key, 32, iv)) {
            FURI_LOG_E(TAG, "SW encryption failed");
            return false;
        }
    } else {
        if(!furi_hal_crypto_encrypt(cert_key, key, 32)) {
            FURI_LOG_E(TAG, "Encryption failed");
            furi_hal_crypto_enclave_unload_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);
            return false;
        }
        furi_hal_crypto_enclave_unload_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(flipper_format, U2F_CERT_KEY_FILE)) {
        do {
            if(!flipper_format_write_header_cstr(
                   flipper_format, U2F_CERT_KEY_FILE_TYPE, U2F_CERT_KEY_VERSION))
                break;
            if(!flipper_format_write_uint32(flipper_format, "Type", &cert_type, 1)) break;
            if(!flipper_format_write_hex(flipper_format, "IV", iv, 16)) break;
            if(!flipper_format_write_hex(flipper_format, "Data", key, 48)) break;
            state = true;
        } while(0);
    }

    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);

    return state;
}

bool u2f_data_cert_key_load(uint8_t* cert_key) {
    furi_assert(cert_key);

    bool state = false;
    uint8_t iv[16];
    uint8_t key[48];
    uint32_t cert_type = 0;
    uint8_t key_slot = 0;
    uint32_t version = 0;

    // Check if unique key exists in secure enclave and generate it if missing (proceed even if false)
    furi_hal_crypto_enclave_ensure_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);

    FuriString* filetype;
    filetype = furi_string_alloc();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_existing(flipper_format, U2F_CERT_KEY_FILE)) {
        do {
            if(!flipper_format_read_header(flipper_format, filetype, &version)) {
                FURI_LOG_E(TAG, "Missing or incorrect header");
                break;
            }

            if(strcmp(furi_string_get_cstr(filetype), U2F_CERT_KEY_FILE_TYPE) != 0 ||
               version != U2F_CERT_KEY_VERSION) {
                FURI_LOG_E(TAG, "Type or version mismatch");
                break;
            }

            if(!flipper_format_read_uint32(flipper_format, "Type", &cert_type, 1)) {
                FURI_LOG_E(TAG, "Missing cert type");
                break;
            }

            if(cert_type == U2F_CERT_STOCK) {
                key_slot = U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_FACTORY;
            } else if(cert_type == U2F_CERT_USER) {
                key_slot = U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE;
            } else if(cert_type == U2F_CERT_USER_UNENCRYPTED) {
                key_slot = 0;
            } else {
                FURI_LOG_E(TAG, "Unknown cert type");
                break;
            }
            if(key_slot != 0) {
                if(!flipper_format_read_hex(flipper_format, "IV", iv, 16)) {
                    FURI_LOG_E(TAG, "Missing IV");
                    break;
                }

                if(!flipper_format_read_hex(flipper_format, "Data", key, 48)) {
                    FURI_LOG_E(TAG, "Missing data");
                    break;
                }

                if(!furi_hal_crypto_enclave_load_key(key_slot, iv)) {
                    FURI_LOG_W(TAG, "DIY fallback: decrypting cert key with SW key");
                    memset(cert_key, 0, 32);
                    if(u2f_data_sw_decrypt(key, cert_key, 32, iv)) {
                        state = true;
                    } else {
                        FURI_LOG_E(TAG, "SW decryption of cert key failed");
                    }
                    break;
                }
                memset(cert_key, 0, 32);

                if(!furi_hal_crypto_decrypt(key, cert_key, 32)) {
                    memset(cert_key, 0, 32);
                    FURI_LOG_E(TAG, "Decryption failed");
                    break;
                }
                furi_hal_crypto_enclave_unload_key(key_slot);
            } else {
                if(!flipper_format_read_hex(flipper_format, "Data", cert_key, 32)) {
                    FURI_LOG_E(TAG, "Missing data");
                    break;
                }
            }
            state = true;
        } while(0);
    } else {
        FURI_LOG_I(TAG, "Cert key file missing, loading fallback key");
        memcpy(cert_key, u2f_fallback_key, sizeof(u2f_fallback_key));
        state = true;
    }

    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(filetype);

    if(cert_type == U2F_CERT_USER_UNENCRYPTED) {
        return u2f_data_cert_key_encrypt(cert_key);
    }

    return state;
}

bool u2f_data_key_load(uint8_t* device_key) {
    furi_assert(device_key);

    bool state = false;
    uint8_t iv[16];
    uint8_t key[48];
    uint32_t version = 0;

    FuriString* filetype;
    filetype = furi_string_alloc();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_existing(flipper_format, U2F_KEY_FILE)) {
        do {
            if(!flipper_format_read_header(flipper_format, filetype, &version)) {
                FURI_LOG_E(TAG, "Missing or incorrect header");
                break;
            }
            if(strcmp(furi_string_get_cstr(filetype), U2F_DEVICE_KEY_FILE_TYPE) != 0 ||
               version != U2F_DEVICE_KEY_VERSION) {
                FURI_LOG_E(TAG, "Type or version mismatch");
                break;
            }
            if(!flipper_format_read_hex(flipper_format, "IV", iv, 16)) {
                FURI_LOG_E(TAG, "Missing IV");
                break;
            }
            if(!flipper_format_read_hex(flipper_format, "Data", key, 48)) {
                FURI_LOG_E(TAG, "Missing data");
                break;
            }
            if(!furi_hal_crypto_enclave_load_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE, iv)) {
                FURI_LOG_W(TAG, "DIY fallback: decrypting device key with SW key");
                memset(device_key, 0, 32);
                if(u2f_data_sw_decrypt(key, device_key, 32, iv)) {
                    state = true;
                } else {
                    FURI_LOG_E(TAG, "SW decryption of device key failed");
                }
                break;
            }
            memset(device_key, 0, 32);
            if(!furi_hal_crypto_decrypt(key, device_key, 32)) {
                memset(device_key, 0, 32);
                FURI_LOG_E(TAG, "Decryption failed");
                break;
            }
            furi_hal_crypto_enclave_unload_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);
            state = true;
        } while(0);
    }
    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(filetype);
    return state;
}

bool u2f_data_key_generate(uint8_t* device_key) {
    furi_assert(device_key);

    bool state = false;
    uint8_t iv[16];
    uint8_t key[32];
    uint8_t key_encrypted[48];

    // Generate random IV and key
    furi_hal_random_fill_buf(iv, 16);
    furi_hal_random_fill_buf(key, 32);

    if(!furi_hal_crypto_enclave_load_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE, iv)) {
        FURI_LOG_W(TAG, "Enclave load key failed, using SW encryption for device key");
        if(!u2f_data_sw_encrypt(key, key_encrypted, 32, iv)) {
            FURI_LOG_E(TAG, "SW encryption failed");
            return false;
        }
    } else {
        if(!furi_hal_crypto_encrypt(key, key_encrypted, 32)) {
            FURI_LOG_E(TAG, "Encryption failed");
            furi_hal_crypto_enclave_unload_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);
            return false;
        }
        furi_hal_crypto_enclave_unload_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(flipper_format, U2F_KEY_FILE)) {
        do {
            if(!flipper_format_write_header_cstr(
                   flipper_format, U2F_DEVICE_KEY_FILE_TYPE, U2F_DEVICE_KEY_VERSION))
                break;
            if(!flipper_format_write_hex(flipper_format, "IV", iv, 16)) break;
            if(!flipper_format_write_hex(flipper_format, "Data", key_encrypted, 48)) break;
            state = true;
            memcpy(device_key, key, 32);
        } while(0);
    }

    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);

    return state;
}

bool u2f_data_cnt_read(uint32_t* cnt_val) {
    furi_assert(cnt_val);

    bool state = false;
    bool old_counter = false;
    uint8_t iv[16];
    U2fCounterData cnt;
    uint8_t cnt_encr[48];
    uint32_t version = 0;

    FuriString* filetype;
    filetype = furi_string_alloc();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_existing(flipper_format, U2F_CNT_FILE)) {
        do {
            if(!flipper_format_read_header(flipper_format, filetype, &version)) {
                FURI_LOG_E(TAG, "Missing or incorrect header");
                break;
            }
            if(strcmp(furi_string_get_cstr(filetype), U2F_COUNTER_FILE_TYPE) != 0) {
                FURI_LOG_E(TAG, "Type mismatch");
                break;
            }
            if(version == U2F_COUNTER_VERSION_OLD) {
                // Counter is from previous U2F app version with endianness bug
                FURI_LOG_W(TAG, "Counter from old version");
                old_counter = true;
            } else if(version != U2F_COUNTER_VERSION) {
                FURI_LOG_E(TAG, "Version mismatch");
                break;
            }
            if(!flipper_format_read_hex(flipper_format, "IV", iv, 16)) {
                FURI_LOG_E(TAG, "Missing IV");
                break;
            }
            if(!flipper_format_read_hex(flipper_format, "Data", cnt_encr, 48)) {
                FURI_LOG_E(TAG, "Missing data");
                break;
            }
            if(!furi_hal_crypto_enclave_load_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE, iv)) {
                FURI_LOG_W(TAG, "DIY fallback: decrypting counter with SW key");
                memset(&cnt, 0, sizeof(U2fCounterData));
                if(u2f_data_sw_decrypt(cnt_encr, (uint8_t*)&cnt, sizeof(U2fCounterData), iv)) {
                    if(cnt.control == U2F_COUNTER_CONTROL_VAL) {
                        *cnt_val = cnt.counter;
                        state = true;
                    }
                } else {
                    FURI_LOG_E(TAG, "SW decryption of counter failed");
                }
                break;
            }
            memset(&cnt, 0, sizeof(U2fCounterData));
            if(!furi_hal_crypto_decrypt(cnt_encr, (uint8_t*)&cnt, sizeof(U2fCounterData))) {
                memset(&cnt, 0, sizeof(U2fCounterData));
                FURI_LOG_E(TAG, "Decryption failed");
                break;
            }
            furi_hal_crypto_enclave_unload_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);
            if(cnt.control == U2F_COUNTER_CONTROL_VAL) {
                *cnt_val = cnt.counter;
                state = true;
            }
        } while(0);
    }
    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(filetype);

    if(old_counter && state) {
        // Change counter endianness and rewrite counter file
        *cnt_val = __REV(cnt.counter);
        state = u2f_data_cnt_write(*cnt_val);
    }

    return state;
}

bool u2f_data_cnt_write(uint32_t cnt_val) {
    bool state = false;
    uint8_t iv[16];
    U2fCounterData cnt;
    uint8_t cnt_encr[48];

    // Generate random IV and key
    furi_hal_random_fill_buf(iv, 16);
    furi_hal_random_fill_buf(cnt.random_salt, 24);
    cnt.control = U2F_COUNTER_CONTROL_VAL;
    cnt.counter = cnt_val;

    if(!furi_hal_crypto_enclave_load_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE, iv)) {
        FURI_LOG_W(TAG, "Enclave load key failed, using SW encryption for counter");
        if(!u2f_data_sw_encrypt((uint8_t*)&cnt, cnt_encr, sizeof(U2fCounterData), iv)) {
            FURI_LOG_E(TAG, "SW encryption failed");
            return false;
        }
    } else {
        if(!furi_hal_crypto_encrypt((uint8_t*)&cnt, cnt_encr, sizeof(U2fCounterData))) {
            FURI_LOG_E(TAG, "Encryption failed");
            furi_hal_crypto_enclave_unload_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);
            return false;
        }
        furi_hal_crypto_enclave_unload_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE);
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(flipper_format, U2F_CNT_FILE)) {
        do {
            if(!flipper_format_write_header_cstr(
                   flipper_format, U2F_COUNTER_FILE_TYPE, U2F_COUNTER_VERSION))
                break;
            if(!flipper_format_write_hex(flipper_format, "IV", iv, 16)) break;
            if(!flipper_format_write_hex(flipper_format, "Data", cnt_encr, 48)) break;
            state = true;
        } while(0);
    }

    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);

    return state;
}
