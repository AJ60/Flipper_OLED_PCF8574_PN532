# WAV player

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---


 A Flipper Zero application for playing wav files. My fork adds support for correct playback speed (for files with different sample rates) and for mono files (original wav player only plays stereo). ~~You still need to convert your file to unsigned 8-bit PCM format for it to played correctly on flipper~~. Now supports 16-bit (ordinary) wav files too, both mono and stereo!

Original app by https://github.com/DrZlo13.

Also outputs audio on `PA6` - `3(A6)` pin