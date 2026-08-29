# 🐬 DIY Flipper Zero (OLED Edition)

> Build your own DIY Flipper Zero with an I2C OLED display, PCF8574 keypad, PN532 NFC reader, and discrete sub-GHz / 125kHz RFID hardware!

[![FBT Build](https://img.shields.io/badge/build-FBT-blue.svg)](https://github.com/AJ60/oled_flipper)
[![Platform](https://img.shields.io/badge/platform-STM32WB55-orange.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32wb-series.html)
[![Maintainer](https://img.shields.io/badge/maintainer-AJ__60-brightgreen.svg)](https://github.com/AJ60)
[![License](https://img.shields.io/badge/license-GPL--3.0-green.svg)](LICENSE)

> [!CAUTION]
> ⚖️ **LEGAL & EDUCATIONAL DISCLAIMER**:
> This project and firmware are created strictly for **educational, academic research, and authorized security testing purposes only**. 
> - **DO NOT** use this firmware or hardware for any unauthorized access, card cloning, or malicious activities.
> - The developers and contributors assume **no liability or responsibility** for any misuse, damage to property, or illegal actions committed using this software or hardware.

> [!WARNING]
> 🚧 **DEVELOPMENT STATUS NOTICE**:
> - **NFC Subsystem (PN532)**: **Under Active Development / Experimental.** Currently tested and verified only with **MIFARE Classic 1K tags**, **NTAG series (NTAG213/215/216/Ultralight)**, and **EMV ATM / Bank payment cards** (ISO 14443-4 APDU reading). Other NFC standards and card types may behave unpredictably.
> - **125 kHz LF-RFID Subsystem**: **Under Active Development / Experimental.** May contain bugs due to discrete analog hardware tolerances, coil inductance variance, and signal demodulation thresholds.

---

## 🧩 The Building Blocks (Hardware Modules)

Think of your DIY Flipper as a friendly little robot made of modular building blocks:

![DIY Flipper Component Guide](misc/module_overview.jpg)

```mermaid
graph TD
    subgraph Core [The Core Hardware]
        MCU["🧠 The Brain<br><b>WeAct STM32WB55</b>"]
        OLED["👀 The Eyes<br><b>SSD1306 0.96 inch OLED</b>"]
        KEYPAD["🎮 The Hands<br><b>PCF8574 Button Board</b>"]
    end
    
    subgraph Radios [Wireless & Sensors]
        NFC["💳 Keycard Reader<br><b>PN532 NFC Module</b>"]
        RADIO["📻 The Antenna<br><b>CC1101 Sub-GHz</b>"]
        RFID["🏷️ Key Fob Reader<br><b>125kHz Discrete Tank</b>"]
    end
    
    subgraph StoragePower [Storage, Sound & Power]
        SD["💾 The Backpack<br><b>MicroSD SPI Module</b>"]
        BUZZER["🔊 The Voice<br><b>Piezo Buzzer PB8</b>"]
        INA["🔋 Fuel Gauge<br><b>INA219 / INA226</b>"]
    end

    MCU --- OLED
    MCU --- KEYPAD
    MCU --- NFC
    MCU --- RADIO
    MCU --- RFID
    MCU --- SD
    MCU --- BUZZER
    MCU --- INA
```

| # | Part | Nickname | What It Does (In Simple Words) |
|---|---|---|---|
| **1** | **WeAct STM32WB55** | 🧠 **The Brain** | Fast dual-core chip that runs the operating system, games, and apps. |
| **2** | **SSD1306 0.96" OLED** | 👀 **The Eyes** | Shows animations, dolphin pet, menus, and signal frequencies. |
| **3** | **PCF8574 Expander** | 🎮 **The Hands** | Connects 6 direction/action buttons + haptic vibration rumble. |
| **4** | **PN532 NFC Module** | 💳 **Keycard Reader** | Reads 13.56 MHz NFC (MIFARE Classic 1K, NTAG, Bank Cards). |
| **5** | **CC1101 Radio** | 📻 **The Antenna** | Transmits and catches sub-GHz radio signals (gates, remotes, sensors). |
| **6** | **MicroSD Card Module** | 💾 **The Backpack** | Stores your saved keys, remotes, scripts, games, and animations. |
| **7** | **Passive Buzzer** | 🔊 **The Voice** | Plays fun 8-bit chimes, game sounds, and keypress clicks. |
| **8** | **INA219 / INA226** | 🔋 **Fuel Gauge** | Monitors battery voltage and charging percentage accurately. |

---

## 🔌 Easy Visual Wiring Guide (Breadboard Style)

Connecting the modules is just like snapping together color-coded blocks. Follow the wiring visual and connection map below:

![DIY Flipper Breadboard Wiring Diagram](misc/wiring_diagram_easy.jpg)

```mermaid
graph LR
    subgraph MCU_Pins [WeAct STM32WB55 MCU]
        PWR["🔴 3.3V (Power Rail)"]
        GND["⚫ GND (Ground Rail)"]
        I2C1_SCL["🟡 PA9 (I2C1 Clock)"]
        I2C1_SDA["🔵 PB9 (I2C1 Data)"]
        I2C3_SCL["🟡 PC0 (I2C3 Clock)"]
        I2C3_SDA["🔵 PC1 (I2C3 Data)"]
        SPI_SCK["🟡 PB3 (SPI Clock)"]
        SPI_MOSI["🔵 PB5 (SPI MOSI)"]
        SPI_MISO["🟡 PA6 (SPI MISO)"]
    end

    subgraph I2C_Bus [Shared I2C1 Bus]
        OLED_MOD["SSD1306 OLED (0x3C)"]
        PCF_MOD["PCF8574 Keypad (0x20)"]
        INA_MOD["INA219 Power Gauge (0x40)"]
    end

    subgraph SPI_Bus [Shared SPI1 Bus]
        SD_MOD["MicroSD Card (CS: PA10)"]
        CC_MOD["CC1101 Radio (CS: PA15)"]
    end

    subgraph NFC_Bus [NFC I2C3 Bus]
        PN_MOD["PN532 NFC (IRQ: PA2)"]
    end

    I2C1_SCL --> OLED_MOD
    I2C1_SCL --> PCF_MOD
    I2C1_SCL --> INA_MOD

    I2C1_SDA --> OLED_MOD
    I2C1_SDA --> PCF_MOD
    I2C1_SDA --> INA_MOD

    SPI_SCK --> SD_MOD
    SPI_SCK --> CC_MOD
    SPI_MOSI --> SD_MOD
    SPI_MOSI --> CC_MOD
    SPI_MISO --> SD_MOD
    SPI_MISO --> CC_MOD

    I2C3_SCL --> PN_MOD
    I2C3_SDA --> PN_MOD
```

### 🎨 Wire Color Rule:
* 🔴 **Red** = Power (`3.3V`)
* ⚫ **Black** = Ground (`GND`)
* 🔵 **Blue** = Data line (`SDA` / `MOSI`)
* 🟡 **Yellow** = Clock line (`SCL` / `SCK` / `MISO`)
* 🟢 **Green** = Control signal (`CS` / `INT` / `IRQ`)

---

### 📋 Pin-to-Pin Connection Table

#### 1. I2C Bus Devices (Screen, Keypad & Power Monitor)
All 3 modules share the same two clock & data pins:

| Module Pin | Connects to MCU Pin | Wire Color | Purpose |
|---|---|---|---|
| **VCC** (All 3 modules) | **3.3V** | 🔴 Red | Power |
| **GND** (All 3 modules) | **GND** | ⚫ Black | Ground |
| **SCL** (All 3 modules) | **PA9** | 🟡 Yellow | I2C Clock |
| **SDA** (All 3 modules) | **PB9** | 🔵 Blue | I2C Data |
| **PCF8574 INT** | **PB0** | 🟢 Green | Button Press Wakeup Signal |
| **INA219/226 ALERT** | **PB1** | 🟢 Green | Low Battery / Overcurrent Alert |

> [!IMPORTANT]
> **I2C Pull-Up Resistors**: The OLED screen board has built-in 4.7 kΩ pull-up resistors on SDA/SCL. Keep the OLED connected so the I2C bus stays stable during boot!

---

#### 2. SPI & External Bus Devices (SD Card, CC1101 Radio & NFC)

| Module | Module Pin | Connects to MCU Pin | Purpose |
|---|---|---|---|
| **Shared Clock** | `SCK` | **PB3** | SPI Clock |
| **Shared MOSI** | `MOSI` | **PB5** | Data Out from MCU |
| **Shared MISO** | `MISO` | **PA6** | Data In to MCU |
| **MicroSD Card** | `CS` | **PA10** | SD Card Select |
| **CC1101 Radio** | `CSN / CS` | **PA15** | Radio Select |
| | `GDO0 / G0` | **PA1** | Radio IRQ Signal |
| **PN532 NFC (I2C3)** | `SCL` / `SDA` | **PC0** / **PC1** | NFC I2C Bus 3 |
| | `IRQ` | **PA2** | NFC Card Detect IRQ |
| **ST25R3916 (SPI)** | `CS` | **PE4** | SPI NFC Select (Alternative) |

---

#### 3. Other Peripherals (Buzzer, IR & 125kHz RFID)

| Feature | Component Pin | Connects to MCU Pin | Note |
|---|---|---|---|
| 🔊 **Speaker / Buzzer** | Positive `+` | **PB8** (TIM16) | Connect negative `-` to GND |
| 🔴 **IR Receiver** | `DATA` | **PA0** | 38 kHz TSOP receiver |
| 💡 **IR Transmitter** | `LED Anode` | **PA8** | High-power IR LED (via NPN transistor) |
| 🏷️ **1-Wire / iButton** | `Data` | **PA3** | Dallas iButton probe with 4.7k pull-up |
| 📻 **LF-RFID (125 kHz)** | Carrier TX | **PA5** (TIM2_CH1) | Coil driver push-pull stage *(Experimental)* |
| | Envelope RX | **PA1** (TIM1_CH1) | Demodulated envelope input *(Experimental)* |
| | Emulate | **PA2** (TIM2_CH3) | Tag emulation pulse switch *(Experimental)* |

---

## 🎮 Button Mapping Guide (PCF8574 Keypad)

Buttons are wired to the PCF8574 board in an **active-low** configuration (pressing a button connects the pin to **GND**):

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
       │ ↩ BACK (P5)  │     │ 📳 VIBRO(P6) │
       └──────────────┘     └──────────────┘
```

* **P0** ➔ Up Button
* **P1** ➔ Down Button
* **P2** ➔ Left Button
* **P3** ➔ Right Button
* **P4** ➔ OK (Select) Button
* **P5** ➔ Back Button
* **P6** ➔ Vibration Motor (driven through an N-channel MOSFET; do not connect motor directly to pin!)

---

## 🚀 4-Step Quick Start: Flashing the Device

```mermaid
graph LR
    A[1. Connect Hardware] --> B[2. Set OTP Profile]
    B --> C[3. Flash in qFlipper]
    C --> D[4. Have Fun!]
```

### Step 1: Connect your modules
Connect your OLED screen, buttons, and MCU according to the wiring diagram.

### Step 2: Configure OTP Memory (Only Once)
1. Open **`generate_otp_gui.exe`** (in the [`mics/FlipperOTP/`](mics/FlipperOTP/) folder).
2. Set **Device Name** (e.g. `Flipper`), **Board Version** (`12` for WeAct STM32WB55), and **Display Type: MGG** *(required for OLED)*.
3. Put the board into **DFU mode**: hold the physical **BOOT0** button on the WeAct board, plug in the USB cable, and release BOOT0.
4. Click **"2. Flash (DFU)"** in the app.

---

### Step 3: Flash Firmware via qFlipper (1-Click Install)

#### For First-Time Setup:
1. Put the board in **DFU mode** (hold `BOOT0`, plug in USB, release `BOOT0`).
2. Open the official **qFlipper** application on your PC.
3. qFlipper will show **"RECOVERY MODE"**. Click **"REPAIR"** to install the bootloader.
4. Put the board back into **DFU mode** once more.
5. Click **"Install from file"** in qFlipper and select our **`.tgz`** firmware package from the [Releases](https://github.com/AJ60/oled_flipper/releases) page.
6. qFlipper will flash the firmware, turn on the OLED screen, and automatically copy all required game/app resource files to your microSD card!

#### For Normal Updates:
Connect the DIY Flipper via USB, open **qFlipper**, click **"Install from file"**, and select the updated **`.tgz`** package.

---

## 🛠️ How to Build from Source (For Developers)

Use the built-in FBT build system to compile the firmware locally:

```bash
# Windows (Command Prompt / PowerShell)
cmd /c fbt.cmd

# Linux / macOS
./fbt

# Build the complete qFlipper .tgz installer bundle & SDK
cmd /c fbt.cmd --with-updater updater_package
```

Compiled binaries land in `build/f7-firmware-C/` and `dist/f7-C/`.

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

### 3. 🔄 I2C Bus Arbitration & Rate-Limited Self-Healing Waterfall

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

### 4. ⚡ PN532 Hardware Crypto1 & ISO 14443-4 APDU Protocol Flow

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

### 📚 Deep-Dive Engineering Documentation:
* 🏛️ [**System & Firmware Architecture Guide**](documentation/ARCHITECTURE.md) — FreeRTOS task scheduling, memory maps, IPCC dual-core mailbox, and Furi OS primitives.
* 🌊 [**Firmware Boot & System Lifecycle Guide**](documentation/BOOT_AND_LIFECYCLE.md) — Step-by-step waterfall sequence, OTP validation, bus recovery, and sleep/wake state machines.
* ⚡ [**PN532 NFC Protocol & Hardware Acceleration Guide**](documentation/NFC_PN532_ENGINEERING.md) — Hardware Crypto1 authentication, ISO 14443-4 APDU tunneling for bank cards, and checksum error retries.
* 📐 [**Hardware & Electrical Engineering Guide**](documentation/HARDWARE_DESIGN.md) — Schematic analysis, I2C pull-up calculations, 125kHz analog tank tuning, and power decoupling.

---

## 📐 LF-RFID Discrete Analog Subsystem

The LF-RFID 125 kHz subsystem operates using discrete analog components:

* 📄 **LF-RFID PDF Schematic**: [Download 125kHz Subsystem Schematic (PDF)](misc/rfid_lf.pdf)

> [!WARNING]
> The 125 kHz analog circuit is sensitive to component tolerances (coil inductance, capacitor values, and diode forward voltage). It is provided for **educational experimentation** and may require fine-tuning on breadboards.

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
