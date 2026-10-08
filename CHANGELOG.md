# Changelog

## [v2.3-OLED] — 2026-10-08 — Advanced Feature Integration (Unleashed & RogueMaster Parity, Display Burn-In Protection, Ham Radio & Sub-GHz Suite)

### Highlights & Enhancements

- **Covert Stealth Mode ("Games Only" Profile)**:
  - Added Konami code observer (`Up, Up, Down, Down, Left, Right, Left, Right`) directly on the main desktop view with PCF8574 haptic vibration feedback.
  - Toggles `momentum_settings.games_only_mode` persistently in flash storage.
  - Filters and completely masks out wireless security modules (`Sub-GHz`, `125 kHz RFID`, `NFC`, `Infrared`, `iButton`, `Bad USB`, and `GPIO`) from the application menu when active, presenting a benign handheld gaming console.
- **OLED Display Burn-In Protection**:
  - Decreased default `display_off_delay_ms` to 30s (`30000 ms`) to protect monochrome SSD1306/SH1106 displays against phosphor burn-in.
  - Linked `furi_hal_light_set(LightBacklight, ...)` to SSD1306 hardware contrast scaling register `0x81` (scaling from `0x15` to `0xCF`) and display sleep command `0xAE` on brightness `0`.
- **Dolphin Gamification & Animations (RogueMaster Parity)**:
  - Expanded `dolphin_deed_limits` daily cap from 69 to 198 across all RF, RFID, and NFC actions.
  - Enabled `.unlock_anims = true` by default to unlock and cycle through all Level 1–3 animations without gating.
- **Ham Radio Suite Overhaul**:
  - Full amateur radio receiver tool with 8 preloaded 70cm amateur channels (`433.000` to `446.000 MHz`), including `433.500 MHz` International FM Calling and `446.000 MHz` National Simplex Calling frequencies.
  - Resolved main menu `NULL` callback lockups and enabled standard Back navigation.
  - Fixed inverted color drawing bug: S-meter, sweep bars, and status text now render in solid high-contrast `ColorBlack` on the OLED screen.
  - Added full band plan persistence (`/ext/ham_radio/band_plan.txt`) saving added/removed channels across reboots.
  - Eliminated CC1101 radio re-tuning thrashing during 200ms meter polling ticks.
  - Re-spaced sweep plot boundaries (`y: 12..42`) and labels (`y: 43..51`) to avoid overlap with bottom on-screen action buttons (`y: 54..63`).
- **NFC PN532 4-Byte UID Target Emulation**:
  - Corrected target collision resolution data mapping in `furi_hal_nfc_iso14443a.c`: maps `uid[1..3]` to `target_params[2..4]` for 4-byte UIDs to account for PN532 hardware ROM's fixed `0x08` prefix, preventing duplicate-byte truncation.
- **125 kHz Discrete LF-RFID Tuning Meter**:
  - Added delta resonance frequency calculation (`Tank: +/- X.X Hz`) and visual center balance bar in `lfrfid_debug` to streamline discrete inductor/capacitor tank tuning.
- **Sub-GHz Visualizer & Auto-Recognition Suite**:
  - Verified and compiled ProtoView (pulse timing, zoom, and live decoding for TPMS, KeeLoq, Oregon v2, PT2262).
  - Verified and compiled Spectrum Analyzer, Sub Analyzer, SubHound, and core Sub-GHz Frequency Hopper.

---

## [v2.2-OLED] — 2026-08-30 — DIY Flipper Zero (OLED Edition by AJ_60)

### Highlights & Enhancements

- **NFC & PN532 Engine**:
  - Implemented PN532 hardware Crypto1 acceleration for instantaneous MIFARE Classic authentication
  - Added ISO 14443-4 APDU frame wrapping and unwrapping support for EMV banking card reads and extended transaction timeouts
  - Implemented 3-attempt retry loop on `InDataExchange` for DCS checksum errors with PN532 re-initialization on persistent failures
  - SAK card type detection refinements and reliable dictionary attack state recovery
- **Display & Power Management**:
  - Implemented SSD1306 I2C OLED display driver (replaces SPI display)
  - Dynamic OLED contrast control and dimming
  - Eliminated display reinit flicker on keypresses and sleep/wake transitions
  - Accelerated INA219 / INA226 power monitor bus probing
- **Input & Bus Robustness**:
  - PCF8574 I2C keypad with hardware interrupt (PB0) and 2-tick debounce for sub-millisecond button response
  - Rate-limited self-healing I2C bus verification (`furi_hal_pcf8574_check_and_restore`) to prevent key drops during heavy RF/I2C activity
  - I2C1 and I2C3 buses configured for 400kHz fast mode
  - Removed artificial vibro notification delays
- **LF-RFID 125kHz**:
  - Discrete analog resonant tank support (carrier: PA5, envelope RX: PA1, emulate: PA2)
  - EM4100 fob reading and tag emulation (experimental)
- **Build & Tooling**:
  - FlipperOTP GUI tool for OTP profile generation and flashing
  - diy_flasher_gui.py for automatic DFU flashing
  - GitHub Actions CI for automated builds

---

### Breaking Changes

- **Display**: I2C SSD1306 OLED replaces SPI display. OLED screen will be blank during OTA updates (updater uses SPI drivers only).
- **Input**: PCF8574 I2C keypad replaces direct GPIO buttons. Software-triggered DFU reboot is not supported; use hardware BOOT0 button.
- **NFC**: PN532 over I2C3 (PA7/PB4) replaces ST25R3916 over SPI. NFC IRQ shared with LF-RFID emulate function on PA2.
- **Pin Mapping**: Header labels differ from MCU pins. See `documentation/HARDWARE_DESIGN.md` for the complete mapping matrix.

---

### Added (Fork-specific)

- PN532 hardware Crypto1 acceleration and ISO 14443-4 APDU tunneling
- SSD1306 I2C OLED display driver with dynamic contrast
- PCF8574 I2C keypad driver with self-healing bus
- INA219/INA226 battery fuel gauge support
- LF-RFID 125kHz discrete analog subsystem
- I2C bus arbitration and rate-limited self-healing
- FlipperOTP GUI tool (`mics/FlipperOTP/generate_otp_gui.exe`)
- DIY Flasher GUI (`scripts/diy_flasher_gui.py`)
- GitHub Actions build workflow
- Comprehensive documentation with Mermaid diagrams, hardware guides, and FAQ

---

### Added (Upstream Momentum/OFW base firmware)

- **Sub-GHz**: Roger (static 28-bit), V2 Phoenix, Keeloq variants (Motorline, Rosh, Pecinin, Rossi, Merlin, Steelmate), Nero Radio, Marantec, ZKTeco, Elplast, BFT, Linear EZCode, Dickert MAHS, Prastel, Feron, Came Atomo, Holtek, and more
- **NFC**: MIFARE Ultralight C dictionary attack and key management, FeliCa Service Directory Traverse, FeliCa emulation, Amusement IC parser, MFC Banapass parser, DESFire Transaction MAC, NTAG4xx detection, MIFARE Plus EV1/2 info
- **Infrared**: Hitachi AC, Midea AC, Mitsubishi AC, Daikin FTXN25LV1B9, Toyotomi KTN22-12R32, JVC universal, LIDAR Emulator, Xbox Controller
- **JS**: New GUI views (button_menu, button_panel, menu, number_input, popup, vi_list), widget button event type exposure
- **Desktop**: Keybinds for directories, skip sliding animations, lock screen fixes, date/time input module
- **CLI**: NFC commands, buzzer command
- **BLE**: Improved pairing security
- **Apps**: SPI Terminal, Fmatrix, CO2 Logger, INA Meter, USB-MIDI, LEGO Dimensions Toy Pad, GPIO Explorer, FlipCrypt, HC-11 Modem, Programmer Calculator, Voltage Calculator, Resistance Calculator, Image Viewer, Tasks, Space Playground, FlipTDI, iButton Converter, Sub-GHz Scheduler, Flipper Share, Video Player, FlipBoard apps, FM Transmitter, Geometry Flip, Free Roam, Chief Cooker, Ghost ESP, NFC-Eink, Nearby Files, Sub Analyzer, Combo Cracker, Weebo, Flipper Flame RNG, AS7331 UV Meter, A33 Flipper Blackhat, Seos compatible, NFC APDU Runner, Passport Reader, Portal Of Flipper, and many more

---

### Fixed

- PN532 listener framing and ISO 14443-4 APDU unwrapping for EMV bank cards
- PN532 InDataExchange retry on checksum errors with dictionary attack early abort
- OLED display reinit flicker on button presses and sleep/wake transitions
- Button lag and screen blackouts from tight I2C timeout
- I2C bus wedging during RF noise (rate-limited self-healing)
- INA219 battery percentage estimation and charging logic
- Battery voltage reading below 3.2V
- SD card SPI timing and storage settings crash
- Input button interrupt handling
- System fast tick and power insomnia
- IR receiver stability and memory safety
- U2F certificate error and NFC card emulation
- Various upstream bug fixes (Sub-GHz protocols, NFC parsers, IR remotes, JS engine, GUI, CLI, BLE)

---

### Removed

- SPI display driver (replaced by I2C SSD1306)
- Direct GPIO button inputs (replaced by PCF8574 I2C keypad)
- ST25R3916 SPI NFC driver (replaced by PN532 I2C3)
