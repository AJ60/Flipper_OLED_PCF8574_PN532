# 🏷️ Reading RAW 125 kHz RFID Data {#lfrfid_raw}

> Guide for sampling and analyzing raw analog 125 kHz RFID envelope signals for protocol debugging.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

> [!CAUTION]
> ⚖️ **EDUCATIONAL & RESEARCH USE ONLY**:
> This document and raw RFID sampling tools are intended strictly for educational purposes, signal analysis, and authorized transponder research. Unauthorized keycard duplication is prohibited.

> [!WARNING]
> 🚧 **ANALOG HARDWARE SENSITIVITY**:
> On DIY hardware builds, raw 125 kHz signal capture quality is dependent on analog coil tuning (~1.2 mH), resonant capacitance, and diode detector threshold. Refer to [`documentation/HARDWARE_DESIGN.md`](HARDWARE_DESIGN.md) for schematic details.

---

## 🛠️ Step-by-Step Raw RFID Capture

1. **Enable Debug Mode**:
   - Go to **Main Menu** → **Settings** → **System**.
   - Set **Debug** to **ON**.

2. **Capture Raw Signals**:
   - Go to **Main Menu** → **125 kHz RFID** → **Extra Actions**.
   - Select **Read RAW RFID Data** and enter a name for the capture file.
   - Hold your 125 kHz RFID tag near the DIY resonant coil.
   - Once reading finishes, press **OK** to save.

3. **Output Files**:
   - Two raw capture files (`.ask.raw` and `.psk.raw`) are stored in `/ext/lfrfid/` on the microSD card for waveform analysis.
