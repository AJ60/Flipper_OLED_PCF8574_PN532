# 🤝 Contributing to DIY Flipper Zero (OLED Edition)

Thank you for your interest in contributing! This project is a DIY hardware firmware fork targeting the **WeAct STM32WB55** with I2C OLED, PCF8574 keypad, PN532 NFC, CC1101 Sub-GHz, and 125 kHz LF-RFID.

**Maintainer**: [AJ_60](https://github.com/AJ60)

> [!IMPORTANT]
> All contributions must comply with the project's **educational and ethical use** policy.
> This firmware is for authorized security research and academic purposes only. Do not submit features designed to facilitate unauthorized access to NFC/RFID cards or Sub-GHz remotes.

---

## 🚀 Getting Started

### Prerequisites

- **Hardware**: WeAct STM32WB55CGU6 development board wired as described in the [README](ReadMe.md)
- **OS**: Linux, macOS, or Windows (with WSL or Git Bash recommended for FBT)
- **Python**: 3.8+
- **Toolchain**: `gcc-arm-none-eabi`, `scons`, `protobuf`, `grpcio-tools`

### Building from Source

```bash
# Clone with all submodules
git clone --recurse-submodules https://github.com/AJ60/Oled_PCF8574_PN532.git
cd Oled_PCF8574_PN532

# Build basic firmware
./fbt

# Build complete qFlipper .tgz installer package
./fbt COMPACT=1 DEBUG=0 --with-updater updater_package

# Run linter (required before submitting a PR)
./fbt lint_all
```

Compiled binaries land in `build/f7-firmware-C/` and `dist/f7-C/`.

---

## 📋 How to Contribute

### 1. Fork & Branch

```bash
git checkout -b feat/your-feature-name
# or
git checkout -b fix/short-description-of-bug
```

### 2. Branch Naming Convention

| Type | Pattern | Example |
|---|---|---|
| Feature | `feat/<name>` | `feat/ina226-support` |
| Bug fix | `fix/<name>` | `fix/oled-flicker-on-wake` |
| Documentation | `docs/<name>` | `docs/nfc-protocol-guide` |
| Build / CI | `ci/<name>` | `ci/cache-toolchain` |
| Refactor | `refactor/<name>` | `refactor/pcf8574-driver` |

### 3. Commit Message Format

This project uses **Conventional Commits**:

```
<type>(<scope>): <short description>

[optional body]
[optional BREAKING CHANGE: <description>]
```

**Types**: `feat`, `fix`, `docs`, `ci`, `refactor`, `test`, `chore`  
**Scopes**: `hal`, `nfc`, `rfid`, `subghz`, `oled`, `keypad`, `power`, `build`, `docs`

**Examples**:
```
feat(nfc): add PN532 NTAG216 full memory read support
fix(oled): eliminate display flicker during sleep/wake cycle
docs(hardware): correct I2C3 pin mapping for PN532
ci(build): add apt package caching for ARM toolchain
```

### 4. Code Style

Run `./fbt lint_all` before pushing. The project uses `clang-format` with the config in [`.clang-format`](.clang-format).

---

## 🔧 Hardware Testing Requirements

**All PRs that touch firmware code MUST be tested on physical hardware** before merging.

At minimum, verify:
- ✅ Device boots and shows the OLED splash screen
- ✅ All 6 PCF8574 keypad buttons respond correctly
- ✅ The subsystem you modified works correctly (NFC, Sub-GHz, RFID, etc.)
- ✅ Sleep/wake cycle does not cause OLED flicker or system hang
- ✅ No unexpected I2C bus wedging under normal use

Use the PR checklist in the [pull request template](.github/pull_request_template.md).

---

## 🐛 Reporting Bugs

Use the [Bug Report issue template](https://github.com/AJ60/Oled_PCF8574_PN532/issues/new?template=01_bug_report.yml).

Please include:
- Firmware version tag
- Hardware configuration (MCU board, modules connected)
- Steps to reproduce
- Debug log output if available (via USB serial CLI)

---

## 🔒 Security Vulnerabilities

**Do NOT open a public issue for security bugs.**  
See [SECURITY.md](SECURITY.md) for responsible disclosure instructions.

---

## 📝 License

By contributing, you agree that your contributions will be licensed under the [GNU GPL v3.0](LICENSE).
