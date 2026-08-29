# 📻 2.4 GHz NRF24 Protocol & Sniffer Module

> Documentation for interfacing Nordic Semi NRF24L01+ 2.4 GHz transceivers with the DIY Flipper Zero.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

> [!NOTE]
> **External App**: NRF24 support is provided via external applications (`.fap` files), not built-in firmware features. Install the NRF24 apps to your SD card to use them. Available apps: `nrf24scan`, `nrf24sniff`, `nrf24mousejacker`, `nrf24batch`, `nrf24channelscanner`.

---

> [!CAUTION]
> ⚖️ **EDUCATIONAL & RESEARCH USE ONLY**:
> These wireless sniffing and injection tools are provided strictly for educational purposes, wireless security research, and authorized personal testing. Strictly prohibited from unauthorized interception or disruption of wireless peripherals.

---

## 🔌 Hardware Wiring (NRF24L01+ to Flipper GPIO)

| Flipper Pin | MCU Pin | NRF24L01+ Pin | Function |
|---|---|---|---|
| **Pin 2** | `PB5` (header "PA7") | **Pin 6** | `MOSI` |
| **Pin 3** | `PA6` | **Pin 7** | `MISO` |
| **Pin 4** | `PA4` | **Pin 4** | `CSN` (Chip Select) |
| **Pin 5** | `PB3` | **Pin 5** | `SCK` (SPI Clock) |
| **Pin 6** | `PB2` | **Pin 3** | `CE` (Chip Enable) |
| **Pin 8** | `GND` | **Pin 1** | Ground (`GND`) |
| **Pin 9** | `3.3V` | **Pin 2** | Power (`3.3V` — NOT 5V!) |

> [!NOTE]
> **Header "PA7" is wired to MCU PB5** (SPI1 MOSI). The physical PA7 MCU pin is used for I2C3 SCL (PN532 NFC). Do not confuse the header label with the MCU pin.

> [!NOTE]
> **SPI1 Bus Sharing**: SPI1 is shared with the MicroSD card (CS: PA10) and CC1101 radio (CS: PA15). Only one device can use the bus at a time.

> [!TIP]
> **Power Decoupling**: NRF24L01+ modules are sensitive to supply noise. Solder a **3.3 µF – 10 µF electrolytic capacitor** and a **0.1 µF ceramic capacitor** directly across `VCC` (Pin 2) and `GND` (Pin 1) of the NRF24 module for stable packet transmission.
