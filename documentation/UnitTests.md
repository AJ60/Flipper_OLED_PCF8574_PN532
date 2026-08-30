# 🧪 Unit Tests Framework — DIY Flipper Zero (OLED Edition)

> Guidelines for authoring, running, and debugging automated unit tests on the DIY Flipper firmware.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

## 🚀 Running Unit Tests

1. **Build the firmware with unit test suite enabled**:
   ```bash
   ./fbt FIRMWARE_APP_SET=unit_tests updater_package
   ```
2. **Flash to board and deliver test assets**:
   ```bash
   python scripts/storage.py -p <PORT> send build/f7-firmware-C/resources /ext
   ```
3. **Execute via serial console**:
   ```bash
   unit_tests
   ```
   *(To run an isolated test, pass its name: `unit_tests nfc` or `unit_tests infrared`).*

---

## 🛠️ Adding New Unit Tests

* **Entry Point**: `applications/debug/unit_tests/`
* **Test Assets**: Place test data vectors (`.irtest`, `.fff`, binary dumps) into `applications/debug/unit_tests/resources/unit_tests/<FEATURE>/`.
* **Manifest**: Register test plugins in `applications/debug/unit_tests/application.fam`.
