# 🎯 Hardware Targets & Board Definitions — DIY Flipper Zero (OLED Edition)

> Flipper firmware is modular and supports different hardware configurations within a unified codebase. Hardware-specific differences are encapsulated in `furi_hal`, board initialization routines, linker scripts, and target definitions.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

## 🛠️ DIY Flipper Zero (OLED Edition) Target Profile

Our DIY Flipper Zero port utilizes the **`f7`** target architecture customized for the **WeAct STM32WB55** development board:

| Subsystem | Hardware Component | Interface / Bus | Key Pinout |
|---|---|---|---|
| **MCU** | STM32WB55CGU6 (UFQFPN48) | Dual-Core Cortex-M4 + M0+ | 64 MHz PLL, 1 MB Flash, 256 KB RAM |
| **Display** | 0.96" SSD1306 / SH1106 OLED | I2C1 Bus (0x3C / 0x3D) | SCL: `PA9`, SDA: `PB9` |
| **Keypad** | PCF8574 Remote 8-Bit I/O Expander | I2C1 Bus (0x20) | INT: `PB0` (Ext Interrupt) |
| **NFC** | NXP PN532 (HW Crypto1 Accelerated) | I2C3 Bus (0x24) | SCL: `PA7` (header "C0"), SDA: `PB4` (header "C1"), IRQ: `PA2` |
| **Sub-GHz** | TI CC1101 Transceiver | SPI1 Bus + GDO0 IRQ | CS: `PA15`, SCK: `PB3`, MOSI: `PB5`, MISO: `PA6`, GDO0: `PA1` |
| **MicroSD** | SPI FatFS Storage | SPI1 Bus | CS: `PA10`, SCK: `PB3`, MOSI: `PB5`, MISO: `PA6` |
| **Power Monitor**| TI INA219 / INA226 Fuel Gauge | I2C1 Bus (0x40) | SCL: `PA9`, SDA: `PB9`, ALERT: `PB1` |
| **LF-RFID** | Discrete Analog Tank (125 kHz) | TIM2 / TIM1 Timers | Carrier: `PA5`, Envelope RX: `PA1`, Emulate: `PA2` |

---

## 📋 Target Definition Architecture

Target-specific configurations reside in `targets/` sub-directories:

* `include_paths`: Header search paths relative to the target directory.
* `sdk_header_paths`: Target headers exposed in the generated C/C++ FAP SDK.
* `startup_script`: System initialization script executed on power-on.
* `linker_script_flash`: Linker script defining flash memory layout for the main firmware image.
* `linker_script_ram`: Linker script used for standalone RAM-based updater execution.
* `linker_script_app`: Linker script used for compiling relocatable `.fap` application binaries.
* `sdk_symbols`: Symbol export table for dynamic linking of external applications.

---

## 🚀 Compiling for the Target

To build the custom OLED firmware for target `f7`:

```bash
# Standard compilation
fbt

# Build complete qFlipper update bundle (.tgz)
fbt --with-updater updater_package
```
