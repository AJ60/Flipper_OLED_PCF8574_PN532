# SubGhz Keystore Documentation

## Overview

The SubGhz keystore stores manufacturer codes for rolling code (Keeloq) protocols. These codes are used to decode and transmit signals from remote controls using Keeloq encryption.

## File Locations

- **System keystore** (encrypted): `/ext/subghz/assets/keeloq_mfcodes`
- **User keystore** (encrypted): `/ext/subghz/assets/keeloq_mfcodes_user`
- **Unencrypted common codes**: `/ext/subghz/assets/keeloq_mfcodes` (unencrypted version)

## Keystore Format

```
Filetype: Flipper SubGhz Keystore File
Version: 0
Encryption: 0 or 1 (0=unencrypted, 1=AES256)
KEY:TYPE:NAME
```

### Fields:
- **KEY**: 64-bit manufacturer code in hex (16 characters)
- **TYPE**: Encryption method:
  - 1 = Simple Learning (PT2262, EV1527)
  - 2 = Normal Learning (HT12E, HT6P20B)
  - 3 = Secure Learning / Keeloq (HCS200, HCS300, HCS301, CAME, Nice, Chamberlain, Somfy)
  - 4 = Magic XOR Type 1 Learning
  - 5 = FAAC SLH
  - 6 = Magic Serial Type 1
  - 7 = Magic Serial Type 2
  - 8 = Magic Serial Type 3
  - 0 = Iterates over both previous and manufacturer code in direct and reverse byte sequence
- **NAME**: Name (string without spaces, max 64 characters)

### Example Entry:
```
0000000000000001:1:PT2262_CH1
```

## Encrypted vs Unencrypted Keystores

### Encrypted Keystore (AES256)
- Stored with `Encryption: 1`
- Requires hardware enclave key for decryption
- Enclave key is factory-provisioned on genuine Flipper Zero devices
- **DIY boards**: Enclave key is NOT provisioned → encrypted keystore cannot be decrypted
- When decryption fails, app logs warning once and continues with unencrypted fallback

### Unencrypted Keystore
- Stored with `Encryption: 0`
- No hardware enclave required
- Can be used on any hardware
- Recommended for DIY boards

## Generating Encrypted Keystore

Use the CLI command to generate an encrypted keystore from an unencrypted input:

```bash
subghz keystore_gen <input_unencrypted> <output_encrypted>
```

Example:
```bash
subghz keystore_gen /ext/subghz/assets/keeloq_mfcodes /ext/subghz/assets/keeloq_mfcodes
```

This command:
1. Reads the unencrypted keystore from `<input_unencrypted>`
2. Generates a random IV
3. Encrypts the data using AES256 with hardware enclave
4. Writes encrypted keystore to `<output_encrypted>`

**Note**: This command requires hardware with provisioned enclave key. On DIY boards without enclave, the command will fail.

## Common Manufacturer Codes

The system includes common manufacturer codes in the unencrypted keystore at `/ext/subghz/assets/keeloq_mfcodes`:

| Manufacturer | Type | Codes |
|--------------|------|-------|
| PT2262/EV1527 | Simple (1) | CH1-CH4 |
| HT12E/HT6P20B | Normal (2) | HT12E, HT6P20B |
| HCS200/300/301 | Secure (3) | HCS200, HCS300, HCS301 |
| FAAC SLH | FAAC (5) | FAAC_SLH |
| CAME TAM/ATOMO | Secure (3) | CAME_TAM, CAME_ATOMO |
| Nice Flo/Flor-S | Secure (3) | NICE_FLO, NICE_FLOR_S |
| Chamberlain/LiftMaster | Secure (3) | CHAMBERLAIN |
| Somfy RTS | Secure (3) | SOMFY_RTS |

## User Keystore

Users can add their own manufacturer codes to `/ext/subghz/assets/keeloq_mfcodes_user`. This file is auto-created on first run if missing.

### Format:
Same as system keystore but typically encrypted. Users can:
1. Create unencrypted file with their codes
2. Use `subghz keystore_gen` on a provisioned device to encrypt it
3. Copy encrypted file to DIY board

## Troubleshooting

### "Decrypted keystore data is not valid ASCII (enclave key not provisioned)"
- **Cause**: Hardware enclave key not provisioned (DIY board)
- **Solution**: Use unencrypted keystore, or provision enclave key on genuine Flipper Zero

### "Unable to open file for read: /ext/subghz/assets/keeloq_mfcodes_user"
- **Cause**: User keystore doesn't exist
- **Solution**: Auto-created on first run, or manually create empty file

### "Invalid encrypted data: [garbage]"
- **Cause**: Trying to read encrypted keystore without enclave key
- **Solution**: Use unencrypted keystore, or use `subghz keystore_gen` on provisioned hardware

## Security Notes

- Encrypted keystore uses AES256 with hardware enclave
- IV is randomly generated per file
- IV is stored in file header (not secret)
- Enclave key never leaves secure element
- Unencrypted keystore is readable by anyone with SD card access

## Related Files

- `/ext/subghz/assets/keeloq_mfcodes` - System keystore (encrypted)
- `/ext/subghz/assets/keeloq_mfcodes_user` - User keystore (encrypted, auto-created)
- `/ext/subghz/assets/keeloq_mfcodes` (unencrypted) - Common codes for DIY boards
- `/ext/subghz/assets/keeloq_mfcodes_user.example` - Template for user codes