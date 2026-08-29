# 🎯 Firmware Targets

> Hardware target definitions and board abstraction layers for Flipper firmware.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

## 📁 Directory Structure

| Directory | Target / Function | Description |
|---|---|---|
| **`f7/`** | **Flipper Zero (STM32WB55)** | Primary hardware target for the DIY Flipper (WeAct STM32WB55 + I2C OLED SSD1306/SH1106 + PCF8574 Keypad + PN532 NFC). Contains HAL drivers, pinmux configurations, startup scripts, and linker files. |
| **`furi_hal_include/`** | **Global HAL Includes** | Hardware abstraction interfaces, driver declarations, and bus APIs common across all target platforms. |
| **`f18/`** | Legacy target definition | Reserved legacy hardware definition. |

---

For complete architectural details on pin multiplexing, interrupt vectors, and peripheral buses, see [`documentation/HARDWARE_DESIGN.md`](../documentation/HARDWARE_DESIGN.md) and [`documentation/HardwareTargets.md`](../documentation/HardwareTargets.md).
