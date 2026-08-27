/**
 * @file pn532.h
 * @brief NXP PN532 NFC Controller I2C Driver
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <furi_hal_i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "pn532_reg.h"

#define PN532_I2C_ADDR_7BIT PN532_I2C_ADDRESS
#define PN532_I2C_ADDR_8BIT PN532_I2C_ADDRESS_8BIT

/* Baud rates / Modulation types for InListPassiveTarget */
#define PN532_BAUD_ISO14443A  (0x00) // 106 kbps type A (ISO/IEC14443 Type A)
#define PN532_BAUD_FELICA_212 (0x01) // 212 kbps (FeliCa)
#define PN532_BAUD_FELICA_424 (0x02) // 424 kbps (FeliCa)
#define PN532_BAUD_ISO14443B  (0x03) // 106 kbps type B (ISO/IEC14443-3B)
#define PN532_BAUD_JEWEL      (0x04) // 106 kbps Innovision Jewel tag

/* Mifare Commands */
#define PN532_MIFARE_CMD_AUTH_A       (0x60)
#define PN532_MIFARE_CMD_AUTH_B       (0x61)
#define PN532_MIFARE_CMD_READ         (0x30)
#define PN532_MIFARE_CMD_WRITE        (0xA0)
#define PN532_MIFARE_CMD_TRANSFER     (0xB0)
#define PN532_MIFARE_CMD_DECREMENT    (0xC0)
#define PN532_MIFARE_CMD_INCREMENT    (0xC1)
#define PN532_MIFARE_CMD_RESTORE      (0xC2)
#define PN532_MIFARE_ULTRALIGHT_WRITE (0xA2)

/* Errors */
typedef enum {
    Pn532ErrorNone = 0,
    Pn532ErrorTimeout,
    Pn532ErrorBus,
    Pn532ErrorAckMismatch,
    Pn532ErrorChecksum,
    Pn532ErrorBufferOverflow,
    Pn532ErrorInternal,
} Pn532Error;

/** Target structure for detected ISO14443A card */
typedef struct {
    uint8_t tg;
    uint16_t sens_res; // ATQA
    uint8_t sel_res; // SAK
    uint8_t uid_len;
    uint8_t uid[10];
    uint8_t ats_len;
    uint8_t ats[32];
} Pn532TargetIso14443a;

/**
 * @brief Initialize communication with PN532 on the given I2C handle.
 */
Pn532Error pn532_init(const FuriHalI2cBusHandle* handle);

/**
 * @brief Wake up the PN532 from low power sleep mode.
 */
Pn532Error pn532_wake_up(const FuriHalI2cBusHandle* handle);

/**
 * @brief Get firmware version of PN532.
 * @param[out] ic IC type (should be 0x32 for PN532)
 * @param[out] ver Version byte (should be 1)
 * @param[out] rev Revision byte (should be 6)
 * @param[out] support Feature support mask
 */
Pn532Error pn532_get_firmware_version(
    const FuriHalI2cBusHandle* handle,
    uint8_t* ic,
    uint8_t* ver,
    uint8_t* rev,
    uint8_t* support);

/**
 * @brief Configure SAM (Security Access Module).
 * Normal mode: mode=0x01, timeout=0x14 (1 sec), use_irq=true
 */
Pn532Error pn532_sam_config(
    const FuriHalI2cBusHandle* handle,
    uint8_t mode,
    uint8_t timeout,
    bool use_irq);

/**
 * @brief Set RF Field status (On/Off).
 */
Pn532Error pn532_set_rf_field(const FuriHalI2cBusHandle* handle, bool state);

/**
 * @brief Configure maximum retries for passive target listing.
 * @param MxRtyPassiveActivation 0xFF for infinite, 0x00-0xFE for count
 */
Pn532Error
    pn532_set_passive_activation_retries(const FuriHalI2cBusHandle* handle, uint8_t max_retries);

/**
 * @brief Poll for passive ISO14443A card.
 */
Pn532Error pn532_in_list_passive_target_iso14443a(
    const FuriHalI2cBusHandle* handle,
    Pn532TargetIso14443a* target,
    uint32_t timeout_ms);

/** Target structure for detected ISO14443B card */
typedef struct {
    uint8_t tg;
    uint8_t atqb[12];
    uint8_t app_data[4];
    uint8_t protocol_info[3];
    uint8_t uid_len;
    uint8_t uid[10];
    uint8_t ats_len;
    uint8_t ats[32];
} Pn532TargetIso14443b;

/**
 * @brief Poll for passive ISO14443B card.
 */
Pn532Error pn532_in_list_passive_target_iso14443b(
    const FuriHalI2cBusHandle* handle,
    Pn532TargetIso14443b* target,
    uint32_t timeout_ms);

/**
 * @brief Exchange data with active target using host-controlled timing.
 * Uses InCommunicateThru command for raw frame TX/RX with custom FDT.
 */
Pn532Error pn532_in_communicate_thru(
    const FuriHalI2cBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_len,
    uint8_t* rx_data,
    size_t* rx_len,
    uint32_t timeout_ms);

/**
 * @brief Exchange data with active target.
 */
Pn532Error pn532_in_data_exchange(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    const uint8_t* tx_data,
    size_t tx_len,
    uint8_t* rx_data,
    size_t* rx_len,
    uint32_t timeout_ms);

/**
 * @brief Authenticate a Mifare Classic sector.
 */
Pn532Error pn532_mifare_classic_auth(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    uint8_t auth_cmd, // 0x60 (Key A) or 0x61 (Key B)
    uint8_t block,
    const uint8_t* key,
    const uint8_t* uid,
    uint8_t uid_len);

/**
 * @brief Read a 16-byte block from Mifare Classic or 4-byte page from Ultralight.
 */
Pn532Error pn532_mifare_read_block(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    uint8_t block,
    uint8_t* block_data,
    size_t* block_len);

/**
 * @brief Write a 16-byte block to Mifare Classic.
 */
Pn532Error pn532_mifare_classic_write_block(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    uint8_t block,
    const uint8_t* block_data);

/**
 * @brief Write a 4-byte page to Mifare Ultralight.
 */
Pn532Error pn532_mifare_ultralight_write_page(
    const FuriHalI2cBusHandle* handle,
    uint8_t tg,
    uint8_t page,
    const uint8_t* page_data);

/**
 * @brief Deselect / Release current target.
 */
Pn532Error pn532_in_release(const FuriHalI2cBusHandle* handle, uint8_t tg);

/**
 * @brief Initialize PN532 as target for card emulation.
 */
Pn532Error pn532_tg_init_as_target(
    const FuriHalI2cBusHandle* handle,
    const uint8_t* mifare_params,
    size_t params_len,
    uint8_t* rx_cmd,
    size_t* rx_len,
    uint32_t timeout_ms);

/**
 * @brief Send and receive data in target emulation mode.
 */
Pn532Error pn532_tg_get_data(
    const FuriHalI2cBusHandle* handle,
    uint8_t* rx_data,
    size_t* rx_len,
    uint32_t timeout_ms);

Pn532Error pn532_tg_set_data(
    const FuriHalI2cBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_len,
    uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
