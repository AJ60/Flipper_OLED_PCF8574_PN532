# 🎮 Key Combos & Hardware Recovery — DIY Flipper Zero (OLED Edition)

> Quick reference guide for button combinations, hardware reset, recovery modes, and DFU flashing on the DIY Flipper Zero (OLED Edition).

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

## 🛠️ DIY Flipper Zero (OLED Edition) Button Architecture

On this DIY hardware build, buttons are wired to a **PCF8574 remote 8-bit I/O expander** over the shared **I2C1** bus (`PA9` SCL, `PB9` SDA) with active-low logic:

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

---

## ⚡ Recovery & Reset Methods for DIY Hardware

### 1. Hardware Reboot / Reset (Physical Button)
* **Method**: Press the physical **NRST / RESET** tactile button on the WeAct STM32WB55 development board.
* **Effect**: Pulls the hardware `NRST` pin low, triggering an immediate power-on reset vector.

---

### 2. Entering STM32 USB DFU Mode (Flashing & Recovery)
* **Method**:
  1. Unplug the USB cable from the WeAct board.
  2. Press and **hold** the physical **BOOT0** button on the WeAct board.
  3. Plug the USB cable into your PC (while continuing to hold BOOT0).
  4. Release the **BOOT0** button.
* **Effect**: Forces the internal STM32 boot ROM into USB DFU mode for 1-click bootloader repair in qFlipper or flashing via `generate_otp_gui.exe` and `STM32_Programmer_CLI`.

---

### 3. In-OS Shortcut Combos (Normal Operation)

| Action | Combo | Description |
|---|---|---|
| **Return to Desktop** | Press `BACK` | Exits the active app and returns to the main pet screen. |
| **Quick Action / Favorite** | Hold `OK` (on Desktop) | Opens the quick-access menu for favorite scripts/apps. |
| **System Info** | Press `UP` (on Desktop) | Opens device info, battery state, and firmware build details. |
| **Lock Device** | Hold `UP` (on Desktop) | Locks keypad inputs to prevent unintended presses in pocket. |
