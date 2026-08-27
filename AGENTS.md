# AGENTS.md — DIY Flipper OLED firmware (Momentum fork)

## Repo layout

- Real source root is `oled_flipper/oled_flipper/`. The outer `oled_flipper/` is just a wrapper holding `build/`, `.vscode/`, and the nested repo.
- Project README is `ReadMe.md` (uppercase `R`, lowercase `m`), not `README.md`.
- This is a **Momentum-firmware fork** (`applications/main/momentum_app/`), customised for SH1106/SSD1306 OLED on I2C + PCF8574 I/O expander on WeAct STM32WB55. Upstream OFW behaviour may not apply.

## Build commands

- Toolchain launcher: `fbt` (POSIX sh, `fbt:1`) and `fbt.cmd` (Windows, `fbt.cmd:1`). Both bootstrap `scripts/toolchain/fbtenv`.
- `./fbt` **refuses to run under MinGW / Git Bash** on Windows ("In MinGW shell, use fbt.cmd instead"). From Git Bash use `cmd //c fbt.cmd`; from PowerShell/cmd use `.\fbt.cmd`.
- **Default target is `basic_dist`** (`SConstruct:146`). There is **no `fbt firmware` user target** in this fork — do not invent it.
- Build everything firmware: `fbt.cmd` (no args) or `fbt.cmd basic_dist`.
- Build a single external app: `fbt.cmd fap_<appid>` (e.g. `fap_signal_generator`). `<appid>` comes from the app's `application.fam` (`firmware.scons:207`).
- Build/deploy all FAPs: `fap_dist`, `fap_deploy` (`SConstruct:171,175`).
- Updater `.tgz` bundle: `fbt.cmd updater_package --with-updater` (`--with-updater` is defined at `site_scons/commandline.scons:4-8`).
- Other useful aliases (all in `SConstruct`): `flash`, `jflash`, `flash_usb`, `flash_usb_full`, `debug`, `blackmagic`, `openocd`, `cli`, `copro_dist`, `vscode_dist`, `get_apiversion`, `fw-version`, `doxygen`.
- Asset targets (`assets/ReadMe.md`): `icons proto dolphin_internal dolphin_blocking dolphin_ext resources`.
- Skip git-submodule sync on every invocation: `set FBT_NO_SYNC=1` (Windows) / `FBT_NO_SYNC=1` (POSIX) — `fbt:15`, `fbt.cmd:6`.
- Discover all targets: `fbt.cmd --help` (runs the `fbt_help` tool, `SConstruct:21-26`).

## Lint & format

- CI runs `./fbt lint_all` (`.github/workflows/lint.yml:26`). Local: `fbt.cmd lint_all`.
- `lint_all` = `lint + lint_py + lint_img`; `format_all` = `format + format_py + format_img` (`SConstruct:418-419`).
- C lint/format scope: `applications/` minus `applications/system/js_app/packages/` and `applications/external/`, plus `furi/` (`firmware.scons:30-36`, `furi/SConscript:4`). **Libs under `lib/*` are not in C lint scope** unless each library appends itself.
- C formatter: clang-format (tab=4). Python: `black` (`SConstruct:371-380`).

## Code style (essentials — see `CODING_STYLE.md` for full)

- Tabs = 4 spaces.
- Types: PascalCase (`FuriHalUsb`, `Gui`, `SubGhzKeystore`). Functions: snake_case (`furi_hal_usb_init`).
- Filename = type/function prefix (`subghz_keystore.{h,c}`).
- Directory names: `^[0-9A-Za-z_]+$`. File names: `^[0-9A-Za-z_]+\.[a-z]+$`. Extensions: `.h .c .cpp .cxx .hpp` only.
- Ctor/dtor suffixes: `_alloc` (allocate + init), `_free` (deinit + release).

## Apps

- `applications/` — stock firmware apps. `disabled_apps/` — apps held back from the build (excluded by `SConstruct:333-337`). Both excluded from C lint intentionally to avoid upstream merge conflicts.
- `applications_user/` — drop custom apps here (`applications_user/README.md`). The dir is fully git-ignored (`applications_user/.gitignore: *` — only `*`), so contents live only on the developer's checkout. fbt still picks it up as an app source (`SConstruct:335`).
- Each app declares itself in an `application.fam` file.

## API surface

- `targets/f7/api_symbols.csv` is the public C API surface. CI (`build.yml:54-77`) checks its first two lines match upstream `flipperzero-firmware:release` exactly. **Any change to the public API must update this file or CI fails.**

## Hardware gotchas (OLED fork)

- **Do not run `fbt firmware`** — it does not exist. Use the default target.
- **First-time DFU install** on a blank WeAct board must go via qFlipper "REPAIR" or the README's Method A. A blank board has no Flipper bootloader; it shows as a generic STM32 DFU.
- **STM32CubeProgrammer: never "Full Chip Erase"** — only "Sector Erase". Full erase wipes the OTP/partition table and bricks the boot (blank screen, no USB).
- **I2C1 (SCL=PA9, SDA=PB9)** needs external **2.2 kΩ–4.7 kΩ pull-ups**. Without them the firmware boot-loops on I2C bus recovery and the device appears frozen.
- **Vibration motor** must be driven through an N-MOSFET (e.g. 2N7002/AO3400) with a flyback diode. PCF8574 I/O pins are only rated for 25 mA — direct drive will reset/damage the pin.
- **NFC controller: PN532 V3 over I2C** (not ST25R3916 SPI). Driver: `lib/drivers/pn532/`. HAL select: `FURI_HAL_NFC_CHIP_PN532` defined in `targets/f7/furi_hal/furi_hal_resources.h:253`. I2C3 bus (`furi_hal_i2c_handle_external`, PC0=SCL/PC1=SDA), address `0x24`, 400 kHz. IRQ pin: `gpio_nfc_irq_rfid_pull` (PA2, active-low). API symbols in `targets/f7/api_symbols.csv` (16 PN532 functions).
- **PN532 I2C pull-ups**: module typically includes 10 kΩ to 3.3V. If not, add 4.7 kΩ to 3.3V on SDA/SCL.
- **Battery monitor (INA219/INA226)** is calibrated for a **0.1 Ω** shunt (`R100`).
- **Software DFU reboot is unsupported** — the keyboard is on PCF8574, not a GPIO the bootloader can read. Use BOOT0 hardware method.
- **Built-in updater** runs outside the main OS and only supports the original SPI screen. During a `.tgz` flash the OLED will go black for 1–2 minutes — expected, do not unplug.
- **Default CC1101 PPM calibration is +100** in the Momentum App to compensate for cheap-crystal drift (target 433.920 MHz).
- **Speaker on PB8**: only **passive piezo buzzers** directly. Dynamic speakers (8–32 Ω) need a transistor driver.
- **1-Wire (iButton) on PA3** needs an external 2.2–4.7 kΩ pull-up to 3.3V.
- **OTP** is one-time programmable — there is no undo. Windows GUI tool: `mics/FlipperOTP/`.
- Full troubleshooting: `faq_diy_flipper.md`.

## NFC architecture (PN532)

- **Build-time chip selection**: `FURI_HAL_NFC_CHIP_PN532` in `furi_hal_resources.h:253` selects PN532 over I2C. Without it, ST25R3916 over SPI is built. All HAL NFC code uses `#if defined(FURI_HAL_NFC_CHIP_PN532)` to switch between the two.
- **Driver**: `lib/drivers/pn532/pn532.c` — I2C frame protocol, card detection (`InListPassiveTarget`), data exchange (`InDataExchange`), MIFARE auth/read/write, card emulation (`TgInitAsTarget`).
- **HAL layer**: `targets/f7/furi_hal/furi_hal_nfc.c` (common), `furi_hal_nfc_iso14443a/b.c`, `furi_hal_nfc_iso15693.c`, `furi_hal_nfc_felica.c` (per-technology). Each has PN532-specific init/deinit/tx/rx paths.
- **State**: `FuriHalPn532Ctx` in `furi_hal_nfc_i.h` holds the current target and RX buffer.
- **IRQ**: `furi_hal_nfc_irq.c` uses GPIO ISR on `gpio_nfc_irq_rfid_pull` (PA2) for PN532 (active-low, level-held). SPI IRQ register reads for ST25R3916.
- **Known gaps**: ISO14443B/ISO15693/FeliCa poller paths exist but TX/RX functions may need hardware validation. ST25TB not implemented for PN532. ISO15693/FeliCa listener modes use GPIO signal generation that may conflict with PN532.

## Logging

- On-device: **Settings > System > Log Level** → `Debug` or `Trace`.
- Serial CLI: `log debug` (stop with `Ctrl+C`).
- View in qFlipper Log window (`Ctrl+L`) or any serial terminal on the virtual COM port at **115200 baud** (per `faq_diy_flipper.md` Q4).

## More info

- `ReadMe.md` — full build, flash, and wiring guide + schematic.
- `faq_diy_flipper.md` — hardware troubleshooting Q&A.
- `CODING_STYLE.md` — full C/Python style guide.
- `assets/ReadMe.md` — asset-naming rules and asset build targets.
- `mics/FlipperOTP/README.md` — OTP flashing tool.
