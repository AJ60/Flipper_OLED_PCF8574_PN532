# 🐬 DIY Flipper Zero (OLED Edition)

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

> Build your own DIY Flipper Zero with an I2C OLED display, PCF8574 keypad, PN532 NFC reader, CC1101 sub-GHz radio, and discrete 125 kHz RFID hardware!

[![CI Build](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/actions/workflows/build.yml/badge.svg)](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/actions/workflows/build.yml)
[![FBT Build](https://img.shields.io/badge/build-FBT-blue.svg)](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)
[![Platform](https://img.shields.io/badge/platform-STM32WB55-orange.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32wb-series.html)
[![Maintainer](https://img.shields.io/badge/maintainer-AJ__60-brightgreen.svg)](https://github.com/AJ60)
[![License](https://img.shields.io/badge/license-GPL--3.0-green.svg)](LICENSE)

> [!CAUTION]
> ⚖️ **LEGAL & EDUCATIONAL DISCLAIMER**:  
> This project and firmware are created strictly for **educational, academic research, and authorized security testing purposes only**. 
> - **DO NOT** use this firmware or hardware for unauthorized access, card cloning, or malicious activities.
> - The developers and contributors assume **no liability or responsibility** for any misuse, damage to property, or illegal actions committed using this software or hardware.

> [!WARNING]
> 🚧 **DEVELOPMENT STATUS NOTICE**:
> - **NFC Subsystem (PN532)**: **Under Active Development / Experimental.** Currently verified with **MIFARE Classic 1K/4K tags**, **NTAG series (NTAG213/215/216/Ultralight)**, and **EMV ATM / Bank payment cards** (ISO 14443-4 APDU reading).
> - **125 kHz LF-RFID Subsystem**: **Under Active Development / Experimental.** May contain bugs due to discrete analog hardware tolerances, coil inductance variance, and signal demodulation thresholds.

---

## 🧩 The Building Blocks (Hardware Modules)

Your DIY Flipper is built from standard, readily available breakout modules:

![DIY Flipper Component Guide](misc/module_overview.jpg)

```mermaid
graph TD
    subgraph Core [Core Hardware]
        MCU["🧠 The Brain<br><b>WeAct STM32WB55</b>"]
        OLED["👀 The Eyes<br><b>0.96 / 1.3 inch I2C OLED</b>"]
        KEYPAD["🎮 The Hands<br><b>PCF8574 + 5-Way Joystick</b>"]
    end
    
    subgraph Radios [Wireless & Sensors]
        NFC["💳 Keycard Reader<br><b>PN532 NFC Module</b>"]
        RADIO["📻 Sub-GHz Radio<br><b>CC1101 Transceiver</b>"]
        RFID["🏷️ Key Fob Reader<br><b>125kHz Discrete Tank</b>"]
    end
    
    subgraph StoragePower [Storage, Sound & Power]
        SD["💾 The Backpack<br><b>MicroSD SPI Module</b>"]
        BUZZER["🔊 The Voice<br><b>Piezo Buzzer (PB8)</b>"]
        INA["🔋 Fuel Gauge<br><b>INA219 / INA226</b>"]
        PWR_CHAIN["⚡ Power Management<br><b>TP4056 + LiPo + 5V Boost</b>"]
    end

    MCU --- OLED
    MCU --- KEYPAD
    MCU --- NFC
    MCU --- RADIO
    MCU --- RFID
    MCU --- SD
    MCU --- BUZZER
    MCU --- INA
    INA --- PWR_CHAIN
```

| # | Part | Nickname | What It Does (In Simple Words) |
|:---:|---|---|---|
| **1** | **WeAct STM32WB55** | 🧠 **The Brain** | Fast dual-core chip running Furi OS, FreeRTOS, games, and apps. |
| **2** | **SSD1306 / SH1106 OLED** | 👀 **The Eyes** | Shows animations, dolphin pet, menus, and frequency graphs (0.96" or 1.3"). |
| **3** | **PCF8574 Expander** | 🎮 **The Hands** | Connects 6 direction/action buttons + haptic vibration rumble motor. |
| **4** | **PN532 NFC Module** | 💳 **Keycard Reader** | Reads 13.56 MHz NFC (MIFARE Classic, NTAG, Bank Cards) on dedicated I2C3. |
| **5** | **CC1101 Radio** | 📻 **The Antenna** | Transmits and sniffs sub-GHz radio signals (gates, remotes, sensors). |
| **6** | **MicroSD Card Module** | 💾 **The Backpack** | Stores your saved keys, remotes, scripts, games, and animations. |
| **7** | **Passive Buzzer** | 🔊 **The Voice** | Plays fun 8-bit chimes, game sounds, and keypress clicks via timer PWM. |
| **8** | **INA219 / INA226** | 🔋 **Fuel Gauge** | High-side current/voltage sensor for battery curve & charging animations. |

---

## 🔌 Complete Wiring & Electrical Guide

Connecting the hardware is divided into four straightforward subsystems:

```mermaid
graph LR
    subgraph Bus_Architecture [Bus Routing Architecture]
        I2C1["I2C1 Power Bus (PA9 SCL / PB9 SDA)<br>400 kHz"]
        I2C3["I2C3 Dedicated NFC Bus (PA7 SCL / PB4 SDA)<br>400 kHz"]
        SPI1["SPI1 High-Speed Bus (PB3 SCK / PB5 MOSI / PA6 MISO)<br>up to 32 MHz"]
        TIMERS["Dedicated Timers & GPIOs<br>PB8 (Buzzer) / PA8 (IR TX) / PA0 (IR RX)"]
    end

    I2C1 --> OLED["OLED Display (0x3C)"]
    I2C1 --> PCF["PCF8574 Keypad (0x20)"]
    I2C1 --> INA["INA219 Fuel Gauge (0x40)"]
    
    I2C3 --> PN532["PN532 NFC Module (0x24)"]
    
    SPI1 --> SD["MicroSD Card (CS: PA10)"]
    SPI1 --> CC1101["CC1101 Radio (CS: PA15)"]
```

---

### Part 1: 🖼️ Core Subsystem (Cirkit Designer Schematic)

Follow the authoritative wiring schematic below for the core human-interface peripherals:

![DIY Flipper Zero Core Circuit Diagram (Cirkit Designer)](./misc/cirkit_wiring_diagram.png)

#### 📋 Core Subsystem Pin-to-Pin Table

| Component | Pin / Terminal | Connects to MCU / Destination | Wire Color (Diagram) | Function / Notes |
|---|---|---|:---:|---|
| **1.3" / 0.96" OLED** | `VCC` | **3.3V Rail** | 🔴 Red | Display Power (3.3V) |
| | `GND` | **GND Rail** | ⚫ Black | Ground |
| | `SCL` | **PA9** | 🟡 Yellow | I2C1 Clock (400 kHz) |
| | `SDA` | **PB9** | 🔵 Teal / Blue | I2C1 Data (400 kHz) |
| **PCF8574 I2C Expander** | `VCC` | **3.3V Rail** | 🔴 Red | Expander Power (3.3V) |
| | `GND` | **GND Rail** | ⚫ Black | Ground |
| | `SCL` | **PA9** | 🟡 Yellow | Shared I2C1 Clock |
| | `SDA` | **PB9** | 🔵 Teal / Blue | Shared I2C1 Data |
| | `INT` | **PB0** | 🟢 Dark Teal | Hardware Interrupt (`EXTI0`, active-low) |
| | `A0, A1, A2` | **GND** | — | Sets hardware I2C address to `0x20` |
| **5-Way Joystick & Buttons** | `UP` | **PCF8574 P0** | 🔵 Cyan | Up Navigation Button |
| | `DWN` | **PCF8574 P1** | 🟠 Orange | Down Navigation Button |
| | `LFT` | **PCF8574 P2** | 🟣 Purple | Left Navigation Button |
| | `RHT` | **PCF8574 P3** | 🟤 Brown | Right Navigation Button |
| | `MID` (Center Press)| **PCF8574 P4** | 🟢 Green | OK / Select Button |
| | `SET` / `RST` (Back) | **PCF8574 P5** | ⚪ Gray | Back Button |
| | `COM` | **GND Rail** | ⚫ Black | Common Ground (active-low switches) |
| **Vibration Motor Module** | `VCC` | **3.3V Rail** | 🔴 Red | Motor Power (via onboard driver) |
| | `GND` | **GND Rail** | ⚫ Black | Ground |
| | `IN` | **PCF8574 P6** | 🟣 Violet | Haptic Rumble Trigger (Active-High) |
| **Passive Piezo Buzzer** | Positive `+` | **PB8** | 🟣 Dark Purple | TIM16_CH1 Hardware Audio PWM |
| | Negative `-` | **GND Rail** | ⚫ Black | Ground Return |
| **IR Transmitter (TX)** | `VCC` | **3.3V / 5V Rail** | 🔴 Red | Transmitter Power |
| | `GND` | **GND Rail** | ⚫ Black | Ground |
| | `SIG / IN` | **PA8** | 🌸 Light Pink | TIM1_CH1 38 kHz Modulated Carrier |
| **IR Receiver (RX)** | `VCC` | **3.3V Rail** | 🔴 Red | 38 kHz TSOP Receiver Power |
| | `GND` | **GND Rail** | ⚫ Black | Ground |
| | `OUT / DATA` | **PA0** | 🟣 Purple | TIM2_CH1 Demodulated Signal Input |
| **Decoupling Capacitors** | `10 µF` (Electrolytic) | **3.3V ➔ GND** | — | Bulk Power Rail Filter (absorbs current spikes) |
| | `100 nF` (Ceramic) | Across VCC/GND of each module | — | High-frequency noise suppression |

> [!IMPORTANT]
> **I2C Pull-Up Resistors**: Standard I2C OLED boards have onboard 4.7 kΩ pull-up resistors on SDA/SCL. Keep the OLED connected so the I2C1 bus stays pulled up during boot!

---

### Part 2: 📻 Wireless, NFC & Storage Subsystem

The MicroSD card and internal CC1101 sub-GHz radio share the high-speed **SPI1** bus, while the PN532 NFC reader is isolated on hardware **I2C3** to prevent bus contention:

#### 📋 Onboard Peripherals Pin Table
| Module | Module Pin | Connects to MCU Pin | Purpose / Notes |
|---|---|---|---|
| **Shared SPI1 Clock** | `SCK` | **PB3** | SPI1 Clock (up to 32 MHz) |
| **Shared SPI1 MOSI** | `MOSI` | **PB5** (Header "A7") | Data Out from MCU |
| **Shared SPI1 MISO** | `MISO` | **PA6** (Header "A6") | Data In to MCU |
| **MicroSD Card Module** | `CS` | **PA10** | SD Card Chip Select (Active-Low) |
| | `CD` (Detect) | *Not Connected (NC)* | Optional (firmware detects automatically via software) |
| **Internal CC1101 Radio** | `CSN / CS` | **PA15** | Internal Radio Chip Select (Active-Low) |
| | `GDO0 / G0` | **PA1** | Internal Radio Demodulated Data IRQ (`EXTI1`) |
| | `VCC` / `GND` | **3.3V** / **GND** | **3.3V only!** (5V will permanently damage the CC1101) |
| **PN532 NFC Module** | `SCL` | **PA7** (Header "C0") | Dedicated I2C3 Clock (400 kHz) |
| | `SDA` | **PB4** (Header "C1") | Dedicated I2C3 Data (400 kHz) |
| | `IRQ` | **PA2** (Header "A2") | Card Detection Interrupt (`EXTI2`, Active-Low) |
| **1-Wire / iButton** | `Data` | **PA3** (Header "A3") | Dallas iButton probe with external 4.7 kΩ pull-up |
| **LF-RFID (125 kHz)** | Carrier TX | **PA5** (Header "C3"/"A5") | TIM2_CH1 coil push-pull driver stage *(Experimental)* |
| | Envelope RX | **PA1** | TIM1_CH1 demodulated envelope input *(Experimental)* |
| | Emulate | **PA2** | TIM2_CH3 tag emulation pulse switch *(Experimental)* |

---

### 🌐 Flipper-Compatible 18-Pin Expansion Header

The DIY Flipper Zero exposes a pinout compatible with the official Flipper Zero 18-pin GPIO expansion header, allowing plug-and-play connection of external CC1101 modules, NRF24L01+ transceivers, GPS, ESP32/ESP8266 Wi-Fi boards, and sensors:

| Flipper Pin # | Flipper Label | Actual MCU Pin | WeAct Silkscreen Label | Direction | Primary Function & Peripherals |
|:---:|:---:|:---:|:---:|:---:|---|
| **1** | **5V** | `5V` (VBUS/Boost) | **`5V`** | Power OUT/IN | 5.0V DC regulated rail (from USB-C or boost converter) |
| **2** | **A7** | `PB5` | **`A7`** *(MCU PB5)* | Output | **SPI1 MOSI** (Shared high-speed bus data output) |
| **3** | **A6** | `PA6` | **`A6`** | Input | **SPI1 MISO** (Shared high-speed bus data input) |
| **4** | **A4** | `PA4` | **`A4`** | Output | **External SPI1 CS / CSN** (Active-Low chip select) |
| **5** | **B3** | `PB3` | **`B3`** | Output | **SPI1 SCK** (Shared high-speed SPI clock, up to 32 MHz) |
| **6** | **B2** | `PB2` | **`B2`** | In / Out | **Ext CC1101 GDO0 IRQ** / **NRF24 CE** / General GPIO |
| **7** | **C3** | `PA5` | **`C3`** *(Board `A5`)* | Output | **E07 CC1101 TXEN** / LF-RFID 125 kHz Carrier PWM |
| **8** | **GND** | `GND` | **`G` / `GND`** | Ground | Common system ground reference |
| **9** | **3V3** | `3.3V` | **`3V3`** | Power OUT | 3.3V DC regulated rail (from onboard LDO) |
| **10** | **SWC** | `PA14` | **`SWC`** | Input | SWD Hardware Clock (for ST-Link debugging) |
| **11** | **GND** | `GND` | **`G` / `GND`** | Ground | Common system ground reference |
| **12** | **SWD** | `PA13` | **`SWD`** | Bidirectional | SWD Hardware Data (for ST-Link debugging) |
| **13** | **TX** | `PB6` | **`B6`** | Output | USART1 TX (Debug console / CLI serial) |
| **14** | **RX** | `PB7` | **`B7`** | Input | USART1 RX (Debug console / CLI serial) |
| **15** | **C1** | `PB4` | **`C1`** *(MCU PB4)* | Bidirectional | **Dedicated I2C3 SDA** (NXP PN532 NFC Data line) |
| **16** | **C0** | `PA7` | **`C0`** *(MCU PA7)* | Bidirectional | **Dedicated I2C3 SCL** (NXP PN532 NFC Clock line) |
| **17** | **iBtn** | `PA3` | **`A3`** | Bidirectional | **1-Wire iButton Probe** (requires external 4.7 kΩ pull-up) |
| **18** | **GND** | `GND` | **`G` / `GND`** | Ground | Common system ground reference |

> [!IMPORTANT]
> **WeAct Header Label vs MCU Pin Mapping Quirks**:
> On the WeAct STM32WB55CGU6 core board, several header silkscreen labels differ from their physical STM32 GPIO mappings:
> - Header pin labeled **`A7`** is physically routed to MCU pin **`PB5`** (`SPI1 MOSI`).
> - Header pin labeled **`C0`** is physically routed to MCU pin **`PA7`** (`I2C3 SCL`).
> - Header pin labeled **`C1`** is physically routed to MCU pin **`PB4`** (`I2C3 SDA`).
> - Header pin labeled **`C3`** (or board **`A5`**) is physically routed to MCU pin **`PA5`** (`E07 TXEN`).

---

### 📡 External CC1101 & High-Power Ebyte E07 Modules

The firmware provides full, first-class support for external Sub-GHz CC1101 modules, including high-power **Ebyte E07-433M20S** and **E07-900M20S** transceivers equipped with +20 dBm (100 mW) power amplifiers (PA) and low-noise amplifiers (LNA).

```text
  WeAct STM32WB55 (Expansion Header)             Ebyte E07-433M20S Module
  ┌────────────────────────────┐                  ┌────────────────────────┐
  │ Pin 9 (3.3V Rail)          ├─────[ 3.3V ]─────┤ VCC (3.3V DC)          │
  │ Pin 8 (GND Rail)           ├─────[ GND ]──────┤ GND                    │
  │ Pin 4 (PA4 - Ext SPI CS)   ├─────[ CSN ]──────┤ CSN (Chip Select)      │
  │ Pin 5 (PB3 - SPI1 SCK)     ├─────[ SCK ]──────┤ SCK (SPI Clock)        │
  │ Pin 2 (PB5 - SPI1 MOSI)    ├─────[ MOSI ]─────┤ MOSI (Data In)         │
  │ Pin 3 (PA6 - SPI1 MISO)    ├─────[ MISO ]─────┤ MISO (Data Out)        │
  │ Pin 6 (PB2 - Ext GDO0)     ├─────[ GDO0 ]─────┤ GDO0 (Demod Data IRQ)  │
  │ Onboard CC1101 GDO2        │═════[ RXEN ]═════│ GDO2 (Internal Switch) │
  │ Pin 7 (PA5 - Header C3/A5) ├─────[ TXEN ]─────┤ TXEN (PA Amp Enable)   │
  └────────────────────────────┘                  └────────────────────────┘
```

#### Ebyte E07 Connection Table:
| Header Pin # | MCU Pin | E07 Module Pin | Description & Hardware Function |
|:---:|:---:|:---:|---|
| **Pin 9** | `3.3V` | `VCC` | 3.3V power (do NOT connect to 5V; peak current during TX is ~120 mA) |
| **Pin 8** | `GND` | `GND` | Common ground reference |
| **Pin 4** | `PA4` | `CSN` | Dedicated External SPI1 Chip Select (`furi_hal_spi_bus_handle_external_cc1101`) |
| **Pin 5** | `PB3` | `SCK` | SPI1 Clock (up to 32 MHz) |
| **Pin 2** | `PB5` (Silkscreen "A7") | `MOSI` | SPI1 Master-Out Slave-In |
| **Pin 3** | `PA6` | `MISO` | SPI1 Master-In Slave-Out |
| **Pin 6** | `PB2` | `GDO0` | Async sample capture interrupt line (`EXTI2`) |
| *Internal* | *CC1101 GDO2* | `RXEN` | Controlled via `IOCFG2`: driven HIGH (`0x2F`) in RX, driven LOW (`0x2E`) in TX/idle |
| **Pin 7** | `PA5` (Silkscreen "C3") | `TXEN` | Driven HIGH (`1`) during active TX, parked LOW (`0`) in idle/sleep/RX |

#### How to Switch Between Internal and External Radio:
1. Open **Settings > Radio Settings** (or **Momentum > Radio Settings**).
2. Set **Sub-GHz Radio** to **`Internal`** or **`External`**.
3. The selection is automatically saved to `/int/radio.settings` (v4 format) and persists across reboots.
4. All Sub-GHz features — including Frequency Analyzer, Read, Transmit, and FAP apps like **Sub-GHz Bruteforcer** — automatically route commands to the active radio device without requiring separate app builds!
5. 📖 **Full Engineering Guide**: See [**External CC1101 Technical Reference**](documentation/EXTERNAL_CC1101.md).

---

### 📶 Nordic NRF24L01+ 2.4 GHz Module

The NRF24L01+ module enables 2.4 GHz sniffing, packet injection, BLE beaconing, and MouseJacker attacks:

```text
  WeAct STM32WB55 (Expansion Header)             Nordic NRF24L01+ Transceiver
  ┌────────────────────────────┐                  ┌────────────────────────┐
  │ Pin 9 (3.3V Rail)          ├─────[ 3.3V ]─────┤ Pin 2: VCC (3.3V ONLY!)│
  │ Pin 8 (GND Rail)           ├─────[ GND ]──────┤ Pin 1: GND             │
  │ Pin 4 (PA4 - Ext SPI CS)   ├─────[ CSN ]──────┤ Pin 4: CSN (SPI CS)    │
  │ Pin 6 (PB2 - GPIO)         ├─────[ CE ]───────┤ Pin 3: CE (Chip Enable)│
  │ Pin 5 (PB3 - SPI1 SCK)     ├─────[ SCK ]──────┤ Pin 5: SCK (SPI Clock) │
  │ Pin 2 (PB5 - SPI1 MOSI)    ├─────[ MOSI ]─────┤ Pin 6: MOSI (Data In)  │
  │ Pin 3 (PA6 - SPI1 MISO)    ├─────[ MISO ]─────┤ Pin 7: MISO (Data Out) │
  │ *(Unconnected / Optional)* │                  │ Pin 8: IRQ (Interrupt) │
  └────────────────────────────┘                  └────────────────────────┘
```

#### NRF24 Connection Table:
| Header Pin # | MCU Pin | NRF24 Pin # | Pin Name | Description / Requirements |
|:---:|:---:|:---:|:---:|---|
| **Pin 9** | `3.3V` | **Pin 2** | `VCC` | **3.3V ONLY**. Never connect to 5V! |
| **Pin 8** | `GND` | **Pin 1** | `GND` | Common ground reference |
| **Pin 4** | `PA4` | **Pin 4** | `CSN` | SPI Chip Select (Active-Low) |
| **Pin 6** | `PB2` | **Pin 3** | `CE` | Chip Enable (Active-High RX/TX enable) |
| **Pin 5** | `PB3` | **Pin 5** | `SCK` | SPI1 Clock |
| **Pin 2** | `PB5` (Silkscreen "A7") | **Pin 6** | `MOSI` | SPI1 Master-Out Slave-In |
| **Pin 3** | `PA6` | **Pin 7** | `MISO` | SPI1 Master-In Slave-Out |

> [!CAUTION]
> **Power Decoupling Capacitor is Mandatory**:
> NRF24L01+ modules suffer from extreme RF supply ripple and current transients during transmission bursts. You **MUST solder a 10 µF electrolytic capacitor + a 100 nF ceramic capacitor** directly across `VCC` (Pin 2) and `GND` (Pin 1) of the NRF24 module. Without this capacitor, the module will freeze, fail to transmit, or cause MCU brownouts.
> 📖 **Full Engineering Guide**: See [**NRF24 Transceiver Guide**](documentation/NRF24.md).

---

### 💳 NXP PN532 NFC Subsystem (Dedicated I2C3)

The PN532 operates over hardware **I2C3**, completely isolated from the display and keypad I2C1 bus:

| PN532 Pin | Connects to | MCU Pin | Function / Description |
|---|---|---|---|
| **VCC** | **3.3V Rail** | `3.3V` | Logic power (3.3V only) |
| **GND** | **GND Rail** | `GND` | System ground reference |
| **SCL** | **Pin 16** (Header "C0") | `PA7` | Dedicated I2C3 Clock (400 kHz) |
| **SDA** | **Pin 15** (Header "C1") | `PB4` | Dedicated I2C3 Data (400 kHz) |
| **IRQ** | **Header Pin "A2"** | `PA2` | Card Detect Interrupt (`EXTI2`, Active-Low) |

#### Hardware DIP Switch Configuration (I2C Mode):
| Switch | Setting | Description |
|:---:|:---:|---|
| **I0** (or `CH1`) | **OFF (`0`)** | Selects I2C protocol mode |
| **I1** (or `CH2`) | **ON (`1`)** | Selects I2C protocol mode |

- **Features Supported**:
  - **MIFARE Classic 1K / 4K**: Hardware Crypto1 cipher authentication (`InAuth 0x40`) with dictionary attack recovery.
  - **NTAG Series (213 / 215 / 216 / Ultralight)**: Direct page reading and payload dumping.
  - **Contactless EMV Bank / ATM Cards**: ISO 14443-4 APDU frame tunneling (`InDataExchange 0x42`) for reading cardholder PAN and expiry dates.
- 📖 **Full Engineering Guide**: See [**PN532 NFC Protocol & Hardware Acceleration Guide**](documentation/NFC_PN532_ENGINEERING.md).

---

### Part 3: ⚡ Battery & Power Management Subsystem

To ensure accurate battery percentage curves and trigger the Flipper OS **charging lightning bolt animation**, the INA219 sensor must sit on the **battery side (3.0V – 4.2V)**, *before* the 5V boost converter:

```text
    [ USB-C 5V Input ]
               │
    ┌──────────▼──────────┐
    │     TP4056 Board    │
    │  B+   B-  OUT+ OUT- │
    └──┬────┬────┬────┬───┘
       │    │    │    │
       │    │    │    └───────────────┬───────────────────────────► Common GND
       │    │    │                    │
       │  ┌─┴────┴───────┐            │
       │  │ 3.7V Battery │            │
       │  │  (+)     (-) │            │
       │  └──┬───────────┘            │
       │     │                        │
       └─────┼────────────┐           │
             │            │           │
          ┌──▼────────────▼───┐       │
          │      INA219       │       │
          │  Vin+        Vin- │       │
          │ 3.3V GND SCL  SDA │       │
          └───┬───┬───┬────┬──┘       │
              │   │   │    │          │
              │   └───┼────┼──────────┤ (Common GND)
              │       │    │          │
              │       │    │  [Power Switch]
              │       │    │        / 
              │       │    └───►[ ]── / ──[ ]
              │       │           (ON/OFF)  │
              │       │                     │
              │       │       ┌─────────────▼─────────┐
              │       │       │ Mini Boost Converter  │
              │       │       │ VIN  GND   GND   OUT  │
              │       │       └──┬────┬─────┬─────┬───┘
              │       │          │    │     │     │ (Regulated 5.0V)
              │       │          │    └─────┴─────┼───────────────► Common GND
              │       │          │                │
              │       │          │                └───────────────┐
              │       │          │                                │
  ┌───────────▼───────┼──────────┼────────────────────────────────┼────────┐
  │  3.3V            PA9        PB9                              5V   GND  │
  │ (Logic Out)     (SCL)       (SDA)                         (Power In)    │
  │                                                                        │
  │                       WeAct STM32WB55CGU6 Board                        │
  └────────────────────────────────────────────────────────────────────────┘
```

#### 📋 Power Chain Connection Table

| From Component & Pin | To Component & Pin | Wire Color | Voltage | Role / Notes |
|---|---|:---:|:---:|---|
| **Battery (+)** | **INA219 `Vin+`** & **TP4056 `B+`** | 🔴 Red | 3.0V – 4.2V | Cell positive terminal |
| **Battery (-)** | **TP4056 `B-`** | ⚫ Black | 0V | Protected negative terminal |
| **TP4056 `OUT+`** | **INA219 `Vin-`** | 🔴 Red | 3.0V – 4.2V | Charger output rail |
| **TP4056 `OUT-`** | **Common Ground (`GND`)** | ⚫ Black | 0V | System ground return |
| **INA219 `Vin-`** | **Power Switch (Terminal 1)** | 🔴 Red | 3.0V – 4.2V | Unswitched raw battery power |
| **Power Switch (Terminal 2)** | **Boost Converter `VIN`** | 🔴 Red | 3.0V – 4.2V | Switched battery power into boost |
| **Boost Converter `OUT`** | **WeAct `5V` (or `VBUS`)** | 🔴 Red | **5.0V DC** | Clean 5.0V into WeAct onboard LDO |
| **INA219 `VCC`** / `GND` | **WeAct `3.3V`** / `GND` | 🔴 / ⚫ | 3.3V / 0V | Sensor logic power & ground |
| **INA219 `SCL`** / `SDA` | **WeAct `PA9`** / `PB9` | 🟡 / 🔵 | 3.3V I2C | `I2C1` fuel gauge telemetry |

---

### Part 4: 🎮 Keypad & 5-Way Joystick Mapping

The buttons operate in an **active-low** configuration (pressing connects the pin to **GND**). The PCF8574 weak internal pull-up holds released pins `HIGH`:

```text
                 ┌───────────────┐
                 │    ▲ UP (P0)  │
                 └───────┬───────┘
                         │
       ┌──────────────┐  │  ┌──────────────┐
       │ ◄ LEFT (P2)  ├──┼──┤ RIGHT ► (P3) │
       └──────────────┘  │  └──────────────┘
                         │
                 ┌───────┴───────┐
                 │  ● OK (P4)    │
                 ├───────────────┤
                 │   ▼ DOWN (P1) │
                 └───────────────┘

       ┌──────────────┐     ┌──────────────┐
       │ ↩ BACK (P5)  │     │ 📳 VIBRO (P6)│
       └──────────────┘     └──────────────┘
```

* **P0** ➔ Up Button
* **P1** ➔ Down Button
* **P2** ➔ Left Button
* **P3** ➔ Right Button
* **P4** ➔ OK (Select) Button *(Joystick Center Press)*
* **P5** ➔ Back Button *(SET or RST switch)*
* **P6** ➔ Vibration Motor *(Driven through N-MOSFET / NPN transistor with 1N4148 flyback diode; never connect motor directly to pin!)*

---

## 🚀 4-Step Quick Start: Flashing the Device

```mermaid
graph LR
    A[1. Connect Hardware] --> B[2. Set OTP Profile]
    B --> C[3. Bootloader Repair in qFlipper]
    C --> D[4. Install Firmware .tgz]
```

### Step 1: Connect your modules
Wire your OLED screen, buttons, and MCU according to the visual wiring diagram above.

### Step 2: Configure OTP Memory (One-Time Only)
1. Open **`generate_otp_gui.exe`** (in [`mics/FlipperOTP/`](mics/FlipperOTP/)).
2. Set **Device Name** (e.g. `Flipper`), **Board Version: 12** *(WeAct STM32WB55)*, and **Display Type: MGG** *(Required for OLED)*.
3. Put the board into **DFU mode**: hold physical **BOOT0** on the WeAct board, plug in USB, and release BOOT0.
4. Click **"2. Flash (DFU)"** in the app.

---

### Step 3: Flash Firmware via qFlipper (1-Click Install)

#### For First-Time Setup:
1. Put the board into **DFU mode** (hold `BOOT0`, plug in USB, release `BOOT0`).
2. Open official **qFlipper** on your PC.
3. qFlipper will display **"RECOVERY MODE"**. Click **"REPAIR"** to flash the official bootloader.
4. Put the board back into **DFU mode** once more.
5. In qFlipper, click **"Install from file"** and select our **`.tgz`** release package from [Releases](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/releases).
6. qFlipper will flash the firmware, turn on the OLED screen, and copy all game/app assets to your microSD card!

> [!NOTE]
> **Black Screen During Update is Normal**: The standalone updater payload only contains drivers for the factory SPI screen. During installation, the screen stays off. Wait until qFlipper shows **"Update Successful!"** and the device will boot into the full OS.

---

## 🛠️ How to Build from Source (For Developers)

Use the built-in FBT build tool to compile locally:

```bash
# Windows (Command Prompt / PowerShell)
cmd /c fbt.cmd

# Linux / macOS
./fbt

# Build the complete qFlipper .tgz installer bundle & SDK
cmd /c fbt.cmd --with-updater updater_package
```

Compiled binaries are output to `build/f7-firmware-C/` and `dist/f7-C/`.

---

## 📐 Engineering Architecture & Waterfall Diagrams

### 1. 🏗️ Multi-Layer System Stack

```mermaid
graph TD
    subgraph AppLayer [Application Layer]
        Apps[GUI Apps: NFC, Sub-GHz, RFID, BadUSB, FAPs]
    end

    subgraph ServiceLayer [Middleware & System Services]
        GUI_Srv[GUI Service & Canvas]
        Input_Srv[Input Service & Debounce]
        Storage_Srv[Storage Service & SD FatFS]
        Power_Srv[Power Service & INA219 Fuel Gauge]
        Dolphin_Srv[Dolphin Pet Engine]
    end

    subgraph CoreLayer [Furi OS Core]
        PubSub[FuriPubSub Event Broker]
        Record[FuriRecord Service Locator]
        Threads[FuriThread / FreeRTOS Kernel]
    end

    subgraph HalLayer [Furi Hardware Abstraction Layer - HAL]
        HAL_OLED[SSD1306 / SH1106 Driver]
        HAL_PCF[PCF8574 Keypad Driver]
        HAL_NFC[PN532 HW Crypto1 / ST25R3916]
        HAL_RFID[125kHz Analog Timer Demodulator]
        HAL_SubGHz[CC1101 SPI Driver]
    end

    Apps --> GUI_Srv
    Apps --> Storage_Srv
    GUI_Srv --> PubSub
    Input_Srv --> PubSub
    PubSub --> Threads
    Threads --> HAL_OLED
    Threads --> HAL_PCF
    Threads --> HAL_NFC
    Threads --> HAL_RFID
    Threads --> HAL_SubGHz
```

---

### 2. 🌊 System Startup & Initialization Waterfall

```mermaid
graph TD
    Reset[Power-On Reset Vector] --> Clock[Init RCC Clocks: 64 MHz PLL]
    Clock --> OTP[Verify OTP Profile @ 0x1FFF7000]
    OTP --> GPIO[Configure GPIO Pinmux & Pull-ups]
    GPIO --> Buses[Initialize I2C1, I2C3, and SPI1 Buses]
    Buses --> RTOS[Start FreeRTOS Kernel]
    RTOS --> FuriCore[Initialize Furi Core & Record Locator]
    FuriCore --> Probe[Probe Hardware: INA219, SSD1306, PCF8574, SD Card]
    Probe --> Services[Spawn System Services in Dedicated Threads]
    Services --> Desktop[Render Splash & Launch Main Desktop UI]
```

---

### 3. 🔄 I2C Bus Arbitration & Rate-Limited Self-Healing

```mermaid
sequenceDiagram
    autonumber
    participant App as Application Render Loop
    participant Gui as GUI Service
    participant HAL as Furi HAL I2C1 Engine
    participant Bus as Physical I2C1 Bus
    participant PCF as PCF8574 Keypad (0x20)
    participant OLED as SSD1306 OLED (0x3C)

    Gui->>HAL: Acquire I2C1 Mutex
    HAL->>Bus: Flush 1024-byte Framebuffer Chunk
    Bus-->>OLED: Display Pixels Refreshed
    HAL->>Gui: Release I2C1 Mutex

    PCF-->>HAL: Button Pressed (PB0 Low Ext Interrupt)
    HAL->>Bus: Read Keypad Byte (0x20)
    alt Bus Normal
        Bus-->>HAL: Valid Key State (0xEF)
        HAL->>App: Dispatch InputEvent (Key: OK)
    else Bus Wedged by RF Noise
        Bus-->>HAL: Timeout / NACK
        HAL->>HAL: Rate-Limited Self-Heal (Check & Restore)
        HAL->>Bus: Send 9x SCL Clock Pulses + STOP + Reconfigure
        HAL->>App: Retain Cached Key (No Dropped Presses)
    end
```

---

### 4. ⚡ PN532 Hardware Crypto1 & ISO 14443-4 Protocol Flow

```mermaid
graph TD
    Detect[Detect Tag via InListPassiveTarget 0x4A] --> TypeCheck{Check SAK / ATQA}
    
    TypeCheck -->|SAK 0x08/0x18: MIFARE Classic 1K/4K| HW_Crypto[PN532 Hardware Crypto1 InAuth 0x40]
    TypeCheck -->|SAK 0x28: EMV ATM / Bank Cards| ISO_Tunnel[Tunnel ISO 7816-4 APDUs via InDataExchange 0x42]
    TypeCheck -->|SAK 0x00: NTAG / Ultralight| NTAG_Read[Direct Page Read 0x30]

    HW_Crypto --> AuthCheck{Auth Success?}
    AuthCheck -->|Yes| ReadBlock[Read Block Data via InDataExchange]
    AuthCheck -->|Checksum Error| RetryCheck{Retry Attempt < 3?}
    RetryCheck -->|Yes| HW_Crypto
    RetryCheck -->|No| NextSector[Advance to Next Key / Sector]
```

---

### 📻 Supported Sub-GHz Applications & External Module Compatibility

All Sub-GHz external applications are built to seamlessly integrate with both the onboard CC1101 and external high-power transceivers (such as the **Ebyte E07-900M10S / E07-400M10S** with PA/LNA or standard CC1101 modules connected to the 18-pin expansion header). 

The table below outlines each app's role and its external hardware routing mechanism:

| Application | Category | Primary Function | External CC1101 Compatibility & Mechanism |
|---|---|---|---|
| **Sub-GHz RAW Edit** (`subghz_raw_edit`) | 🛠️ Signal Utility | On-device waveform editor; trims noise and zooms into pulse trains | 💾 File-based processor (acts on captured `.sub` files) |
| **Subhound** (`subhound`) | 🛠️ Signal Utility | On-device classifier for BinRAW captures; classifies 17 ISM signal protocols | 💾 File-based analyzer (acts on `.sub` files) |
| **Sub Analyzer** (`sub_analyzer`) | 🛠️ Signal Utility | Deeply parses `.sub` captures, extracts protocol bits and timing | 💾 File-based analyzer (acts on captured `.sub` files) |
| **Sub Duplicate Finder** (`sub_dup_finder`) | 🛠️ Signal Utility | Scans SD card to identify and delete duplicate Sub-GHz captures | 💾 Storage utility (manages `.sub` file duplicates) |
| **Radio Scanner** (`radio_scanner`) | 🛠️ Signal Utility | Real-time scanner piping raw analog demodulation to piezo speaker | 🔌 **Automatic** via `subghz_radio_device_loader` with `[Ext]` OLED indicator |
| **Spectrum Analyzer** (`spectrum_analyzer`) | 🛠️ Signal Utility | Live graphical RF signal strength & band activity visualizer | 🔌 **Automatic** via `subghz_radio_device_loader` (`cc1101_int` / `cc1101_ext`) |
| **Weather Station** (`weather_station`) | 🚗 Sensor Decoder | Decodes outdoor weather sensors (Acurite, Oregon, Nexus 433/868/915 MHz) | 🔌 **Automatic** via `subghz_radio_device_loader` |
| **TPMS Reader** (`tpms_reader`) | 🚗 Sensor Decoder | Tire Pressure Monitoring System sensor decoder and RF bridge | 🔌 **Automatic** via `subghz_radio_device_loader` |
| **RS41 Tracker** (`rs41_tracker`) | 🚗 Sensor Decoder | Receives & decodes Vaisala RS41 radiosondes (GPS, altitude, PTU weather data) | 🔌 **Native** external CC1101 SPI (PA4 CS) toggle with INT/EXT indicator |
| **Walkie Talkie** (`walkie_talkie`) | 📡 Comm & Remote | FRS 22-channel activity monitor, auto-squelch, and live RSSI scanner | 🔌 **Automatic** via `subghz_radio_device_loader` with `[Ext]` OLED indicator |
| **Genie Door Recorder** (`genie_record`) | 🚗 Vehicle Decoder | Decodes and replays Genie garage door rolling codes (`.gne`) | 🔌 **Automatic** via `radio_device_loader_set(NULL, SubGhzRadioDeviceTypeAuto)` |
| **Rolling Flaws** (`rolling_flaws`) | 🚗 Vehicle Decoder | Keeloq and rolling-code flaw vulnerability tester & transmitter | 🔌 **Automatic** via `radio_device_loader_set(NULL, SubGhzRadioDeviceTypeAuto)` |
| **Meal Pager Tool** (`meal_pager`) | 📡 Comm & Remote | Simulates and tests restaurant paging systems (Retekess T119 coaster pagers) | 🔌 **Native** internal & external CC1101 switching |
| **Enhanced Sub-GHz Chat** (`esubghz_chat`)| 📡 Comm & Remote | Point-to-point encrypted Sub-GHz two-way text messaging | 🔌 **Native** auto-loader with external CC1101 pin routing & fallback |
| **Flipper Share** (`flipper_share`) | 📡 Comm & Remote | Ad-hoc wireless file transfer system over Sub-GHz channels | 🔌 **Automatic** via `radio_device_loader_set(NULL, SubGhzRadioDeviceTypeAuto)` |
| **Sub-GHz Remote** (`subghz_remote`) | 📡 Comm & Remote | Quick-action remote mapping 5 `.sub` files to hardware buttons | 🔌 **Native** `SubGhzRadioDeviceTypeExternalCC1101` selector |
| **Sub-GHz Scheduler** (`subghz_scheduler`) | 📡 Comm & Remote | Automated and periodic transmission of `.sub` trigger files | 🔌 **Native** `SubGhzRadioDeviceTypeExternalCC1101` selector |
| **Morse Flipper** (`morse_flipper`) | 📡 Comm & Remote | CW/Morse trainer, ARDF direction finding, and Sub-GHz transmitter | 🔌 **Automatic** radio transmission engine |
| **APRS Transmitter** (`aprstx`) | 📡 Comm & Remote | Automatic Packet Reporting System telemetry & AX.25 beaconing | 🔌 **Native** internal / external CC1101 hardware switcher |
| **Subdriving / Wardriver** (`wardriver`) | 📡 Comm & Remote | RF mapping tool pairing Sub-GHz captures with GPS coordinates | 🔌 **Automatic** Sub-GHz receiver pipeline |
| **Quac! Remote** (`quac`) | 📡 Comm & Remote | Universal multi-protocol macro controller (Sub-GHz, IR, RFID) | 🔌 **Native** `SubGhzRadioDeviceTypeExternalCC1101` support |
| **Music to Sub-GHz Radio** (`fmf_to_sub`) | 📡 Comm & Remote | Converts `.fmf` ringtones/audio layouts to broadcast-ready `.sub` files | 💾 File converter (prepares `.sub` files for RF broadcast) |

> [!TIP]
> **Radio Selection**: When using an external CC1101 or Ebyte E07 module, go to **Settings ➔ Radio** in the firmware and select **External** (or **Auto**). All apps using `subghz_radio_device_loader` will automatically route through the external module on the 18-pin expansion header, unlocking full transmission range and extended frequencies!

---

### 📚 Deep-Dive Documentation Index
* 🏛️ [**System & Firmware Architecture Guide**](documentation/ARCHITECTURE.md) — FreeRTOS task scheduling, memory maps, and Furi OS primitives.
* 🌊 [**Firmware Boot & System Lifecycle Guide**](documentation/BOOT_AND_LIFECYCLE.md) — Waterfall boot sequence, OTP validation, bus recovery, and sleep/wake state machines.
* 📡 [**External CC1101 & Ebyte E07 Technical Reference**](documentation/EXTERNAL_CC1101.md) — Transceiver connection, E07 PA/LNA RF switch sequencing, and Radio Settings v4 integration.
* 📶 [**NRF24L01+ 2.4 GHz Transceiver Guide**](documentation/NRF24.md) — Pinout wiring, decoupling capacitor power filtration, and MouseJacker/Sniffer FAP apps.
* ⚡ [**PN532 NFC Protocol & Hardware Acceleration Guide**](documentation/NFC_PN532_ENGINEERING.md) — Hardware Crypto1 acceleration, ISO 14443-4 APDU tunneling for bank cards, and retry mechanisms.
* 🔌 [**Expansion Modules & GPIO Protocol Guide**](documentation/ExpansionModules.md) — Serial expansion protocol, RPC framing, and 18-pin expansion header pinout.
* 📐 [**Hardware & Electrical Engineering Guide**](documentation/HARDWARE_DESIGN.md) — Complete schematic analysis, 18-pin header matrix, I2C pull-up calculations, and decoupling guidelines.
* ❓ [**FAQ & Troubleshooting Guide**](faq_diy_flipper.md) — Common boot issues, I2C freeze recovery, CC1101 calibration, and power troubleshooting.

---

## 📐 LF-RFID Discrete Analog Subsystem

The LF-RFID 125 kHz subsystem operates using discrete analog components:

* 📄 **LF-RFID PDF Schematic**: [Download 125kHz Subsystem Schematic (PDF)](misc/rfid_lf.pdf)

<details>
<summary><b>🔍 View LF-RFID Component Bill of Materials (BOM)</b></summary>

| Stage | Component | Value / Part | Description |
|---|---|---|---|
| **Transmitter Driver** | `PA5` (PWM) | MCU `TIM2_CH1` | 125 kHz Carrier PWM Drive |
| | `Q1` | BC337 / S8050 / 2N2222 | NPN Push-Pull Transistor |
| | `Q2` | BC327 / S8550 / 2N2907 | PNP Push-Pull Transistor |
| | `C1` | 2.2 nF | Drive Coupling Capacitor |
| **Resonant Tank** | `L1` | 1.2 mH / 95T | Antenna Coil |
| | `PA2` (Emulate) | MCU `TIM2_CH3` | Emulation Pulse Driver (`R2` 1k, `R8` 10k) |
| | `Q3` | 2N2222 | Emulation Switch Transistor (`R1` 100 Ohm) |
| **Demodulator** | `D1` | 1N4148 / BAT54S | Envelope Schottky Diode |
| | `C3`, `R3` | 1 nF, 10 kOhm | RC Low-Pass Filter |
| | `C4`, `R4` | 22 nF, 10 kOhm | AC Coupling Stage |
| | `U1` | LM2904 / LM358 / MCP6002 | Op-Amp Signal Amplifier (`R5`-`R7` 100k, `R9` 50k) |
| | `PA1` (Data In) | MCU `TIM1_CH1` | Demodulated RX Envelope Input |

</details>

---

## 🤝 Credits and Maintainer

* **Maintainer & Developer**: [**AJ_60**](https://github.com/AJ60)
* **Design & Code Contributors**: Nucleus Dark, Lamtran, artema0g, and the Flipper Zero / Momentum community.
* **License**: Open-source under the [GNU General Public License v3.0](LICENSE).
