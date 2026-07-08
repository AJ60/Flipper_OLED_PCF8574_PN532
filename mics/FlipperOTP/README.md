# 🐬 DIY Flipper Zero — OTP Tool

[![GitHub](https://img.shields.io/badge/GitHub-oled__flipper-orange?logo=github)](https://github.com/artema0g/oled_flipper)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://github.com/artema0g/oled_flipper/blob/main/LICENSE)

Standalone Windows application to **generate**, **flash**, and **read** the OTP (One-Time Programmable) memory profile for the DIY Flipper Zero (SSD1306 OLED & MCP23017 edition).

> Part of the [**oled_flipper**](https://github.com/artema0g/oled_flipper) project.

---

## ⚠️ WARNING

> [!CAUTION]
> **OTP memory can only be written ONCE.** You cannot erase it or change written bits back from `1` to `0`.  
> Double-check all settings before flashing. **Proceed at your own risk.**

---

## 📦 Files

| File | Description |
|---|---|
| `generate_otp_gui.exe` | Portable standalone GUI tool (everything bundled — just run it) |
| `generate_otp_gui.py` | Python source code of the GUI tool |
| `generate_otp.py` | Legacy command-line version of the generator |
| `flipper.ico` | Application icon |

---

## 💻 System Requirements

| Requirement | Details |
|---|---|
| **OS** | Windows 10 / 11 (64-bit). Acrylic glass effects require Windows 11 |
| **STM32CubeProgrammer** | **Required** — the tool uses `STM32_Programmer_CLI.exe` for flash/read operations. [Download here](https://www.st.com/en/development-tools/stm32cubeprog.html) |
| **USB Drivers** | Installed automatically with STM32CubeProgrammer (DFU driver for STM32) |
| **Hardware** | WeAct STM32WB55 board connected via USB |

> [!NOTE]
> Without STM32CubeProgrammer installed, only the **"1. Save .bin"** button works (generates a binary file for manual flashing).  
> The **"2. Flash (DFU)"** and **"3. Read (DFU)"** buttons require the CLI tool.

---

## 🚀 Quick Start

### 1. Run the tool
Simply launch **`generate_otp_gui.exe`** — no installation required.

### 2. Configure your device profile

| Field | Description | Constraints |
|---|---|---|
| **Device Name** | Unique name for your Flipper (shown in About menu & Bluetooth) | Max 8 ASCII characters |
| **Board Version** | Hardware board revision | 0–255 (use `12` for WeAct STM32WB55) |
| **Display Type** | Screen driver selection | **MGG** for custom SSD1306 I2C OLED *(critical!)* |
| **Body Color** | Cosmetic shell color in animations | Black / White / Transparent |
| **Sub-GHz Region** | Frequency rules for CC1101 radio | Europe, USA, Japan, or World |

> [!IMPORTANT]
> **Display Type must be set to `MGG`** for the custom SSD1306 OLED screen to work.  
> Selecting `ERC` will result in a black screen on DIY boards.

### 3. Enter DFU mode on the WeAct board
1. **Disconnect** the USB cable from the board
2. **Press and hold** the **BOOT0** button
3. **Connect** the USB cable to your PC
4. **Release** the BOOT0 button
5. The status indicator in the app should change to **🟢 Connected (Ready)**

> [!TIP]
> Hover over the DFU status label in the app for a quick reminder of these steps.

### 4. Flash or Read

| Button | Action |
|---|---|
| **1. Save .bin** | Generates and saves an OTP binary file to disk (for manual flashing via STM32CubeProgrammer) |
| **2. Flash (DFU)** | Writes the OTP profile directly to address `0x1FFF7000` via USB DFU |
| **3. Read (DFU)** | Reads and displays the current OTP configuration from the connected device |

---

## 🔧 Features

- **Live DFU status** — auto-detects board connection every 3 seconds
- **Interactive tooltips** — hover over any field label for a detailed explanation
- **Input validation** — Device Name limited to 8 ASCII chars, Board Version to 0–255
- **Custom dark UI** — Windows 11 Acrylic glass effect with Flipper Orange accents
- **Built-in warnings** — confirmation dialog before any irreversible OTP write
- **Clean error logs** — CLI progress bar artifacts are filtered from error messages

---

## 📐 OTP Memory Layout (Address: `0x1FFF7000`)

The tool writes a 32-byte OTP v2 structure:

```
Offset  Size  Field
------  ----  -----
0x00    2     Header Magic (0xBABE)
0x02    1     Header Version (2)
0x03    1     Reserved
0x04    4     Timestamp (Unix epoch)
0x08    1     Board Version
0x09    1     Board Target (7)
0x0A    1     Board Body (9)
0x0B    1     Board Connect (6)
0x0C    1     Display Type (1=ERC, 2=MGG)
0x0D    1     Reserved
0x0E    2     Reserved
0x10    1     Body Color (1=Black, 2=White, 3=Transparent)
0x11    1     Region (1=EU, 2=US, 3=JP, 4=World)
0x12    2     Reserved
0x14    4     Reserved
0x18    8     Device Name (ASCII, null-padded)
```

---

## 🛠️ Building from Source

To compile the `.exe` from the Python source:

```bash
pyinstaller --onefile --noconsole \
    --icon=flipper.ico \
    --add-data "flipper.ico;." \
    generate_otp_gui.py
```

Requires: Python 3.11+, PyInstaller, tkinter.

---

## 📄 License

This project is licensed under the [GNU General Public License v3.0](https://github.com/artema0g/oled_flipper/blob/main/LICENSE).
