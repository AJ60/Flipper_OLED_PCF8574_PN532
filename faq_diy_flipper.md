# ❓ DIY Flipper Zero — FAQ & Troubleshooting Guide 🐬

Welcome to the frequently asked questions and troubleshooting guide for the DIY Flipper Zero (SSD1306 OLED & PCF8574 Keyboard edition). 

---

## 💿 Flashing & OS

### **Q1: I connected my new WeAct board, but qFlipper does not detect it. What should I do?**
> [!IMPORTANT]
> A blank WeAct STM32WB55 board lacks the Flipper bootloader and will be detected as a generic STM32 DFU device.

**Solution:**
1. Unplug the USB cable from the WeAct board.
2. Press and hold the physical **BOOT0** button on the WeAct board.
3. Plug the USB cable back into the PC (while holding the button).
4. Release the **BOOT0** button. The board is now in DFU mode.
5. Launch **qFlipper** — it will detect the board in recovery mode.
6. Click the **"REPAIR"** button. This writes the official Flipper bootloader.
7. Once finished, click **"Install from file"** on the main screen and choose our custom `.dfu` or `.tgz` firmware.

---

### **Q2: The firmware successfully flashes, but the device is frozen/unresponsive (blank screen, and qFlipper throws an "RPC Session/Protobuf Timeout" error). How to fix?**
> [!IMPORTANT]
> The custom firmware (v2.0+) features an active I2C bus recovery loop. If the I2C1 bus has no hardware pull-up resistors or is floating, the firmware gets stuck in an infinite boot loop trying to recover the bus, which freezes the USB connection.

**Solution:**
1.  **Connect your OLED screen:** Standard I2C OLED screens (SSD1306/SH1106) have built-in **4.7 kΩ pull-up resistors** on their SDA/SCL lines. If you try to boot the board *without* the display plugged in, the I2C bus floats and halts the boot process.
2.  **Solder physical I2C pull-ups:** Solder physical **2.2 kΩ or 3.3 kΩ resistors** between the SDA (`PB9`) -> 3.3V rail and SCL (`PA9`) -> 3.3V rail on the WeAct board. This stabilizes the PCF8574 keyboard, provides strong hardware pull-ups, and prevents boot loops.

---

### **Q3: During the `.tgz` update, my screen went completely black. Is it bricked?**
> [!WARNING]
> **This is expected behavior!** The built-in firmware updater runs outside the main OS and only supports the original SPI screen. Since our DIY board uses an I2C OLED display, the screen will go black during the update.

**Solution:**
*   **DO NOT unplug the USB cable!**
*   Monitor the installation progress in the qFlipper application on your PC.
*   Once qFlipper reports **"Update Successful!"**, the device will automatically reboot, launch the main OS, and the OLED screen will turn back on. This takes 1-2 minutes.

---

### **Q4: How and where can I view and enable Debug Logs on the DIY Flipper?**
**Solution:**
By default, the Flipper Zero filters out low-priority debug messages to save CPU cycles. There are two steps to enable and view real-time **Debug** logs:

#### 1. How to Enable Debug/Trace Logging:
*   **On the Flipper Screen:** Go to **Settings > System > Log Level** and change it from `Default`/`Info` to **`Debug`** (or **`Trace`** for maximum granularity).
*   **Via CLI Command:** If you are connected to the serial console, you can start streaming debug-level logs directly by running:
    ```bash
    log debug
    ```
    *(To stop the log stream, press `Ctrl + C`).*

#### 2. Where to View the Logs:
*   **Via qFlipper (Desktop App):**
    1. Open **qFlipper** and connect your Flipper.
    2. Click the **"Log"** button in the bottom left corner (represented by a terminal/notepad icon, or press `Ctrl + L` / `Cmd + L`).
*   **Via Serial Terminal (UART over USB):**
    1. Connect your Flipper to the PC using a USB cable.
    2. Open any serial terminal program (e.g., PuTTY, Tera Term, or run `screen`/`minicom` in Linux/macOS terminal).
    3. Select your Flipper's virtual COM port. (Baud rate is virtual and can be left at `115200`).
    4. Type `log debug` (or just `log`) and press Enter.

---

### **Q5: Can I reboot into DFU mode using the Flipper settings menu?**
**Solution:**
*   **No.** Due to the custom keyboard interface via the PCF8574 I2C expander, software-triggered DFU reboot is not supported by the bootloader.
*   Use the hardware method instead (hold BOOT0 while plugging in the USB cable).

---

### **Q6: Why is the CC1101 PPM calibration set to +100 PPM?**
> [!TIP]
> Cheap CC1101 modules often use low-tolerance crystals that drift from the target frequency.

**Solution:**
*   Testing showed an average hardware crystal offset of **-32 kHz** (about -74 PPM).
*   Configuring the PPM calibration to **+100 PPM** (under *Momentum App -> Protocols -> Sub-GHz -> Calib*) shifts the frequency back to the standard **433.920 MHz**, ensuring stable decoding by other Flipper Zero units.

---

## 🔌 Hardware Troubleshooting

### **Q7: Keyboard buttons freeze or stop responding periodically. Why?**
> [!CAUTION]
> Sub-GHz transmission or NFC activity generates strong RF noise that couples into the I2C1 bus, freezing the PCF8574 IO expander.

**Solution:**
1.  **Check physical Pull-up resistors:** Solder **2.2 kΩ** or **3.3 kΩ** resistors between the I2C1 SDA/SCL lines and the 3.3V rail.
2.  **Avoid internal pull-ups:** The MCU's internal pull-ups are too weak (~40 kΩ) to protect the bus against RF noise.
3.  **Add a capacitor:** Solder a **0.1 µF (100 nF)** ceramic capacitor as close as possible to the VCC/GND pins of the PCF8574.

---

### **Q8: What are the recommendations for power filtering (capacitors) on the board?**
**Solution:**
To suppress voltage transients during high-current operations (vibration motor clicks, NFC scans):
*   **Decoupling:** Solder **0.1 µF** ceramic capacitors close to the VCC/GND pins of each chip (PCF8574, OLED, INA219, NFC, CC1101).
*   **Bulk Filtering:** Solder a **10–47 µF** tantalum or electrolytic capacitor on the main 3.3V rail near the OLED screen.

---

### **Q9: Why does the vibration motor cause the board to reset or freeze?**
> [!CAUTION]
> **Never connect the vibration motor directly to the PCF8574 pins!** The motor's start-up current (60-100 mA) exceeds the expander's 25 mA limit. This will damage the pin or cause MCU resets.

**Solution:**
1.  Drive the motor using an **N-channel MOSFET** (e.g., `2N7002` or `AO3400`) as a switch.
2.  **Always** solder a flyback diode (such as `1N4148`) in parallel with the motor terminals (cathode to VCC, anode to the transistor) to clamp voltage spikes.

---

### **Q10: My SD card disconnects immediately when I plug in the NFC module (e.g. SCLK to PB3), or I get SD read errors. How do I fix this?**
> [!IMPORTANT]
> The SD card and NFC module share the SPI1 SCK/MOSI lines, but **they must use separate MISO pins** because the ST25R3916 chip does not release the MISO line (doesn't go High-Z) when deactivated.

**Solution:**
1.  **Check MISO wiring:** Ensure **NFC MISO** is connected to **PB4** (`gpio_nfc_miso`), while the **SD Card MISO** connects to **PA6** (`gpio_spi_miso`). If they share PA6, the NFC chip will block the bus and disable the SD card.
2.  **Pull-up resistors:** Solder external **10 kΩ** pull-up resistors to the 3.3V rail on all Chip Select (CS) lines: SD (`PA10`), NFC (`PE4`), and CC1101 (`PA15`). This prevents multiple devices from enabling simultaneously during resets.

---

### **Q11: Dallas iButton keys (1-Wire, DS1990) are not reading on pin PA3.**
**Solution:**
*   The 1-Wire protocol requires a pull-up resistor. 
*   Solder a **2.2 kΩ** or **4.7 kΩ** resistor between the iButton data line (`PA3`) and the **3.3V** rail.

---

### **Q12: Can I connect a speaker directly to pin PB8?**
**Solution:**
*   **No**, if it is a standard low-impedance dynamic speaker (8-32Ω). Direct connection will burn out the MCU pin.
*   Only connect **passive piezo buzzers** directly to `PB8`. For dynamic speakers, use a transistor switch circuit (like `BC847` or `2N7002`).

---

### **Q13: Short IR range or poor NFC read performance on battery power.**
**Solution:**
*   The IR LEDs and the ST25R3916 NFC module operate at peak performance when powered by **5V**. On battery, the 5V rail is inactive.
*   **Solution:** Install a tiny 5V boost converter (e.g., based on the `MT3608` chip) to feed the IR and NFC circuits when running on battery.

---

## 🔌 PN532 V3 NFC Troubleshooting (I2C)

### **Q15: NFC is not working after flashing firmware. The app shows "NFC not found" or similar errors.**
> [!IMPORTANT]
> This firmware uses a **PN532 V3 module over I2C** (not the ST25R3916 SPI module). Ensure your wiring matches the table below.

**Solution:**
1. **Check I2C wiring:**
   | PN532 Pin | WeAct Pin | Function |
   |-----------|-----------|----------|
   | SCL       | PA7 (I2C3_SCL) | I2C Clock |
   | SDA       | PB4 (I2C3_SDA) | I2C Data |
   | VCC       | 3.3V | Power (3.3V, NOT 5V) |
   | GND       | GND | Ground |
   | IRQ       | PA2 | Interrupt (active-low) |

2. **Verify I2C address:** The PN532 uses address `0x24` (7-bit). Some modules have a different address — check your module's documentation.
3. **Check pull-up resistors:** The PN532 module typically includes 10 kΩ pull-ups to 3.3V on SDA/SCL. If not, add 4.7 kΩ pull-ups to 3.3V on both lines.
4. **Check IRQ connection:** The IRQ pin (PA2) must be connected for card detection to work. Without it, the firmware will timeout waiting for interrupts.

---

### **Q16: NFC reads are intermittent or fail randomly.**
> [!CAUTION]
> I2C bus noise from RF activity can cause communication errors with the PN532.

**Solution:**
1. **Add I2C pull-ups:** Ensure 4.7 kΩ pull-ups are present on SDA/SCL (PA7/PB4). The PN532's internal pull-ups may be insufficient.
2. **Shorten wiring:** Keep I2C wires as short as possible (< 10 cm). Long wires act as antennas for RF noise.
3. **Add decoupling:** Solder a 0.1 µF ceramic capacitor close to the PN532's VCC/GND pins.
4. **Check ground:** Ensure a solid ground connection between the PN532 and the WeAct board. Ground loops cause intermittent failures.

---

### **Q17: Can I use both the PN532 and the OLED screen at the same time?**
> [!TIP]
> The PN532 uses I2C3 (PA7/PB4) while the OLED uses I2C1 (PA9/PB9). They are on separate buses and do not conflict.

**Solution:**
*   **Yes.** The two devices are on different I2C buses and can operate simultaneously without issues.
*   The PCF8574 keyboard is also on I2C1, sharing the bus with the OLED. This is normal and supported.

---

### **Q18: Does the PN532 support card emulation (reader acts as a card)?**
**Solution:**
*   **Yes.** The PN532 supports `TgInitAsTarget` for card emulation. The firmware implements this via `pn532_tg_init_as_target`.
*   The PN532 emulates a MIFARE Classic 1K target by default. Other card types may require additional configuration.

---

### **Q19: Which NFC card types are supported with the PN532?**
> [!TIP]
> The PN532 supports fewer protocols than the ST25R3916 in this firmware build.

**Supported:**
*   ISO14443A (MIFARE Classic, MIFARE Ultralight, NTAG)
*   ISO14443B (card detection only)
*   ISO15693 (card detection only)
*   FeliCa (card detection only)
*   Card emulation (MIFARE Classic 1K target)

**Not supported (PN532 limitation):**
*   ISO15693 listener (card emulation)
*   FeliCa listener (card emulation)
*   ST25TB (tight timing requirements not met by PN532's `InDataExchange`)
*   ISO14443A listener with full transparent mode (uses `TgInitAsTarget` instead)

---

### **Q14: The INA219 battery monitor reports incorrect current or battery percentage.**
**Solution:**
*   The firmware is calibrated for a **0.1 Ω** current shunt resistor (marked `R100`).
*   Ensure the shunt resistor installed on your board is exactly 0.1 Ω (1% tolerance).
