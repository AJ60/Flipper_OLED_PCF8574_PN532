# 🐬 Modified qFlipper for DIY Flipper Zero

> Modified version of the official **qFlipper** desktop application that allows flashing and recovering raw STM32WB55 development boards without requiring factory OTP pre-configuration.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

## 📦 What Is In This Directory?

* **`qFlipperMod.7z`**: Pre-built portable Windows build of modified qFlipper with patched bootloader verification logic.

---

## 🚀 How to Use

1. Extract **`qFlipperMod.7z`** to a folder on your computer using 7-Zip or WinRAR.
2. Put your WeAct STM32WB55 board into **DFU mode** (hold **BOOT0**, connect USB, release **BOOT0**).
3. Run **`qFlipper.exe`** from the extracted directory.
4. When qFlipper reports **"RECOVERY MODE"**, click **"REPAIR"** to flash the Flipper bootloader.
5. Once complete, put the board back into DFU mode and select **"Install from file"** to flash the custom **`.tgz`** firmware package from [Releases](https://github.com/AJ60/Oled_PCF8574_PN532/releases).

---

## 📄 License & Attribution

Based on the open-source [qFlipper](https://github.com/flipperdevices/qFlipper) application under the GNU General Public License v3.0.