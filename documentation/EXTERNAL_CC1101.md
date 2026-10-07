# 📻 External CC1101 Sub-GHz Transceiver Engineering Guide

> Complete hardware wiring, RF front-end switching, and software integration guide for external CC1101 modules on the DIY Flipper Zero (OLED Edition).

**Target Modules**: 
- **Ebyte E07-433M20S** (433 MHz, +20 dBm / 100 mW with onboard PA/LNA & RF Switch)
- **Ebyte E07-900M20S** (868/915 MHz, +20 dBm / 100 mW with onboard PA/LNA)
- **Generic CC1101 Breakout Modules** (Standard 3.3V green/blue boards without external PA)

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

## 1. Hardware Overview & Features

While the DIY Flipper Zero includes an onboard CC1101 transceiver on internal `SPI1` (`CS: PA15`), adding an **external high-power CC1101 module** (such as the Ebyte E07 series) provides:
* **Extended Range**: Up to +20 dBm (100 mW) transmission power via dedicated RF power amplifier (PA).
* **Enhanced Sensitivity**: High-gain low-noise amplifier (LNA) for long-range reception of weak weather station, TPMS, and car keyfob signals.
* **Frequency Flexibility**: Swap between dedicated 433 MHz tuned modules and 868/915 MHz tuned modules.
* **SMA Antenna Connector**: Support for directional high-gain yagi, whip, or telescopic antennas.

---

## 2. Complete Wiring Diagram (Ebyte E07-433M20S)

```
WeAct STM32WB55 (Flipper Header)               Ebyte E07-433M20S Module
  ┌────────────────────────────┐                  ┌────────────────────────┐
  │ Pin 9 (3.3V Rail)          ├─────[ 3.3V ]─────┤ VCC (3.3V DC Power)    │
  │ Pin 8 (GND Rail)           ├─────[ GND ]──────┤ GND                    │
  │ Pin 4 (PA4 - Ext SPI CS)   ├─────[ CSN ]──────┤ CSN (Chip Select)      │
  │ Pin 5 (PB3 - SPI1 SCK)     ├─────[ SCK ]──────┤ SCK (SPI Clock)        │
  │ Pin 2 (PB5 - SPI1 MOSI)    ├─────[ MOSI ]─────┤ MOSI (Data In)         │
  │ Pin 3 (PA6 - SPI1 MISO)    ├─────[ MISO ]─────┤ MISO (Data Out)        │
  │ Pin 6 (PB2 - Ext GDO0)     ├─────[ GDO0 ]─────┤ GDO0 (Async Data/IRQ)  │
  │ Onboard CC1101 GDO2        │═════[ RXEN ]═════│ GDO2 (Internal Switch) │
  │ Pin 7 (PA5 - Header C3/A5) ├─────[ TXEN ]─────┤ TXEN (PA Amp Enable)   │
  └────────────────────────────┘                  └────────────────────────┘
```

### 📋 Pin Connection Matrix

| Flipper Pin # | MCU Pin | WeAct Header Label | Ebyte E07 Pin | Generic CC1101 Pin | Description & Firmware Handling |
|:---:|:---:|:---:|:---:|:---:|---|
| **Pin 9** | `3.3V` | **`3V3`** | `VCC` | `VCC` | 3.3V DC power rail *(DO NOT connect directly to 5V!)* |
| **Pin 8** | `GND` | **`G` / `GND`** | `GND` | `GND` | System ground reference |
| **Pin 4** | `PA4` | **`A4`** | `CSN` | `CSN` | SPI1 Chip Select *(Asserted LOW during external transactions)* |
| **Pin 5** | `PB3` | **`B3`** | `SCK` | `SCK` | SPI1 Clock *(Up to 10 MHz)* |
| **Pin 2** | `PB5` | **`A7`** *(MCU PB5)* | `MOSI` | `MOSI` | SPI1 Master-Out Slave-In *(Data to CC1101)* |
| **Pin 3** | `PA6` | **`A6`** | `MISO` | `MISO` | SPI1 Master-In Slave-Out *(Data from CC1101)* |
| **Pin 6** | `PB2` | **`B2`** | `GDO0` | `GDO0` | Sub-GHz asynchronous sample capture & pulse IRQ |
| *Internal* | *CC1101 GDO2* | — | `RXEN` | `GDO2` (NC) | Onboard RF switch: driven HIGH in RX, driven LOW in TX/idle |
| **Pin 7** | `PA5` | **`C3`** *(Board `A5`)*| `TXEN` | *NC (Unused)* | Power Amplifier Enable: driven HIGH in TX, parked LOW in idle |

> [!NOTE]
> **Generic Green CC1101 Modules**:
> Generic modules lack the external PA/LNA circuit. You only need to connect:
> `VCC` (Pin 9), `GND` (Pin 8), `CSN` (Pin 4), `SCK` (Pin 5), `MOSI` (Pin 2), `MISO` (Pin 3), and `GDO0` (Pin 6). `Pin 7 (TXEN)` is left unconnected.

---

## 3. RF Front-End Control Architecture (E07 PA/LNA)

The Ebyte E07 series uses an integrated RF switch to route the antenna between the transmit Power Amplifier (PA) and receive Low-Noise Amplifier (LNA). The control logic is **active-high**:

| Radio State | RXEN (CC1101 GDO2) | TXEN (MCU PA5 / Header Pin 7) | Antenna Path & PA Status |
|---|:---:|:---:|---|
| **Receive (`RX`)** | **HIGH** (`1`) | **LOW** (`0`) | LNA Active, PA Disabled |
| **Transmit (`TX`)** | **LOW** (`0`) | **HIGH** (`1`) | PA Active (+20 dBm output), LNA Disabled |
| **Idle / Standby** | **LOW** (`0`) | **LOW** (`0`) | Both Amplifiers Powered Down |
| **Deep Sleep (`SPWD`)**| **LOW** (`0`) | **LOW** (`0`) | Lowest power standby state |

### Firmware Control Implementation:
1. **GDO2 / RXEN Control**:
   - In [`cc1101_ext.c`](../applications/drivers/subghz/cc1101_ext/cc1101_ext.c), `IOCFG2` is written as `CC1101IocfgHW | CC1101_IOCFG_INV` (drive high) when entering RX mode.
   - In idle, reset, sleep, and transmit states, `IOCFG2` is driven low with `CC1101IocfgHW`. The pin is **never** left floating in high-impedance mode.
2. **TXEN Control & PA5 Safety**:
   - MCU pin `PA5` (Header Pin 7 / `C3`) is mapped to `SUBGHZ_DEVICE_CC1101_EXT_E07_TXEN_GPIO`.
   - Driven HIGH (`1`) immediately before transmitting in `subghz_device_cc1101_ext_tx`.
   - Driven LOW (`0`) and parked output-low unconditionally in alloc, init, sleep, and free routines.
3. **Coexistence with 125 kHz LF-RFID**:
   - On this DIY board, MCU `PA5` also drives the LF-RFID 125 kHz carrier stage (`TIM2_CH1`).
   - Because `cc1101_ext_txen_park_low()` parks `PA5` low whenever Sub-GHz is not transmitting, the LF-RFID coil circuit is not energized, avoiding unwanted carrier emission or card read disruption.

---

## 4. SPI Bus Chip Select (CS) Assertion

The external CC1101 module shares `SPI1` with the internal CC1101 (`CS: PA15`) and MicroSD card (`CS: PA10`).
- In [`furi_hal_spi_config.c`](../targets/f7/furi_hal/furi_hal_spi_config.c), the external handle event callback asserts `PA4 = LOW` on `FuriHalSpiBusHandleEventActivate` and returns it `HIGH` on `FuriHalSpiBusHandleEventDeactivate`.
- MISO, MOSI, and SCK are configured with internal **pull-down resistors** (`GpioPullDown`). When no external module is attached, MISO does not float high, preventing 250 ms bus timeouts (`"MISO stuck high"`).

---

## 5. Software Usage & Global Radio Persistence

### How to Select the External Radio:
1. On your Flipper OLED, open the **Sub-GHz** main application.
2. Navigate to **Radio Settings → Module**.
3. Select **`External`**.
   * If detected: The setting switches to `External` and displays `Init OK (part=0x00 ver=0x14)`.
   * If not detected: The UI emits a warning log and reverts to `Internal`.

### Global Setting Persistence (Version 4):
The module choice is saved to the microSD card at:
```
/ext/subghz/assets/last_subghz.settings
```
Under key:
```
RadioDevice: 2    # 1 = Internal, 2 = External
```

### Automatic External App Integration:
All external Sub-GHz applications (`.fap`) dynamically link to the centralized [`subghz_radio_device_loader`](../lib/subghz/subghz_radio_device_loader.h) in the firmware. When `RadioDevice` is set to `External`:
- **Protoview**
- **Weather Station**
- **Sub-GHz Bruteforcer**
- **Spectrum Analyzer**
- **POCSAG Pager**
- **TPMS Reader**
- **Tanks Game**
- **Chief Cooker**
- **Esubghz Chat**
- **JS Sub-GHz Module**

All automatically power on OTG and route their RF traffic through the external E07 CC1101 module without requiring individual app reconfiguration.

---

## 6. Troubleshooting & Diagnostics

### Diagnostic Serial Logs:
Connect to the serial console (`log trace` or `log debug`) to inspect module probing:

* **Successful Detection**:
  ```
  [I][SubGhzDeviceCc1101Ext] check_init: part=0x00 ver=0x14
  [I][SubGhzDeviceCc1101Ext] Init OK (part=0x00 ver=0x14)
  [I][RadioDeviceLoader] External CC1101 detected and initialized (global selection)
  ```

* **Module Not Connected / Power Missing**:
  ```
  [D][SubGhzDeviceCc1101Ext] check_init: SPI released ret=0 (failed)
  [E][RadioDeviceLoader] External selected but CC1101 not detected; refusing internal fallback
  ```

* **Common Wiring Mistakes**:
  - **MOSI wired to wrong pin**: Header labeled `A7` is MCU `PB5` (MOSI). Ensure it is wired to `A7`, not MCU `PA7` (`C0`).
  - **Noisy Power Supply**: If the module intermittently fails to initialize, add a **10 µF tantalum capacitor** directly across `VCC` and `GND` of the CC1101 board.
  - **OTG 5.0V vs 3.3V**: The CC1101 chip is rated for **3.6V maximum**. Only supply 5V if using an external carrier board with an onboard 3.3V low-dropout (LDO) regulator (such as an AMS1117-3.3).
