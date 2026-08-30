# 📐 Hardware & Electrical Engineering Guide: DIY Flipper Zero (OLED Edition)

**Target MCU**: STM32WB55CGU6 (UFQFPN48 package)  
**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Notice**: ⚖️ *Strictly for educational and academic engineering research purposes only.*

> [!WARNING]
> 🚧 **125 kHz LF-RFID Analog Hardware Status**:
> The discrete analog front-end for 125 kHz LF-RFID is **under active development / experimental**. Signal demodulation and card emulation reliability depend heavily on coil inductance tolerances, antenna tuning, and diode forward voltage thresholds. It may contain bugs or require component tuning on custom breadboards.

---

## 1. Complete Pinout & Bus Mapping Matrix

```mermaid
graph LR
    subgraph STM32WB55 [WeAct STM32WB55CGU6 MCU]
        PA0[PA0 - IR Receiver RX]
        PA1[PA1 - LF-RFID RX / CC1101 GDO0]
        PA2[PA2 - LF-RFID Emulate / PN532 IRQ]
        PA3[PA3 - 1-Wire iButton Probe]
        PA5[PA5 - LF-RFID 125kHz TX Carrier]
        PA6[PA6 - SPI1 Shared MISO]
        PA7["PA7 (header C0) - I2C3 PN532 SCL"]
        PA8[PA8 - IR Transmitter TX]
        PA9[PA9 - I2C1 Shared SCL]
        PA10[PA10 - MicroSD Card SPI CS]
        PA15[PA15 - CC1101 Radio SPI CS]
        PB0[PB0 - PCF8574 Keypad Ext INT]
        PB1[PB1 - INA219/226 Power ALERT]
        PB3[PB3 - SPI1 Shared SCK]
        PB4["PB4 (header C1) - I2C3 PN532 SDA"]
        PB5[PB5 - SPI1 Shared MOSI]
        PB8[PB8 - Piezo Speaker Buzzer]
        PB9[PB9 - I2C1 Shared SDA]
        PE4[PE4 - ST25R3916 SPI CS (Alternative NFC)]
    end
```

---

## 2. 125 kHz LF-RFID Analog Signal Chain

The LF-RFID subsystem uses a discrete analog front-end driven directly by hardware timer PWM and sampled via input capture:

```mermaid
graph LR
    PWM[PA5: TIM2_CH1 125kHz PWM] --> PushPull[Q1/Q2 Push-Pull Stage BC337/BC327]
    PushPull --> Tank[Resonant LC Tank: L1 1.2mH + C1 2.2nF]
    Tank --> Diode[D1: 1N4148 / BAT54S Schottky Envelope Detector]
    Diode --> Filter[RC Low-Pass Filter: R3 10k + C3 1nF]
    Filter --> OpAmp[U1: LM358 / LM2904 AC Coupled Gain Stage]
    OpAmp --> RX_Pin[PA1: TIM1_CH1 Input Capture Demodulated Envelope]

    Emulate_Pin[PA2: TIM2_CH3 Pulse] --> Emulate_SW[Q3: 2N2222 Tag Emulation Shunt]
    Emulate_SW -.-> Tank
```

### Resonant Tank Calculation:
$$f_0 = \frac{1}{2 \pi \sqrt{L \cdot C}} = \frac{1}{2 \pi \sqrt{1.2 \times 10^{-3} \cdot 2.2 \times 10^{-9}}} \approx 125.02\text{ kHz}$$

---

## 3. I2C Bus Coexistence & Pull-Up Engineering

The primary `I2C1` bus multiplexes three active peripherals:
1. **SSD1306 / SH1106 OLED Display** (7-bit address `0x3C`)
2. **PCF8574 Keypad & Haptic Expander** (7-bit address `0x20`)
3. **INA219 / INA226 Battery Fuel Gauge** (7-bit address `0x40`)

```
   3.3V Rail
      │
      ├───[ 2.2 kΩ R_pullup ]───┐
      │                         │
      ├───[ 2.2 kΩ R_pullup ]───┼───────────┐
      │                         │           │
     GND                        │           │
      │                        SCL         SDA
      ▼                       (PA9)       (PB9)
                                │           │
  ┌─────────────────────────────┼───────────┼────────────────────────┐
  │                             │           │                        │
  │   ┌───────────────┐         │           │   ┌────────────────┐   │
  │   │ SSD1306 OLED  │─────────┴───────────┼───│ PCF8574 Expander│  │
  │   │ (Addr: 0x3C)  │                     │   │ (Addr: 0x20)   │   │
  │   └───────────────┘                     │   └──────┬─────────┘   │
  │                                         │          │             │
  │                                         │          ▼             │
  │   ┌───────────────┐                     │        PB0 INT         │
  │   │ INA219 Monitor│─────────────────────┘      (Ext Interrupt)   │
  │   │ (Addr: 0x40)  │                                              │
  │   └───────┬───────┘                                              │
  │           │                                                      │
  │           ▼                                                      │
  │        PB1 ALERT                                                 │
  │     (Power Alert)                                                │
  └──────────────────────────────────────────────────────────────────┘
```

> [!TIP]
> **Total Bus Capacitance & Noise Immunity**:
> Standard jumper wires add ~15–30 pF capacitance per node. Adding physical **2.2 kΩ or 3.3 kΩ external pull-ups** ensures fast rise times (<300 ns) at 400 kHz fast-mode I2C and provides robust RF noise immunity during Sub-GHz and NFC transmissions.

---

## 4. Dedicated NFC I2C3 Bus & IRQ Architecture

The **NXP PN532 NFC Controller** is isolated on dedicated hardware **I2C3** with asynchronous interrupt handling on **PA2**:

* **I2C3 Clock (`SCL`)**: `PA7` (labeled **`C0`** on WeAct board header)
* **I2C3 Data (`SDA`)**: `PB4` (labeled **`C1`** on WeAct board header)
* **Data Ready IRQ (`INT0`)**: `PA2` (labeled **`A2`** on WeAct board header, active-low open-drain)

```
WeAct STM32WB55                     NXP PN532 NFC Controller
  ┌──────────────┐                     ┌─────────────────────┐
  │     PA7 (C0) ├──────[ I2C3 SCL ]───┤ SCL (0x24)          │
  │     PB4 (C1) ├──────[ I2C3 SDA ]───┤ SDA                 │
  │     PA2 (A2) │<─────[ EXTI2 IRQ ]──┤ IRQ / INT0          │
  │         3.3V ├──────[ 3.3V Rail ]──┤ VCC                 │
  │          GND ├──────[ Common Gnd]──┤ GND                 │
  └──────────────┘                     └─────────────────────┘
```

> [!NOTE]
> For complete engineering details on hardware Crypto1 acceleration, ISO 14443-4 APDU wrapping for bank cards, and advantages/disadvantages, see [`NFC_PN532_ENGINEERING.md`](NFC_PN532_ENGINEERING.md).

---

## 5. Power Decoupling & Filtering Guidelines

To ensure stable operation under peak RF loads:
* Solder a **0.1 µF (100 nF)** ceramic capacitor directly across the `VCC` and `GND` pins of the **PCF8574** expander and **PN532** module.
* Solder a **10 µF** tantalum or electrolytic capacitor across the 3.3V power rail near the **CC1101** and **PN532** modules to absorb 13.56 MHz carrier inrush surges.
* Ensure all ground lines return directly to the WeAct board's `GND` ground plane (star ground topology).
