# ⚙️ DIY Flipper Zero (OLED Version)
Custom firmware fork supporting standard I2C OLED screens (SH1106 and SSD1306) on DIY Flipper hardware.

[![FBT Build](https://img.shields.io/badge/build-FBT-blue.svg)](https://github.com/artema0g/oled_flipper)
[![Platform](https://img.shields.io/badge/platform-STM32WB55-orange.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32wb-series.html)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-Support%20Me-red?style=flat&logo=kofi)](https://ko-fi.com/artema0g)
[![License](https://img.shields.io/badge/license-GPL--3.0-green.svg)](LICENSE)

> [!WARNING]
> I do not take responsibility if you damage your board or property. This guide is for educational purposes only — proceed at your own risk.

> [!TIP]
> ❓ Need help or have questions about building/flashing the DIY Flipper? 
> Join our community Q&A and troubleshooting discussion: **[GitHub Q&A Discussion #4](https://github.com/artema0g/oled_flipper/discussions/4)**

---

## <a id="hardware-showcase"></a>📷 Hardware Showcase

Here is the physical DIY board in action:

<p align="center">
  <img src="mics/IMG_20260201_143415.JPG" width="45%" alt="DIY Flipper Front" />
  <img src="mics/IMG_20260201_161815.JPG" width="45%" alt="DIY Flipper Angle" />
</p>

---

## <a id="table-of-contents"></a>📚 Table of Contents
- [Summary](#summary)
- [System Architecture](#system-architecture)
- [What Works and Limitations](#what-works-and-limitations)
- [Key Pins and Wiring](#key-pins-and-wiring)
- [PCF8574 Wiring Guide](#pcf8574-wiring-guide)
- [How to Build and Flash](#how-to-build-and-flash)
- [Schematic](#schematic)
- [Credits and Support](#credits-and-support)

---

## <a id="summary"></a>🔍 Summary
This project implements a custom target for a DIY Flipper-style board based on the **WeAct STM32WB55CGU6** board. It integrates the following components:

*   **Display**: I2C OLED display (SH1106 / SSD1306)
*   **Sensors**: INA219 / INA226 power & battery monitor (I2C) with hardware Alert (PB1)
*   **I/O Expander**: PCF8574 (handles buttons and the vibration motor)
*   **Storage**: microSD slot (SPI)
*   **Radio**: CC1101 sub-GHz module (SPI)
*   **NFC**: ST25R3916 Elechouse module (SPI)
*   **LF-RFID (125 kHz)**: Antenna coil driver & envelope detector (PA5 Carrier TX / PA1 Data RX)
*   **Peripherals**: Speaker/buzzer, IR transmitter/receiver, vibration motor

---

## <a id="system-architecture"></a>📐 System Architecture

This diagram visualizes how the different components interface with the STM32WB55 MCU over I2C, SPI, and GPIO.

```mermaid
graph TD
    subgraph MCU [STM32WB55CGU6]
        I2C1[I2C1 Bus]
        SPI1[SPI1 Bus]
        GPIO[Direct GPIO]
    end

    %% I2C Bus Devices
    I2C1 --> OLED[OLED Display <br> SH1106 / SSD1306]
    I2C1 --> INA[INA219 / INA226 <br> Battery Monitor]
    I2C1 --> PCF[PCF8574 <br> I/O Expander]

    %% PCF8574 Expander
    PCF --> Buttons[6-Way Buttons + Back]
    PCF --> Vibro[Vibration Motor]

    %% SPI Bus Devices
    SPI1 --> SD[MicroSD Card CS: PA10]
    SPI1 --> CC1101[CC1101 Radio CS: PA15]
    SPI1 --> NFC[ST25R3916 NFC CS: PE4]

    %% GPIO
    GPIO --> IR_RX[IR Receiver PA0]
    GPIO --> IR_TX[IR Transmitter PA8]
    GPIO --> Speaker[Speaker PB8]
    GPIO --> OneWire[1-Wire iButton PA3]
    GPIO --> RFID_TX[LF-RFID TX PA5]
    GPIO --> RFID_RX[LF-RFID RX PA1]
```

---

## <a id="what-works-and-limitations"></a>✅ What Works and Limitations
*   **Core Systems**: All official Flipper firmware features compile and function.
*   **I2C Devices**: OLED, INA219 / INA226, and PCF8574 are multiplexed onto the primary I2C1 bus to preserve SPI resources.
*   **Power Monitoring**: Automatic dual INA219 / INA226 detection with hardware overcurrent/undervoltage Alert interrupt on PB1.
*   **NFC Support**: Verified working with Elechouse ST25R3916 modules.
*   **LF-RFID (125 kHz)**: Reading, writing, and emulation verified for EM4100, HID Generic, Indala26, Keri, NexWatch, Noralsy, Viking, and IDTeck.
*   **Sub-GHz**: CC1101 module tested and fully functional.

---

## <a id="key-pins-and-wiring"></a>📌 Key Pins and Wiring

| Component | Bus / Interface | MCU pin (macro) | Notes |
|---|---|---|---|
| **I2C1 (Power/Default)** | I2C | SCL: PA9, SDA: PB9 | Used by INA219, PCF8574, and OLED |
| **I2C3 (External)** | I2C | SCL: PC0, SDA: PC1 | Used by PN532 NFC module (firmware default) |
| **SPI1 (Shared)** | SPI | MOSI: PB5, SCK: PB3 | Shared SCK/MOSI bus for CC1101, NFC, and SD card |
| **CC1101** | SPI + IRQ | CS: PA15, MISO: PA6, G0: PA1 | Sub-GHz transceiver |
| **SD card** | SPI | CS: PA10, MISO: PA6 | MicroSD module |
| **NFC (PN532 V3)** | I2C + IRQ | SCL: PC0, SDA: PC1, IRQ: PA2 | PN532 NFC module at I2C address `0x24` (7-bit). Active-low IRQ. |
| **PCF8574 Interrupt** | GPIO | INT: PB0 | Signals button state changes |
| **IR** | GPIO | RX: PA0, TX: PA8 | Safe IR transmitter & receiver |
| **LF-RFID (125 kHz)** | PWM / Timer | TX Carrier: PA5 (TIM2_CH1), RX Data: PA1 | 125 kHz coil driver transistor + envelope demodulator |
| **Speaker** | PWM | PB8 (TIM16) | Sound buzzer |
| **iButton** | 1-Wire | PA3 | Dallas 1-Wire keys |

---

## <a id="pcf8574-wiring-guide"></a>🎛️ PCF8574 Wiring Guide

The PCF8574 I/O expander (I2C address `0x20` by default) handles all buttons and the haptic feedback motor over the I2C1 bus. Buttons are wired in an **active-low** configuration (connecting the pin to GND when pressed).

*   **Button Inputs (active-low)**:
    *   `P0` -> Up Button
    *   `P1` -> Down Button
    *   `P2` -> Left Button
    *   `P3` -> Right Button
    *   `P4` -> OK (Select) Button
    *   `P5` -> Back Button
*   **Outputs**:
    *   `P6` -> Haptic Vibration Motor (Use an N-channel MOSFET; do not drive directly!)

The `INT` output (open-drain, connect to MCU **PB0** with a pull-up) signals button state changes and wakes the input service. Note that the expander's I2C address `0x20` overlaps the INA219/INA226 monitor at `0x40` (wire address) — the driver probes the chip with a write/read-back test so the two devices are never confused.

---

## <a id="how-to-build-and-flash"></a>🛠️ How to Build and Flash

### 1. Build from Source
To compile the firmware for the OLED hardware target, use the Flipper Build Tool.

> [!IMPORTANT]
> **This fork has no `firmware` build target** — running `fbt firmware` (or `fbt.cmd firmware`) fails with a *"target not built"* error. The default target (`basic_dist`) builds the complete firmware package, so invoke `fbt` with **no arguments**:

*   **Windows (Git Bash / MinGW)**: the `./fbt` shell script refuses to run under MinGW ("In MinGW shell, use fbt.cmd instead"). Use the batch launcher:
    ```bash
    # Build the full firmware package (default target: basic_dist)
    cmd //c fbt.cmd
    ```
*   **Linux / macOS**: the POSIX launcher works directly:
    ```bash
    # Build the full firmware package (default target: basic_dist)
    ./fbt
    ```

Both produce the flashable firmware (`.bin` / `.dfu` / `.hex`) under `build/f7-firmware-C/` and `dist/`.

To build only a single external app (FAP), use its `fap_<appid>` target, e.g.:
```bash
cmd //c fbt.cmd fap_signal_generator
```

**Updater & update bundle:** the standalone updater and the qFlipper update package are **not** part of the default build — they must be requested explicitly with `--with-updater`:

```bash
# Build firmware + standalone updater (build/f7-updater-C/updater.elf/.bin/.dfu)
cmd //c fbt.cmd --with-updater

# Build the full self-update bundle + qFlipper .tgz package + SDK zip
cmd //c fbt.cmd --with-updater updater_package
```

The bundle lands in `dist/f7-C/f7-update-v2.1/` (`update.fuf` + `resources.tar.gz`) with the qFlipper installer at `dist/f7-C/flipper-z-f7-update-v2.1.tgz`.

### 2. Configure & Flash OTP (One-Time Programmable) Memory
> [!CAUTION]
> OTP memory can only be written **ONCE**. It cannot be erased or changed. Proceed at your own risk.

1. Open **`generate_otp_gui.exe`** (found in the [`mics/FlipperOTP/`](mics/FlipperOTP/) folder). No installation required.
2. Set your **Device Name** (max 8 ASCII characters) and **Board Version** (`12` for WeAct STM32WB55).
3. Select **Display Type: MGG** (Monochrome Glass Grid) — required for the SSD1306 OLED screen.
4. Put the MCU into **DFU mode**: hold `BOOT0`, connect USB, release `BOOT0`. The app status will show **🟢 Connected**.
5. Click **"2. Flash (DFU)"** — the tool writes OTP directly to `0x1FFF7000`.

> [!TIP]
> Alternatively, click **"1. Save .bin"** to generate the file, then flash it manually via [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html).

### 3. Flash Firmware

Choose the appropriate method depending on whether you are setting up the board for the first time or just updating the firmware.

---

#### Method A: First-Time Setup (or after a Full Flash Erase)
*Use this method if the board is blank, has no bootloader, or was fully erased.*

1. Put the board in DFU mode (hold `BOOT0`, connect USB, release `BOOT0`).
2. Open the **qFlipper** desktop application.
3. qFlipper will detect the board and display **"RECOVERY MODE"** (or **"Update & Recovery Mode DFU started"**).
4. Click the **"REPAIR"** button. qFlipper will automatically restore the official bootloader and partition the internal Flash.
5. Once the recovery is complete, the screen might remain blank (since the official firmware lacks OLED drivers), but the device will now connect to qFlipper.
6. Now, put the board back into **DFU mode** again (so it doesn't freeze under the official firmware).
7. Click **"Install from file"** in qFlipper.
8. Choose your preferred update style:
   * **Using the `.tgz` package (Recommended):** Select the **`.tgz`** archive. qFlipper will flash the firmware (the OLED screen will turn on), reboot into normal mode, and then copy the required resource files to your SD card automatically.
   * **Using the `.dfu` file (Manual):** Select the **`.dfu`** firmware file to flash it (OLED turns on). After it boots, you will need to manually unzip the resources package and copy the files to the root of your microSD card.

---

#### Method B: Regular Firmware Updates
*Use this method to update an already working DIY Flipper.*

* **Using the `.tgz` package:** Connect the Flipper to your PC via USB, open **qFlipper**, click **"Install from file"**, and select the updated **`.tgz`** archive. qFlipper will automatically flash the MCU and update all SD card files in one go.
* **Using the `.dfu` file:** Put the board in **DFU mode**, open qFlipper, click **"Install from file"**, and select the updated **`.dfu`** firmware file.

---

#### Option C: Flashing via STM32CubeProgrammer / ST-Link (Advanced)
> [!WARNING]
> **DO NOT use "Full Chip Erase"** in STM32CubeProgrammer!
> Doing a full chip erase will wipe out the emulation OTP structures, flash partition tables, and calibration settings. If these are wiped, the firmware will freeze early during boot (leading to a blank screen and USB connection loss).
> 
> *   Set the Erase option to **"Sector Erase"** (only erase sectors occupied by the firmware).
> *   If you did a full chip erase by mistake, follow **Method A (First-Time Setup)** to rebuild the device partitions first.

---

## <a id="schematic"></a>🔌 Schematic & Hardware Circuit

A complete wiring schematic is available in the repository:

![DIY Flipper Schematic](misc/schematic.png)

### 📻 LF-RFID (125 kHz) Circuit Details & Schematic:
- 📄 **Schematic PDF**: [Download 125kHz LF-RFID Subsystem Schematic (PDF)](misc/rfid_lf.pdf)

#### Component Bill of Materials (BOM) from KiCad Schematic:
| Stage | Component | Value / Part | Description |
|---|---|---|---|
| **Transmitter Driver [1]** | `PA5` (PWM) | MCU `TIM2_CH1` | 125 kHz Carrier PWM Drive |
| | `Q1` | BC337 / S8050 / 2N2222 | NPN Push-Pull Transistor |
| | `Q2` | BC327 / S8550 / 2N2907 | PNP Push-Pull Transistor |
| | `C1` | 2.2 nF | Drive Coupling Capacitor |
| **Resonant Tank [2]** | `L1` | 1.2 mH / 95T | Antenna Coil |
| | `PA2` (Emulate) | MCU `TIM2_CH3` | Emulation Pulse Driver (`R2` 1k, `R8` 10k) |
| | `Q3` | 2N2222 | Emulation Switch Transistor (`R1` 100 Ohm) |
| **Demodulator [3]** | `D1` | 1N4148 / BAT54S | Envelope Schottky Diode |
| | `C3`, `R3` | 1 nF, 10 kOhm | RC Low-Pass Filter |
| | `C4`, `R4` | 22 nF, 10 kOhm | AC Coupling Stage |
| | `U1` | LM2904 / LM358 / MCP6002 | Op-Amp Signal Amplifier (`R5`-`R7` 100k, `R9` 50k) |
| | `PA1` (Data In) | MCU `TIM1_CH1` | Demodulated RX Envelope Input |

---

## <a id="credits-and-support"></a>🤝 Credits and Support

Special thanks to:
*   **Nucleus Dark** & **Lamtran** for their design inspiration and code contributions.

### ☕ Support this Project
If you find this project useful and would like to support its development, you can buy me a coffee here:
*   **Ko-fi**: [Support artema0g on Ko-fi](https://ko-fi.com/artema0g)
