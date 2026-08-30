**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

## ⬇️ Download

> ### [🐬 qFlipper Package (.tgz)](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/releases/download/{VERSION_TAG}/flipper-z-f7-update-{VERSION_TAG}.tgz)

> ### [📦 Zipped Archive (.zip)](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/releases/download/{VERSION_TAG}/flipper-z-f7-update-{VERSION_TAG}.zip)

> ### [💾 Standalone Firmware (.bin)](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/releases/download/{VERSION_TAG}/flipper-z-f7-firmware-oled-{VERSION_TAG}.bin)

> ### [🔧 DFU Flash File (.dfu)](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/releases/download/{VERSION_TAG}/flipper-z-f7-firmware-oled-{VERSION_TAG}.dfu)

Check the [install guide](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532#-4-step-quick-start-flashing-the-device) in the README if you're not sure, or [open an issue](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/issues) if you have questions or encounter problems!

## 🔧 Hardware Requirements

This firmware is built for the **DIY Flipper Zero (OLED Edition)**:
- **MCU**: WeAct STM32WB55CGU6
- **Display**: SSD1306 0.96" I2C OLED (`0x3C`)
- **Keypad**: PCF8574 I2C Expander (`0x20`) with interrupt on PB0
- **NFC**: PN532 over I2C3 (PA7/PB4, IRQ on PA2)
- **Sub-GHz**: CC1101 over SPI1 (CS: PA15, GDO0: PA1)
- **Power Monitor**: INA219/INA226 I2C (`0x40`)
- **MicroSD**: SPI1 (CS: PA10)

## ⚖️ Legal & Educational Disclaimer

This project is strictly for **educational, academic research, and authorized security testing purposes only**.  
Do **NOT** use this firmware or hardware for unauthorized access, card cloning, or any illegal activities.  
The developers assume **no liability** for any misuse.

## 🚀 Changelog
{CHANGELOG}
