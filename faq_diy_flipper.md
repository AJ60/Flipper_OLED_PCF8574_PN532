# ❓ DIY Flipper Zero (OLED Edition) — FAQ & Troubleshooting Guide 🐬

> Complete troubleshooting and reference guide for the DIY Flipper Zero (I2C OLED, PCF8574 Keypad, and PN532 NFC).

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

> [!CAUTION]
> ⚖️ **LEGAL & EDUCATIONAL DISCLAIMER**:
> This project, firmware, and documentation are provided strictly for **educational, academic research, and authorized security testing purposes only**. 
> - **DO NOT** use this firmware or hardware for any unauthorized access, card cloning, or malicious activities.
> - The developers and contributors assume **no liability or responsibility** for any misuse, damage to property, or illegal actions.

> [!WARNING]
> 🚧 **DEVELOPMENT STATUS NOTICE**:
> - **NFC (PN532)**: **Under Active Development / Experimental.** Currently tested and verified only with **MIFARE Classic 1K/4K tags**, **NTAG series (NTAG213/215/216/Ultralight)**, and **EMV ATM / Bank payment cards** (ISO 14443-4 APDU reading).
> - **125 kHz LF-RFID**: **Under Active Development / Experimental.** May contain bugs due to discrete analog hardware tolerances, coil inductance variance, and signal demodulation thresholds.

---

## 💿 Flashing & Operating System

### **Q1: I connected my new WeAct board, but qFlipper does not detect it. What should I do?**
> [!IMPORTANT]
> A blank WeAct STM32WB55 board lacks the Flipper bootloader and OTP memory configuration. It will initially be detected as a generic STM32 DFU device.

**Solution:**
1. **Configure OTP Profile (First-time only):**
   - Open [`mics/FlipperOTP/generate_otp_gui.exe`](mics/FlipperOTP/) on your PC.
   - Set **Board Version: 12** and **Display Type: MGG** *(required for OLED)*.
   - Put the board into DFU mode (hold **BOOT0**, connect USB, release **BOOT0**) and click **"2. Flash (DFU)"**.
2. **Install Bootloader via qFlipper:**
   - Put the board back into DFU mode.
   - Launch official **qFlipper** — it will detect the board in **RECOVERY MODE**.
   - Click the **"REPAIR"** button to flash the bootloader.
3. **Install OLED Firmware:**
   - Put the board in DFU mode once more.
   - In qFlipper, click **"Install from file"** and choose our custom **`.tgz`** firmware package from [Releases](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/releases).

---

### **Q2: The firmware flashes successfully, but the device is frozen / unbootable (blank screen, and qFlipper reports "RPC Session Timeout"). How do I fix this?**
> [!IMPORTANT]
> The custom firmware features an active I2C bus probe and recovery sequence. If the I2C1 bus has no hardware pull-up resistors or is floating, the firmware enters a recovery loop to avoid bus deadlock, pausing USB serial until the bus is released.

**Solution:**
1. **Connect your OLED screen:** Standard I2C OLED screens (SSD1306/SH1106) have built-in **4.7 kΩ pull-up resistors** on their SDA/SCL lines. Booting without the display plugged in leaves the bus floating.
2. **Solder physical I2C pull-ups:** Solder **2.2 kΩ or 3.3 kΩ resistors** between SDA (`PB9`) -> 3.3V rail and SCL (`PA9`) -> 3.3V rail. This prevents noise-induced bus locks and stabilizes button reads.

---

### **Q3: During the `.tgz` update, my OLED screen goes completely black. Is it bricked?**
> [!NOTE]
> **This is expected behavior.** The standalone updater payload runs outside the main OS and only contains drivers for the original hardware SPI screen. Because our DIY board uses an I2C OLED display, the screen turns off during the flashing process.

**Solution:**
* **DO NOT unplug the USB cable.**
* Monitor installation progress in the qFlipper desktop app.
* Once qFlipper reports **"Update Successful!"**, the board will reboot, start the main OS, and the OLED screen will turn on with all animations and apps loaded.

---

### **Q4: How do I view real-time Debug Logs on the DIY Flipper?**

#### 1. Enable Debug Logging:
* **Via Settings:** Go to **Settings > System > Log Level** and set it to **`Debug`** (or **`Trace`** for maximum verbosity).
* **Via CLI:** In a serial terminal session, run:
  ```bash
  log debug
  ```

#### 2. View Debug Logs:
* **qFlipper:** Open qFlipper and click the **"Log"** icon in the bottom-left corner (`Ctrl + L` / `Cmd + L`).
* **Serial Terminal:** Open any terminal program (PuTTY, Tera Term, minicom) connected to the Flipper virtual COM port at `115200` baud.

---

### **Q5: Can I reboot into DFU mode using the software settings menu?**
* **No.** Because the keypad connects via the PCF8574 I2C expander rather than direct MCU GPIO lines, software-triggered DFU reboot is not supported by the ST bootloader.
* Use the hardware button method (hold **BOOT0** while plugging in USB).

---

### **Q6: Why is CC1101 PPM calibration recommended?**
> [!TIP]
> Generic CC1101 modules use crystal oscillators with slight frequency drift.

* Average hardware crystal offset is approximately **-32 kHz** (~ -74 PPM).
* Adjusting the calibration to **+100 PPM** (under *Momentum / Radio Settings -> Sub-GHz -> Frequency Calibration*) aligns transmissions with standard **433.920 MHz**.

---

## 🔌 Hardware & Bus Troubleshooting

### **Q7: Keyboard buttons freeze or drop presses during RF / Sub-GHz activity. Why?**
> [!CAUTION]
> High-power Sub-GHz transmission or NFC field activation can induce EMI noise into long I2C jumper wires, causing PCF8574 read timeouts.

**Solution:**
1. **Add physical 2.2 kΩ – 3.3 kΩ pull-ups** to the 3.3V rail on `PB9` (SDA) and `PA9` (SCL).
2. **Add decoupling capacitor:** Place a **0.1 µF (100 nF)** ceramic capacitor directly across the VCC and GND pins of the PCF8574 module.
3. **Keep wiring short:** Use short jumper wires (< 10 cm) between the MCU and the keypad board.

---

### **Q8: What power filtering is recommended for custom breadboard/perfboard builds?**
* **Decoupling:** Place **0.1 µF** ceramic capacitors close to the VCC/GND pins of each peripheral module (OLED, PCF8574, PN532, CC1101, INA219).
* **Bulk Filtering:** Place a **10 µF – 47 µF** tantalum or low-ESR electrolytic capacitor across the main 3.3V power rail near the OLED screen.

---

### **Q9: Can I connect a vibration rumble motor directly to PCF8574 pin P6?**
> [!CAUTION]
> **Never connect a vibration motor directly to the PCF8574 pin!** The motor draw (60–100 mA) exceeds the PCF8574 output rating (25 mA) and will damage the chip or cause brownout resets.

**Solution:**
* Drive the motor through an **N-channel MOSFET** (e.g. `2N7002`, `AO3400`) or NPN transistor (`2N2222`).
* Connect a **1N4148 flyback diode** in reverse-parallel across the motor terminals (cathode to +3.3V, anode to transistor drain/collector) to clamp inductive voltage spikes.

---

### **Q10: Dallas iButton / 1-Wire keys (DS1990) are not reading on pin PA3.**
* The 1-Wire protocol requires an active pull-up.
* Solder a **2.2 kΩ or 4.7 kΩ resistor** between `PA3` and the **3.3V** rail.

---

### **Q11: Can I connect a speaker directly to pin PB8?**
* **Passive piezo buzzers** can be connected directly between `PB8` and `GND`.
* **Low-impedance dynamic speakers (8–32 Ω)** must **NOT** be connected directly; use an external transistor amplifier (e.g. `BC847`).

---

### **Q12: The INA219 / INA226 battery monitor reports inaccurate current or percentage.**
* The firmware is calibrated for a **0.1 Ω** current shunt resistor (marked `R100`).
* Ensure your INA module uses a 0.1 Ω 1% precision shunt resistor.

---

## ⚡ PN532 NFC Subsystem (I2C3)

### **Q13: NFC is not detecting cards or shows "NFC not found".**
> [!IMPORTANT]
> The PN532 is interfaced over **I2C3** (SCL: `PC0`/`PA7`, SDA: `PC1`/`PB4`, IRQ: `PA2`). Ensure your wiring matches the pinout below:

| PN532 Pin | MCU Pin / Header | Function |
|---|---|---|
| **VCC** | **3.3V** | Power Rail (3.3V only) |
| **GND** | **GND** | Ground Rail |
| **SCL** | **PA7** (header "C0") | I2C3 Clock |
| **SDA** | **PB4** (header "C1") | I2C3 Data |
| **IRQ** | **PA2** | Card Detect Interrupt |

**Checks:**
1. **DIP Switches / Jumper Pads:** Set the PN532 board to **I2C mode** (`I0=0`, `I1=1` or `CH1=ON`, `CH2=OFF` depending on module model).
2. **I2C Address:** Standard I2C address is `0x24`.
3. **IRQ Wire:** Ensure `IRQ` is firmly connected to `PA2`. The driver relies on the hardware IRQ for card detection.

---

### **Q14: What card types have been verified on the PN532 in this firmware?**
* ✅ **MIFARE Classic 1K / 4K**: Fully supported with hardware Crypto1 authentication and dictionary attack key cracking.
* ✅ **NTAG Series (NTAG213 / 215 / 216 / Ultralight)**: Direct page read and dump support.
* ✅ **Contactless EMV Bank / ATM Cards**: ISO 14443-4 APDU frame tunneling for payment card info extraction.
* ⚠️ **Other NFC protocols / Card Emulation**: Under active experimental development.

---

## 🏷️ 125 kHz LF-RFID Discrete Subsystem

### **Q15: What is the status of 125 kHz RFID reading and writing?**
> [!WARNING]
> The LF-RFID subsystem uses a discrete analog resonant circuit (`TIM2_CH1` PA5 carrier, `TIM1_CH1` PA1 envelope input, `TIM2_CH3` PA2 emulation). It is provided for **educational experimentation** and is under active development.

* **Reading EM4100 Fobs**: Performance depends on coil inductance (recommended ~1.2 mH, 95 turns) and capacitor matching for exact 125 kHz resonance.
* **Writing / Emulation**: Experimental; may contain timing quirks depending on discrete transistor switching speeds.
* Refer to [`misc/rfid_lf.pdf`](misc/rfid_lf.pdf) and [`documentation/HARDWARE_DESIGN.md`](documentation/HARDWARE_DESIGN.md) for full circuit schematics and tuning formulas.

---

## 📡 External Sub-GHz CC1101 & High-Power E07 Modules

### **Q16: How do I connect and enable an external CC1101 (or high-power Ebyte E07-433M20S)?**
The firmware includes full native driver support for external CC1101 transceivers and high-power PA/LNA modules:

| Header Pin # | MCU Pin | E07 / CC1101 Module Pin | Function |
|:---:|:---:|:---:|---|
| **Pin 9** | `3.3V` | `VCC` | 3.3V DC (do NOT connect to 5V) |
| **Pin 8** | `GND` | `GND` | Common ground reference |
| **Pin 4** | `PA4` | `CSN` | External SPI Chip Select |
| **Pin 5** | `PB3` | `SCK` | SPI1 Clock |
| **Pin 2** | `PB5` (Silkscreen "A7") | `MOSI` | SPI1 Data Input to Module |
| **Pin 3** | `PA6` | `MISO` | SPI1 Data Output from Module |
| **Pin 6** | `PB2` | `GDO0` | Demodulated sample capture IRQ (`EXTI2`) |
| *Internal* | *CC1101 GDO2* | `RXEN` | Controlled via CC1101 `IOCFG2` register |
| **Pin 7** | `PA5` (Silkscreen "C3") | `TXEN` | Power amplifier TX enable (Active-High) |

**Enabling the External Radio:**
1. Navigate to **Settings > Radio Settings** (or **Momentum > Radio Settings**).
2. Set **Sub-GHz Radio** to **`External`**.
3. The setting is automatically saved to `/int/radio.settings` (v4 format) and persists across reboots. All Sub-GHz apps and FAPs (like *Sub-GHz Bruteforcer*) will now route to the external radio automatically.

---

### **Q17: The external CC1101 reports "Radio Device Not Found" or SPI timeout. How do I fix it?**
1. **Verify the MOSI Pin:** On the WeAct core board, MCU pin `PB5` is silkscreened as **`A7`**. Do not connect MOSI to MCU `PA7` (which is labeled `C0` and used for PN532 SCL).
2. **Verify the SPI CS Pin:** The external module MUST be connected to `PA4` (Flipper Header Pin 4), NOT to `PA15` (which is reserved for the internal radio).
3. **Verify the GDO0 Pin:** External GDO0 must connect to `PB2` (Flipper Header Pin 6), NOT to `PA1` (internal radio GDO0).
4. **Power Supply & Decoupling:** When transmitting at +20 dBm (100 mW), the E07 module draws up to 120 mA peak. Place a 100 nF ceramic capacitor close to the module's VCC and GND pins.

---

### **Q18: How does the RF front-end switch work on Ebyte E07 (TXEN / RXEN)?**
The firmware driver automatically coordinates the RF front-end amplifiers without needing external logic gates:
* **RX Mode:** `GDO2 (RXEN)` is driven HIGH by the CC1101 hardware (`IOCFG2 = 0x2F`), enabling the LNA. `PA5 (TXEN)` is driven LOW.
* **TX Mode:** `GDO2 (RXEN)` is driven LOW (`IOCFG2 = 0x2E`), disabling the LNA. `PA5 (TXEN)` is driven HIGH, enabling the +20 dBm power amplifier.
* **Idle / Standby:** Both `RXEN` and `TXEN` are parked LOW so the RF front-end never floats or draws unnecessary power.
* **Coexistence with 125 kHz RFID:** Pin `PA5` is also connected to the discrete LF-RFID carrier driver. Because `PA5` is kept LOW in standby, the Sub-GHz driver causes zero interference with RFID operations.

---

## 📶 Nordic NRF24L01+ 2.4 GHz Subsystem

### **Q19: How do I connect an NRF24L01+ module and which applications work with it?**
The NRF24L01+ transceiver connects to the 18-pin expansion header:

| Header Pin # | MCU Pin | NRF24 Pin # | Pin Name | Description |
|:---:|:---:|:---:|:---:|---|
| **Pin 9** | `3.3V` | **Pin 2** | `VCC` | **3.3V ONLY** (5V will destroy the chip) |
| **Pin 8** | `GND` | **Pin 1** | `GND` | Common ground return |
| **Pin 4** | `PA4` | **Pin 4** | `CSN` | SPI Chip Select (Active-Low) |
| **Pin 6** | `PB2` | **Pin 3** | `CE` | Chip Enable (Active-High) |
| **Pin 5** | `PB3` | **Pin 5** | `SCK` | SPI1 Clock |
| **Pin 2** | `PB5` (Silkscreen "A7") | **Pin 6** | `MOSI` | SPI1 Master-Out Slave-In |
| **Pin 3** | `PA6` | **Pin 7** | `MISO` | SPI1 Master-In Slave-Out |

**Supported Applications:**
* **MouseJacker**: Wireless mouse/keyboard injection.
* **NRF24 Sniffer**: 2.4 GHz packet capture and promiscuous sniffing.
* **BLE Beacon**: Bluetooth Low Energy advertisement transmitter.
* **Spectrum Analyzer**: 2.4 GHz channel activity visualizer.

---

### **Q20: Why does the NRF24 module crash, drop packets, or reboot the Flipper during transmission?**
> [!CAUTION]
> **NRF24L01+ modules require dedicated power decoupling.**
> During transmission bursts, the radio's power consumption surges rapidly. Because breakout jumper wires introduce parasitic inductance, the supply voltage dips and crashes the module or causes MCU brownouts.

**Fix:**
* Solder a **10 µF electrolytic or tantalum capacitor** in parallel with a **100 nF ceramic capacitor** directly across `VCC` (Pin 2) and `GND` (Pin 1) of the NRF24 module.
* Keep jumper wires between the Flipper and NRF24 under 10 cm in length.

---

## 🤝 Project Links & Community

* **GitHub Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)
* **Bug Reports & Discussions**: [GitHub Issues](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/issues)
* **License**: [GNU General Public License v3.0](LICENSE)

