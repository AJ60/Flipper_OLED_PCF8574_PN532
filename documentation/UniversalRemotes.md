# 📺 Universal Remotes Guide {#universal_remotes}

> Guide for capturing and adding new infrared remote signals to universal remote database files.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

## 1. Televisions

To add your TV set to the universal remote database:
1. Point your remote at the DIY Flipper IR receiver (TSOP on `PA0`).
2. Go to **Infrared** → **Learn New Remote**.
3. Capture up to 6 buttons: `Power`, `Mute`, `Vol_up`, `Vol_dn`, `Ch_next`, and `Ch_prev`.
4. Test and append the signals to the end of the universal TV database file: `assets/infrared/assets/tv.ir`.

---

## 2. Audio Players & Soundbars

Up to 8 standard signals can be recorded:
* `Power`, `Play`, `Pause`, `Vol_up`, `Vol_dn`, `Next`, `Prev`, `Mute`.
* Append verified signals to `assets/infrared/assets/audio.ir`.

---

## 3. Projectors

Standard projector signals:
* `Power`, `Mute`, `Vol_up`, `Vol_dn`.
* Append verified signals to `assets/infrared/assets/projector.ir`.

---

## 4. Air Conditioners

Air conditioners transmit full multi-byte state packets (Mode, Temperature, Fan Speed) on every keypress:
* Record 6 mandatory states: `Off`, `Dh` (Dehumidify), `Cool_hi` (Lowest cool temp), `Cool_lo` (23°C cool), `Heat_hi` (Highest heat temp), `Heat_lo` (23°C heat).
* Append verified signals to `assets/infrared/assets/ac.ir`.
