# 🏷️ Custom Device Name — DIY Flipper Zero (OLED Edition)

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

## How to change your device name

1. Go to `Settings` → `Misc` → `Spoofing Options` → `Flipper Name`
2. Enter your new custom name and click `Save` — **the name is saved on the microSD card and persists across firmware updates**
3. Exit the Settings app and the device will automatically reboot
4. After reboot you will see your new custom name in device info and on the `Passport` screen
5. Done!

**To reset the device name to default:** follow the same steps but leave the name field empty and click `Save`.

---

## What changes when you set a custom name

Changing the device name on this firmware also affects:
- Bluetooth device name
- Bluetooth MAC address (3 bytes of device ID + 3 bytes of ASCII from custom name)
- USB device name
- Serial number (ASCII from custom name)