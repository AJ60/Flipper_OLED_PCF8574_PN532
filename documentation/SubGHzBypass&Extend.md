# 📻 Sub-GHz Regional Bands & Extended Frequency Configuration

> Information on Sub-GHz frequency provisioning, regional transmission compliance, and extended frequency limits.

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

> [!CAUTION]
> ⚖️ **RADIO COMPLIANCE & LEGAL NOTICE**:
> Wireless transmissions must comply with the RF spectrum regulations of your country (e.g. FCC, CE, TELEC). Transmitting on restricted or emergency bands without a license is illegal. The maintainers assume no responsibility for regulatory violations.

---

## 📡 CC1101 Frequency Coverage Specs

* **Standard CC1101 Tuned Bands**:
  - `300 MHz – 348 MHz`
  - `387 MHz – 464 MHz`
  - `779 MHz – 928 MHz`
* **Extended Experimental Range**:
  - `281 MHz – 361 MHz`
  - `378 MHz – 481 MHz`
  - `749 MHz – 962 MHz`

> [!WARNING]
> Transmitting far outside the antenna's tuned resonant frequency can result in high VSWR (reflected RF power), which may heat or damage the CC1101 output stage.

---

## ⚙️ Configuration

* Settings can be configured under **Settings -> Protocols -> Sub-GHz -> Frequencies** or by editing the text config file `/ext/subghz/assets/setting_user` on your microSD card.