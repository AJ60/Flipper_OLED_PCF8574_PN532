# 🛠️ Supplementary Build & Flashing Scripts

> Collection of helper scripts, flashing utilities, and asset compilation tools for the DIY Flipper Zero (OLED Edition).

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

## ⚡ Recommended Flashing Tools

* 🖥️ **GUI OTP Generator & Flasher**: Use [`mics/FlipperOTP/generate_otp_gui.exe`](../mics/FlipperOTP/) for generating and flashing the board's OTP profile (Board Rev `12`, Display Type `MGG`).
* 🔌 **1-Click Flasher GUI**: Run [`scripts/diy_flasher_gui.py`](diy_flasher_gui.py) (`python scripts/diy_flasher_gui.py`) for automatic download, extraction, and DFU flashing.
* 🐬 **qFlipper Recovery**: Use official qFlipper or [`mics/qFlipperMod/`](../mics/qFlipperMod/) to flash the bootloader and custom firmware `.tgz` package.

---

## 💻 Manual CLI Flashing Scripts

CLI flashing scripts rely on `STM32_Programmer_CLI.exe` from [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html).

### 1. Option Bytes Verification
```bash
python scripts/ob.py check
python scripts/ob.py set
```

### 2. Assets Delivery to MicroSD
After compiling resources, send the asset bundle directly over serial console:
```bash
python scripts/storage.py -p <FLIPPER_COM_PORT> send build/f7-firmware-C/resources /ext
```

### 3. Slideshow & Animation Generation
Place `.png` frames inside `assets/slideshow/<SHOW_NAME>/` (e.g. `frame_00.png`, `frame_01.png`), then compile:
```bash
python scripts/slideshow.py -i assets/slideshow/<SHOW_NAME>/ -o assets/slideshow/<SHOW_NAME>/.slideshow
```
