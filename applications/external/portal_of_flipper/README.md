# Portal of Flipper

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---



USB Emulator

Original App by bettse

App Icon by mikeonut

Additional protocol reverse engineering and testing by norto

## Requirements

NFC files must:

- be Mifare Classic 1k
- have complete data
- have the correct sector 0 Key A

## How to use

- Be sure you don't have qFlipper or lab.flipper.net connected to the flipper when you launch (this will cause the USB emulation to fail to start).
- Use 'Load figure' to select a .nfc file to load
- Figure, if loaded successfully, will appear in list
- Press center when figure highlighted to remove

## TODO:

- Hardware add-on with RGB LEDs to emulate portal and 'jail' lights: https://github.com/flyandi/flipper_zero_rgb_led/blob/master/led_ll.c
