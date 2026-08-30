# What's new

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

## Type of Change

- [ ] 🐛 Bug fix
- [ ] ✨ New feature / enhancement
- [ ] 🔧 Hardware driver / HAL change
- [ ] 📖 Documentation update
- [ ] 🔨 Build / CI / tooling change
- [ ] ⚠️ Breaking change (will require hardware re-flash or re-wiring)

## Summary of Changes

- [Describe changes here]

---

## For the Reviewer

### Code Quality
- [ ] I've confirmed the code builds cleanly with `./fbt` (no new warnings)
- [ ] I've confirmed lint passes with `./fbt lint_all`

### Hardware Verification
- [ ] I've flashed this firmware to a physical **WeAct STM32WB55** board and verified it boots
- [ ] **OLED display** renders correctly (no flicker, no blank screen on sleep/wake)
- [ ] **PCF8574 keypad** buttons respond correctly (UP, DOWN, LEFT, RIGHT, OK, BACK)
- [ ] Relevant subsystem tested (check all that apply):
  - [ ] NFC / PN532 (MIFARE Classic, NTAG, or EMV bank card)
  - [ ] Sub-GHz / CC1101
  - [ ] MicroSD card access
  - [ ] LF-RFID 125kHz (if touched)
  - [ ] INA219/226 battery fuel gauge

### Stability
- [ ] I've confirmed the bug is fixed / feature is stable after extended use
- [ ] No regressions observed in unrelated subsystems
