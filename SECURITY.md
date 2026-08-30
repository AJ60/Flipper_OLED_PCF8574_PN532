# 🔒 Security Policy

## Supported Versions

This project supports the most recent release tag on the `main` branch.

| Version | Supported |
|---------|-----------|
| Latest (`main`) | ✅ |
| Older tags (`v2.1` and below) | ❌ (no backports) |

---

## ⚠️ Scope & Responsible Use Notice

This is a **DIY educational hardware project** for the STM32WB55 microcontroller implementing:
- I2C OLED display driver (SSD1306)
- NFC reader/writer via PN532 (MIFARE Classic, NTAG, EMV)
- Sub-GHz radio via CC1101
- 125 kHz LF-RFID analog subsystem
- I2C keypad via PCF8574

**This firmware is strictly for educational, academic research, and authorized security testing purposes only.**

### In-Scope for Security Reports
- Memory safety vulnerabilities (buffer overflows, use-after-free) in firmware code
- Logic errors in NFC/RFID/Sub-GHz protocol handling that could cause data corruption or unexpected device behavior
- Build system or CI/CD vulnerabilities

### Out-of-Scope
- Issues that require physical access to a third party's NFC/RFID card — by design, this is a research tool
- Issues in upstream Flipper Zero / Momentum Firmware code that have not been patched here
- Issues in third-party libraries (FreeRTOS, STM32 HAL) — report those upstream

---

## 📬 Reporting a Vulnerability

**Please do NOT open a public GitHub Issue for security vulnerabilities.**

Use GitHub's built-in **Private Vulnerability Reporting**:

1. Go to the [Security tab](https://github.com/AJ60/Oled_PCF8574_PN532/security/advisories/new) of this repository
2. Click **"Report a vulnerability"**
3. Fill in the details — include:
   - Affected firmware version/tag
   - Step-by-step reproduction
   - Potential impact assessment
   - Any suggested mitigations

You can expect an initial acknowledgment within **7 days**. We will work with you to assess and fix the issue before public disclosure.

---

## 🙏 Thank You

We appreciate responsible disclosure and the effort it takes to research and report security issues. Thank you for helping keep this project and its users safe.
