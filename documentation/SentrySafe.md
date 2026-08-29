# 🔐 Sentry Safe & Master Lock Electronic Safe Research

> Educational vulnerability analysis and electronic bypass research documentation.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

> [!CAUTION]
> ⚖️ **STRICTLY FOR AUTHORIZED SECURITY RESEARCH ONLY**:
> This documentation and plugin are provided strictly for educational purposes and authorized penetration testing on equipment you own. Any unauthorized access to third-party safes or property is illegal and strictly prohibited.

---

## 🛠️ Hardware Wiring & Usage

* Original vulnerability research: [H4ckd4ddy](https://github.com/H4ckd4ddy/bypass-sentry-safe).

### Pin Connections:
* `Flipper GPIO 8 / GND` ➔ **Black wire** (Safe solenoid circuit)
* `Flipper GPIO 15 / PC1` ➔ **Green wire** (Safe data line)

### Usage Flow:
1. Open the **Sentry Safe** application on Flipper.
2. Connect wires as prompted on screen.
3. Press **OK** to send unlock pulse sequence.
