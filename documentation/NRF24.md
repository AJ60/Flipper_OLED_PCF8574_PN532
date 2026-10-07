# 📻 2.4 GHz NRF24 Protocol & Sniffer Module

> Documentation for interfacing Nordic Semi NRF24L01+ 2.4 GHz transceivers with the DIY Flipper Zero (OLED Edition).

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

> [!NOTE]
> **External Apps**: NRF24 support is provided via external applications (`.fap` files) located in `/ext/apps/GPIO/` or `/ext/apps/Protocols/`. Available apps:
> - `[NRF24] Scanner`: 2.4 GHz frequency occupancy and carrier scanner.
> - `[NRF24] Sniffer`: Promiscuous packet sniffer for ESB (Enhanced ShockBurst) packets.
> - `[NRF24] MouseJacker`: Keystroke injection tool for unencrypted wireless mice/keyboards.
> - `[NRF24] Channel Scanner`: Waterfall RSSI spectrum plot across 125 channels.

---

> [!CAUTION]
> ⚖️ **EDUCATIONAL & RESEARCH USE ONLY**:
> These wireless sniffing and injection tools are provided strictly for educational purposes, wireless security research, and authorized personal testing. Strictly prohibited from unauthorized interception or disruption of wireless peripherals.

---

## 🔌 Hardware Wiring Diagram

```
WeAct STM32WB55 (Flipper Header)               Nordic NRF24L01+ Module
  ┌────────────────────────────┐                  ┌────────────────────────┐
  │ Pin 9 (3.3V Rail)          ├─────[ 3.3V ]─────┤ Pin 2: VCC (3.3V ONLY!)│
  │ Pin 8 (GND Rail)           ├─────[ GND ]──────┤ Pin 1: GND             │
  │ Pin 4 (PA4 - Ext SPI CS)   ├─────[ CSN ]──────┤ Pin 4: CSN (SPI CS)    │
  │ Pin 6 (PB2 - GPIO)         ├─────[ CE ]───────┤ Pin 3: CE (Chip Enable)│
  │ Pin 5 (PB3 - SPI1 SCK)     ├─────[ SCK ]──────┤ Pin 5: SCK (SPI Clock) │
  │ Pin 2 (PB5 - SPI1 MOSI)    ├─────[ MOSI ]─────┤ Pin 6: MOSI (Data In)  │
  │ Pin 3 (PA6 - SPI1 MISO)    ├─────[ MISO ]─────┤ Pin 7: MISO (Data Out) │
  │ *(Unconnected / NC)*       │                  │ Pin 8: IRQ (Interrupt) │
  └────────────────────────────┘                  └────────────────────────┘
```

### 📋 Complete Pinout & Connection Matrix

| Flipper Pin # | MCU Pin | WeAct Header Label | NRF24L01+ Pin | Signal Name | Electrical Role / Function |
|:---:|:---:|:---:|:---:|:---:|---|
| **Pin 9** | `3.3V` | **`3V3`** | **Pin 2** | `VCC` | **3.3V DC power rail** *(DO NOT use 5V!)* |
| **Pin 8** | `GND` | **`G` / `GND`** | **Pin 1** | `GND` | Common system ground reference |
| **Pin 4** | `PA4` | **`A4`** | **Pin 4** | `CSN` | SPI1 Chip Select *(Active-Low, asserted by driver)* |
| **Pin 6** | `PB2` | **`B2`** | **Pin 3** | `CE` | Chip Enable *(Active-High RX/TX packet trigger)* |
| **Pin 5** | `PB3` | **`B3`** | **Pin 5** | `SCK` | SPI1 Clock *(Up to 10 MHz)* |
| **Pin 2** | `PB5` | **`A7`** *(MCU PB5)* | **Pin 6** | `MOSI` | SPI1 Master-Out Slave-In *(Data to NRF24)* |
| **Pin 3** | `PA6` | **`A6`** | **Pin 7** | `MISO` | SPI1 Master-In Slave-Out *(Data from NRF24)* |
| — | — | — | **Pin 8** | `IRQ` | *Not Connected* *(FAP apps poll status registers)* |

---

## ⚠️ Critical Engineering Notes

1. **Header Labeling Quirk**:
   - The WeAct pin header labeled **`A7`** connects to MCU pin **`PB5`** (`SPI1 MOSI`).
   - The physical MCU pin `PA7` is reserved for `I2C3 SCL` (NFC PN532).
   - Wire NRF24 `MOSI` directly to the header pin labeled **`A7`**.

2. **SPI1 Bus Arbitration**:
   - `SPI1` is shared between the MicroSD card (`CS: PA10`), internal CC1101 (`CS: PA15`), and external NRF24 (`CS: PA4`).
   - The firmware guarantees mutual exclusion: when an NRF24 application acquires the external SPI bus, other device CS lines remain deasserted (HIGH).

3. **Power Stability & Decoupling**:
   - NRF24 modules (particularly high-power `+PA+LNA` models with metal shield can and SMA antenna) draw up to 150 mA current bursts during packet bursts.
   - Without local decoupling, VCC voltage dips below 2.7V, causing the NRF24 to reset or fail packet reception ("Scanner black screen").
   - **Recommended Modification**: Solder a **10 µF electrolytic or tantalum capacitor** and a **100 nF ceramic capacitor** directly across `Pin 1 (GND)` and `Pin 2 (VCC)` on the NRF24 board header.
