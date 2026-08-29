# Building

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---



```shell
# Clone Flipper Zero firmware
git clone --recursive https://github.com/AJ60/Oled_PCF8574_PN532.git

# Clone application
cd firmware/applications_user && git clone https://github.com/WithSecureLabs/usb-consumer-control.git

# Install toolchain
cd .. && ./fbt

# Build application
./fbt fap_usb_ccb

# Copy built application to your SD card
cp build/f7-firmware-D/.extapps/usb_ccb.fap <SD Card root>/apps/usb/ 
```