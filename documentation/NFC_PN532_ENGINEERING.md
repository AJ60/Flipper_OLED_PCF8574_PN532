# ⚡ PN532 NFC Protocol & Hardware Acceleration Engineering: DIY Flipper Zero

**Component**: PN532 NFC Controller (NXP) over I2C3 (SCL: `PA7`, SDA: `PB4`, IRQ: `PA2`)  
**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Status**: 🚧 **Under Active Development / Experimental**

> [!CAUTION]
> ⚖️ **EDUCATIONAL & RESEARCH USE ONLY**:
> This document and the associated NFC drivers are provided strictly for educational purposes, cryptographic research, and authorized personal testing. Any unauthorized interception, card cloning, or unlawful activities are strictly prohibited. The author assumes no liability for misuse.

> [!NOTE]
> **Tested & Verified NFC Hardware Target Scope**:
> - ✅ **MIFARE Classic 1K / 4K**: Tested with hardware Crypto1 acceleration and dictionary attack parsing.
> - ✅ **NTAG Series (NTAG213, NTAG215, NTAG216, Ultralight)**: Tested with direct page reading and standard NDEF dumps.
> - ✅ **Contactless EMV Bank / ATM Payment Cards**: Tested with ISO 14443-4 APDU wrapping and PPSE application selection.
> - ⚠️ Other proprietary transponders or card types are experimental and under ongoing development.

---

## 1. PN532 Hardware Crypto1 Authentication Architecture

Unlike standard software-emulated Crypto1 cipher implementations, this firmware interfaces directly with the PN532's onboard hardware crypto engine via the `InAuth` (Command `0x40`) and `InDataExchange` (Command `0x42`) primitives.

```mermaid
graph TD
    Start[NFC Poller Starts Reading Tag] --> Detect[InListPassiveTarget 0x4A]
    Detect --> Check_Type{Analyze ATQA & SAK Bytes}
    
    Check_Type -->|SAK 0x08 / 0x18: MIFARE Classic| MFC_Flow[MIFARE Classic Hardware Auth Flow]
    Check_Type -->|SAK 0x20 / 0x28: ISO 14443-4| ISO4_Flow[ISO 14443-4 APDU Tunneling]
    Check_Type -->|SAK 0x00: Ultralight / NTAG| NTAG_Flow[Direct Page Read Flow]

    subgraph MFC_Hardware_Engine [PN532 Hardware Crypto1 Engine]
        MFC_Flow --> Load_Key[Send InAuth Command 0x40 with Key A or B]
        Load_Key --> HW_Auth[PN532 Hardware Crypto1 Mutual Authentication]
        HW_Auth --> Auth_Success{Auth Result OK?}
        Auth_Success -->|Yes| Read_Block[InDataExchange 0x42: Read Data 0x30]
        Auth_Success -->|No / Checksum Error| Checksum_Retry{Retry Attempt < 3?}
        Checksum_Retry -->|Yes| Load_Key
        Checksum_Retry -->|No / Exceeded| Skip_Sector[Mark Key Failed & Advance to Next Sector]
    end

    subgraph ISO4_APDU_Tunnel [ISO 14443-4 APDU Wrapper for EMV Bank Cards]
        ISO4_Flow --> RATS[Send RATS Request for Answer to Select]
        RATS --> Wrap_APDU[Wrap Raw ISO 7816-4 APDU into InDataExchange Frame]
        Wrap_APDU --> Send_APDU[Transmit to PN532 over I2C3]
        Send_APDU --> Unwrap_APDU[Strip PN532 Response Header & Extract Status Word SW1/SW2]
        Unwrap_APDU --> EMV_Parse[Pass to EMV Banking Card Application Parser]
    end
```

---

## 2. ISO 14443-4 APDU Protocol Frame Flow (EMV Cards)

To read modern contactless payment cards (Visa, Mastercard, RuPay, Amex) and government e-Passports, APDUs are tunneled through the PN532 without breaking ISO 7816-4 framing:

```mermaid
sequenceDiagram
    autonumber
    participant App as EMV Application / FAP
    participant HAL as Furi HAL PN532 Driver
    participant I2C3 as I2C3 Bus Controller
    participant PN532 as PN532 NFC Module (0x24)
    participant Card as Contactless Bank Card

    App->>HAL: nfc_poller_trx(APDU: SELECT PPSE "2PAY.SYS.DDF01")
    HAL->>HAL: Wrap APDU: [0x42 (InDataExchange), Target 0x01, CLA, INS, P1, P2, Lc, Data, Le]
    HAL->>I2C3: Transmit Frame via I2C3 DMA
    I2C3-->>PN532: RF Field Transmission (13.56 MHz)
    PN532-->>Card: ISO 14443-4 I-Block Transmission
    Card-->>PN532: Response I-Block (FCI Template + SW 0x9000)
    PN532-->>I2C3: Pulls IRQ PA2 Low (Data Ready)
    I2C3->>HAL: Read Response Frame (Length, Status, Data, DCS)
    HAL->>HAL: Verify DCS Checksum & Unwrap PN532 Framing
    HAL->>App: Return Pure ISO 7816-4 Response (FCI Data + SW 0x9000)
```

---

## 3. Communication Robustness & Error Recovery

### DCS Checksum Error Retry Loop:
PN532 I2C communication in high RF fields can occasionally suffer from transient checksum errors. Previously, a single corrupted byte caused an entire sector read to fail, triggering a wasteful 2,475-key dictionary attack timeout.

1. **3-Attempt InDataExchange Retry**: If a DCS checksum mismatch is detected, the driver automatically re-transmits the frame up to 3 times before declaring an error.
2. **CIU Reset on Consecutive Failures**: If 5 consecutive failures occur, the driver pulses the PN532 hardware reset line and re-initializes Contactless Interface Unit (CIU) registers.
3. **Dictionary Attack Early Abort**: If 10 consecutive hardware errors occur, the dictionary attack loop aborts immediately with a clear error prompt rather than freezing the screen for minutes.

---

## 4. Card Type SAK & ATQA Identification Matrix

| SAK Byte | ATQA | Card Technology / Standard | Supported Operations |
|---|---|---|---|
| `0x08` | `0x0004` | **MIFARE Classic 1K** | Read, Write, Emulate, HW Crypto1 Dict Attack |
| `0x18` | `0x0002` | **MIFARE Classic 4K** | Read, Write, Emulate, 40 Sectors HW Auth |
| `0x09` | `0x0004` | **MIFARE Mini (0.3K)** | Read, Write, Emulate |
| `0x00` | `0x0044` | **MIFARE Ultralight / NTAG213/215/216** | Fast Read (Pages 0–44), Password Auth, NDEF Read/Write |
| `0x20` | `0x0344` | **MIFARE DESFire / ISO 14443-4** | APDU Exchange, Free Directory Traverse |
| `0x28` | `0x0048` | **JCOP / EMV Contactless Bank Cards** | ISO 7816-4 APDU Tunneling, PPSE Application Selection |
