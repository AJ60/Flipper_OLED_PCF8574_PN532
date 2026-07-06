# ❓ DIY Flipper Zero — FAQ & Troubleshooting Guide 🐬

Welcome to the frequently asked questions and troubleshooting guide for the DIY Flipper Zero (SSD1306 OLED & MCP23017 Keyboard edition). 

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
6. Click the **"Repair"** button. This writes the official Flipper bootloader.
7. Once finished, click **"Install from file"** on the main screen and choose our custom `.dfu` or `.tgz` firmware.

---

### **Q2: During the `.tgz` update, my screen went completely black. Is it bricked?**
> [!WARNING]
> **This is expected behavior!** The built-in firmware updater runs outside the main OS and only supports the original SPI screen. Since our DIY board uses an I2C OLED display, the screen will go black during the update.

**Solution:**
*   **DO NOT unplug the USB cable!**
*   Monitor the installation progress in the qFlipper application on your PC.
*   Once qFlipper reports **"Update Successful!"**, the device will automatically reboot, launch the main OS, and the OLED screen will turn back on. This takes 1-2 minutes.

---

### **Q3: Can I reboot into DFU mode using the Flipper settings menu?**
**Solution:**
*   **No.** Due to the custom keyboard interface via the MCP23017 I2C expander, software-triggered DFU reboot is not supported by the bootloader.
*   Use the hardware method instead (hold BOOT0 while plugging in the USB cable).

---

### **Q4: Why is the CC1101 PPM calibration set to +100 PPM?**
> [!TIP]
> Cheap CC1101 modules often use low-tolerance crystals that drift from the target frequency.

**Solution:**
*   Testing showed an average hardware crystal offset of **-32 kHz** (about -74 PPM).
*   Configuring the PPM calibration to **+100 PPM** (under *Momentum App -> Protocols -> Sub-GHz -> Calib*) shifts the frequency back to the standard **433.920 MHz**, ensuring stable decoding by other Flipper Zero units.

---

### **Q5: How and where can I view the system logs of the DIY Flipper?**
**Solution:**
There are two primary ways to view real-time system logs from your device:

1. **Via the qFlipper Desktop App:**
   * Open the **qFlipper** application on your PC and connect your device.
   * Click on the **"Log"** button (represented by a terminal or notepad icon in the bottom left, or press `Ctrl + L` / `Cmd + L`).
   * The app will display real-time console messages from the device. You can change the log level in the Flipper's settings.

2. **Via a Serial Terminal (UART over USB):**
   * The Flipper Zero exposes a virtual COM port when connected via USB. You can use any serial terminal client (like PuTTY, Tera Term, or `screen`/`minicom` on Linux/macOS) to connect.
   * Configure the connection to use the correct COM port of your Flipper (baud rate is virtual and does not matter, but standard is `115200`).
   * Once connected, type `log` and press Enter to start streaming logs. Press `Ctrl + C` to stop the log stream.

> [!TIP]
> To change the detail level of logs, go to **Settings > System > Log Level** directly on your Flipper Zero screen.

---

## 🔌 Hardware Troubleshooting

### **Q6: Keyboard buttons freeze or stop responding periodically. Why?**
> [!CAUTION]
> Sub-GHz transmission or NFC activity generates strong RF noise that couples into the I2C1 bus, freezing the MCP23017 IO expander.

**Solution:**
1.  **Check physical Pull-up resistors:** Solder **2.2 kΩ** or **3.3 kΩ** resistors between the I2C1 SDA/SCL lines and the 3.3V rail.
2.  **Avoid internal pull-ups:** The MCU's internal pull-ups are too weak (~40 kΩ) to protect the bus against RF noise.
3.  **Add a capacitor:** Solder a **0.1 µF (100 nF)** ceramic capacitor as close as possible to the VCC/GND pins of the MCP23017.

---

### **Q7: What are the recommendations for power filtering (capacitors) on the board?**
**Solution:**
To suppress voltage transients during high-current operations (vibration motor clicks, NFC scans):
*   **Decoupling:** Solder **0.1 µF** ceramic capacitors close to the VCC/GND pins of each chip (MCP23017, OLED, INA219, NFC, CC1101).
*   **Bulk Filtering:** Solder a **10–47 µF** tantalum or electrolytic capacitor on the main 3.3V rail near the OLED screen.

---

### **Q8: Why does the vibration motor cause the board to reset or freeze?**
> [!CAUTION]
> **Never connect the vibration motor directly to the MCP23017 pins!** The motor's start-up current (60-100 mA) exceeds the expander's 25 mA limit. This will damage the pin or cause MCU resets.

**Solution:**
1.  Drive the motor using an **N-channel MOSFET** (e.g., `2N7002` or `AO3400`) as a switch.
2.  **Always** solder a flyback diode (such as `1N4148`) in parallel with the motor terminals (cathode to VCC, anode to the transistor) to clamp voltage spikes.

---

### **Q9: I get SD card read errors when using NFC at the same time. How do I fix this?**
> [!NOTE]
> The SD card, CC1101 module, and NFC module share the same physical SPI1 bus.

**Solution:**
*   Solder external **10 kΩ** pull-up resistors to the 3.3V rail on all Chip Select (CS) lines: SD (`PA10`), NFC (`PE4`), and CC1101 (`PA15`). This prevents multiple devices from enabling simultaneously during resets.

---

### **Q10: Dallas iButton keys (1-Wire, DS1990) are not reading on pin PA3.**
**Solution:**
*   The 1-Wire protocol requires a pull-up resistor. 
*   Solder a **2.2 kΩ** or **4.7 kΩ** resistor between the iButton data line (`PA3`) and the **3.3V** rail.

---

### **Q11: Can I connect a speaker directly to pin PB8?**
**Solution:**
*   **No**, if it is a standard low-impedance dynamic speaker (8-32Ω). Direct connection will burn out the MCU pin.
*   Only connect **passive piezo buzzers** directly to `PB8`. For dynamic speakers, use a transistor switch circuit (like `BC847` or `2N7002`).

---

### **Q12: Short IR range or poor NFC read performance on battery power.**
**Solution:**
*   The IR LEDs and the ST25R3916 NFC module operate at peak performance when powered by **5V**. On battery, the 5V rail is inactive.
*   **Solution:** Install a tiny 5V boost converter (e.g., based on the `MT3608` chip) to feed the IR and NFC circuits when running on battery.

---

### **Q13: The INA219 battery monitor reports incorrect current or battery percentage.**
**Solution:**
*   The firmware is calibrated for a **0.1 Ω** current shunt resistor (marked `R100`).
*   Ensure the shunt resistor installed on your board is exactly 0.1 Ω (1% tolerance).
