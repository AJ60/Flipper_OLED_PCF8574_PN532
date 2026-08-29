# Changelog

## [v2.2-OLED] — DIY Flipper Zero (OLED Edition by AJ_60)

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
