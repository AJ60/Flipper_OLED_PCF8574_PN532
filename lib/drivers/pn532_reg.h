/**
 * @file pn532_reg.h
 * @brief NXP PN532 Commands, Registers, and Frame Constants
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* I2C Address */
#define PN532_I2C_ADDRESS      (0x24)
#define PN532_I2C_ADDRESS_8BIT (PN532_I2C_ADDRESS << 1)

/* Frame Delimiters and Identifiers */
#define PN532_PREAMBLE   (0x00)
#define PN532_STARTCODE1 (0x00)
#define PN532_STARTCODE2 (0xFF)
#define PN532_POSTAMBLE  (0x00)

#define PN532_HOSTTOPN532 (0xD4)
#define PN532_PN532TOHOST (0xD5)

/* PN532 General Commands */
#define PN532_CMD_DIAGNOSE           (0x00)
#define PN532_CMD_GETFIRMWAREVERSION (0x02)
#define PN532_CMD_GETGENERALSTATUS   (0x04)
#define PN532_CMD_READREGISTER       (0x06)
#define PN532_CMD_WRITEREGISTER      (0x08)
#define PN532_CMD_READGPIO           (0x0C)
#define PN532_CMD_WRITEGPIO          (0x0E)
#define PN532_CMD_SETSERIALBAUDRATE  (0x10)
#define PN532_CMD_SETPARAMETERS      (0x12)
#define PN532_CMD_SAMCONFIGURATION   (0x14)
#define PN532_CMD_POWERDOWN          (0x16)

/* PN532 RF Configuration Commands */
#define PN532_CMD_RFCONFIGURATION  (0x32)
#define PN532_CMD_RFREGULATIONTEST (0x58)

/* PN532 Initiator / Poller Commands */
#define PN532_CMD_INJUMPFORDEP        (0x56)
#define PN532_CMD_INJUMPFORPSL        (0x46)
#define PN532_CMD_INLISTPASSIVETARGET (0x4A)
#define PN532_CMD_INATR               (0x50)
#define PN532_CMD_INPSL               (0x4E)
#define PN532_CMD_INDATAEXCHANGE      (0x40)
#define PN532_CMD_INCOMMUNICATETHRU   (0x42)
#define PN532_CMD_INDESELECT          (0x44)
#define PN532_CMD_INRELEASE           (0x52)
#define PN532_CMD_INSELECT            (0x54)
#define PN532_CMD_INAUTOPOLL          (0x60)

/* PN532 Target / Listener Commands */
#define PN532_CMD_TGINITASTARGET        (0x8C)
#define PN532_CMD_TGSETGENERALBYTES     (0x92)
#define PN532_CMD_TGGETDATA             (0x86)
#define PN532_CMD_TGSETDATA             (0x8E)
#define PN532_CMD_TGSETMETADATA         (0x94)
#define PN532_CMD_TGGETINITIATORCOMMAND (0x88)
#define PN532_CMD_TGRESPONSETOINITIATOR (0x90)
#define PN532_CMD_TGGETTARGETSTATUS     (0x8A)

/* RFConfiguration Items */
#define PN532_RF_CONFIG_FIELD          (0x01) // RF field on/off
#define PN532_RF_CONFIG_VAR_TIMINGS    (0x02) // Various timings (ATR, retry)
#define PN532_RF_CONFIG_MAX_RTY_COM    (0x04) // Max retries for communication
#define PN532_RF_CONFIG_MAX_RETRIES    (0x05) // Max retries for passive target
#define PN532_RF_CONFIG_ANALOG_106A    (0x0A) // Analog 106 kbps Type A
#define PN532_RF_CONFIG_ANALOG_212_424 (0x0B) // Analog 212/424 kbps FeliCa
#define PN532_RF_CONFIG_ANALOG_TYPE_B  (0x0C) // Analog Type B
#define PN532_RF_CONFIG_ANALOG_14443_4 (0x0D) // Analog ISO/IEC 14443-4

/* CIU Contactless Interface Unit Registers (16-bit address) */
#define PN532_REG_CIU_Mode        (0x6301)
#define PN532_REG_CIU_TxMode      (0x6302)
#define PN532_REG_CIU_RxMode      (0x6303)
#define PN532_REG_CIU_TxControl   (0x6304)
#define PN532_REG_CIU_TxAuto      (0x6305)
#define PN532_REG_CIU_TxSel       (0x6306)
#define PN532_REG_CIU_RxSel       (0x6307)
#define PN532_REG_CIU_RxThreshold (0x6308)
#define PN532_REG_CIU_Demod       (0x6309)
#define PN532_REG_CIU_FelNFC      (0x630A)
#define PN532_REG_CIU_FelNFC2     (0x630B)
#define PN532_REG_CIU_MifNFC      (0x630C)
#define PN532_REG_CIU_ManualRCV   (0x630D)
#define PN532_REG_CIU_TypeB       (0x630E)
#define PN532_REG_CIU_Command     (0x6331)
#define PN532_REG_CIU_CommIEn     (0x6332)
#define PN532_REG_CIU_DivIEn      (0x6333)
#define PN532_REG_CIU_CommIrq     (0x6334)
#define PN532_REG_CIU_DivIrq      (0x6335)
#define PN532_REG_CIU_Error       (0x6336)
#define PN532_REG_CIU_Status1     (0x6337)
#define PN532_REG_CIU_Status2     (0x6338)
#define PN532_REG_CIU_FIFOData    (0x6339)
#define PN532_REG_CIU_FIFOLevel   (0x633A)
#define PN532_REG_CIU_WaterLevel  (0x633B)
#define PN532_REG_CIU_Control     (0x633C)
#define PN532_REG_CIU_BitFraming  (0x633D)
#define PN532_REG_CIU_Coll        (0x633E)

/* Special Function Registers (SFRs) */
#define PN532_SFR_P3     (0xFFB0)
#define PN532_SFR_P3CFGA (0xFFFC)
#define PN532_SFR_P3CFGB (0xFFFD)
#define PN532_SFR_P7     (0xFFF7)
#define PN532_SFR_P7CFGA (0xFFF4)
#define PN532_SFR_P7CFGB (0xFFF5)

#ifdef __cplusplus
}
#endif
