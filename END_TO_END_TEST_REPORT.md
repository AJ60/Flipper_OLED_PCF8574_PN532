# Flipper_OLED_PCF8574_PN532 — End-to-End Test & Project Report

**Target:** STM32WB55 (TARGET_HW=7) · Momentum-based Flipper Zero firmware fork
**Date:** 2026-11-02 · **Build:** `4df842de5` (dist suffix `mntm-main-4df842de`)
**Toolchain:** ARM `arm-none-eabi` + vendored Python 3.11.9 (`toolchain/x86_64-windows/`) · Host gcc: MSYS2 ucrt64 16.2.0

---

## 1. Test Results — Summary

| Test | Method | Result |
|------|--------|--------|
| Full firmware build (compile + link + pack) | `python -m SCons TARGET_HW=7` | ✅ **PASS** — `BUILD_EXIT=0` |
| Host unit tests — `test_jammer_freq` | `test_host.sh` (host gcc) | ✅ **20 checks, 0 failures** |
| Host unit tests — `test_jammer_signal` | `test_host.sh` (real `jammer_signal.c`) | ✅ **ALL PASSED** (11 checks) |
| Host unit tests — `test_pulse` | `test_host.sh` (host gcc) | ✅ **8 checks, 0 failures** |
| Host unit tests — `test_pyield` | `test_host.sh` (host gcc) | ✅ **PASS** (exit 0) |
| C lint (clang-format + folder naming) | `python -m SCons TARGET_HW=7 lint` | ⚠️ **70 style warnings, 0 in custom code** |

**Bottom line:** The firmware builds cleanly end-to-end and produces all flashable artifacts. Every host unit-test module passes. The only lint findings are code-style (clang-format) deviations in **upstream** Furi/Momentum files — the project's own code (subghz_jammer + custom subsystems) is clang-format clean.

---

## 2. Build Verification

Command (per AGENTS.md, vendored toolchain):
```bash
export PATH="$PWD/toolchain/x86_64-windows/python:$PWD/toolchain/x86_64-windows/bin:$PATH"
export FBT_NO_SYNC=1
python -m SCons TARGET_HW=7
```

Outputs produced (`build/f7-firmware-C/` and `dist/f7-C/`):
- `firmware.elf` — 851,628 bytes (text 836,956 / data 984 / bss 13,688)
- `firmware.bin` — 837,948 bytes (205 flash pages, last page 57.71% full)
- `firmware.dfu`, `firmware.hex`, `firmware.json`, `compile_commands.json`
- Dist package: `flipper-z-f7-full-mntm-main-4df842de.{bin,dfu,json}` (8.38 MB) + `.elf`

Memory map: **836 KB text / 984 B data / 13.7 KB BSS** — well within the 1 MB Flash / 256 KB RAM1 budget.

---

## 3. Subsystems (custom + integrated)

| Subsystem | Driver / Chip | Status |
|-----------|--------------|--------|
| OLED display | SH1106 (default) / SSD1306 via I2C | ✅ SH1106 set as default (`8972ee0e1`) |
| Keypad | PCF8574 I2C expander (`furi_hal_pcf8574.*`) | ✅ |
| NFC | PN532 only (legacy ST25R3916 HAL stripped, `2809c7e8c`) | ✅ |
| Sub-GHz | CC1101 (subghz app + subghz_jammer external) | ✅ |
| LF-RFID | 125 kHz (lfrfid) | ✅ |
| Power monitor | INA219 / INA226 (I2C) | ✅ |
| microSD | SPI storage (backed off absent-SD polling, `abb1ec82d`) | ✅ |
| BLE / copro | STM32WB5x BLE light stack (copro 1.20.0) | ✅ |

Custom HAL sources live under `targets/f7/furi_hal/` (e.g. `furi_hal_pcf8574.*`), with debug apps `applications/debug/display_test/` and `applications/debug/keypad_test/` for on-device verification.

---

## 4. App Structure

- **Main apps** (`applications/main/`): archive, bad_usb, gpio, ibutton, infrared, lfrfid, momentum_app, nfc, onewire, subghz, u2f.
- **External apps** (`applications/external/`): ~180 apps (games, tools, radio, NFC). Includes the custom **`subghz_jammer`**.
- **subghz_jammer** (the active development app):
  - Modules: `jammer_freq.{c,h}` (freq/hop math), `jammer_signal.{c,h}` (LevelDuration encoder — carrier/noise/sweep/pingpong/pulse), `jammer_radio.{c,h}`, `jammer_settings.{c,h}`, `scenes/` (menu/start/stop/handlers/config), `subghz_jammer.{c,h}`, `application.fam`, images.
  - Tests: `tests/test_{ammer_freq,jammer_signal,pyield,pulse}.c` + `test_host.sh` runner + `build/stubs/` (host Furi stubs).
  - Host harness (`test_host.sh`) is well-engineered: pins MSYS2 gcc, redirects TEMP/TMPDIR to a writable `build/test-tmp.*` dir (avoids the `C:\WINDOWS` and MSYS `/tmp` pitfalls), and adds `-Ibuild/stubs -I../../..` for Furi stubs + repo-root `level_duration.h`.

---

## 5. Build Config (`fbt_options.py`)

- `FIRMWARE_ORIGIN = "Momentum"`, `TARGET_HW = 7`, `COMPACT = 1` (size-optimized), `DEBUG = 0`.
- Dist naming: tag → `mntm-<tag>`; else `mntm-<branch>-<commit8>`.
- App sets: `default` (full), `unit_tests` (adds on-device `unit_tests` app), `unit_tests_min`.
- Copro: BLE light stack, `stm32wb_copro` v1.20.0.

---

## 6. Host Unit Tests — Detail

Run via the project's own `applications/external/subghz_jammer/test_host.sh` (authoritative harness). All four modules pass:

- **test_jammer_freq** — 20/20 checks. Frequency rounding, hop-band math (must land exactly on band_min/band_max), region TX validation.
- **test_jammer_signal** — 11/11 checks. Runs the **real `jammer_signal.c`** (Furi deps stubbed): pulse duty cycle 0–100% correct, carrier 50% duty, hop band edges (433.0 ↔ 435.0 MHz), span math.
- **test_pulse** — 8/8 checks. Pulse-train duty-cycle scheduling.
- **test_pyield** — passes. Yield-level scheduling math.

Note: the pure-math tests (`freq`, `pulse`, `pyield`) need only `-I.`; the `signal` test additionally requires the repo-root `lib/toolbox/level_duration.h` and host stubs for `furi.h` / `furi_hal_random.h` — exactly what `test_host.sh` supplies.

---

## 7. Lint Results

Command: `python -m SCons TARGET_HW=7 lint` → `Error 1` (style gate).

- **70 clang-format violations** — all in **upstream** files: `furi_hal_i2c.c`, `furi_hal_spi_config.c`, `furi_hal_ina219.c`, `furi_hal_light.c`, `furi_hal_power.c`, `furi_hal_region.c`, `lib/momentum/asset_packs.c`, `lib/subghz/subghz_worker.c`, `applications/main/subghz/helpers/subghz_frequency_analyzer_worker.c`, `applications/services/desktop/views/desktop_view_main.c`, `applications/services/loader/loader_menu.c`, `furi/flipper.c`.
- **0 violations** in the project's custom code (subghz_jammer + custom subsystems) — clang-format clean.
- **18 "incorrectly named folder" warnings** — all `graphify-out` folders (third-party graphify app shipped in-tree); not auto-renamed.

These are style/formatting and naming nitpicks, **not** build or correctness failures.

---

## 8. Git History (recent, last 12)

```
4df842de5 feat(subghz_jammer): add pulse-train duty-cycle math (Stage 1)
ec6604de9 feat(subghz_jammer): add host unit-test harness (Stage 0)
560737902 fix(subghz_jammer): fix async tx crash, guard radio stop, correct 868MHz preset, validate region tx
8972ee0e1 feat(display): set default OLED display driver to SH1106
8a1b17650 revert: restore HAL/core/subsystems to known-good origin/main (dd7f5c612)
290f5c78a feat(build): space-delimited quoted targets + COMPRESS flag alias
b5d88683e fix(external): handle realloc failures, purge AI attribution comments
abb1ec82d fix(storage,core): back off absent SD polling, null checks, zero RFID buffers
f1e559b2b fix(hal,power): remove dead PMIC includes, fix storage leak, clean PCF8574 LED stubs
2809c7e8c fix(hal,nfc): strip legacy ST25R3916 HAL, enforce PN532-only path
085c6ca43 fix(hal,core): memory-safety guards, lock inversion, build deps
a7e97082f fix(build): updater_package no longer wipes FAP dist (--noclean)
```

Uncommitted working tree: 11 files modified in `subghz_jammer/` (freq/radio/settings/signal + scenes + app), plus `test_host.sh`.

---

## 9. Known Issues & Recommendations

| # | Item | Severity | Recommendation |
|---|------|----------|----------------|
| 1 | 70 clang-format violations in upstream files | Low | `python -m SCons TARGET_HW=7 format` (or leave — upstream, style-only) |
| 2 | 18 `graphify-out` folder-name warnings | Low | Rename/remove third-party graphify app if not needed |
| 3 | Uncommitted subghz_jammer changes (11 files) | Info | Commit before the next stage; dist hash tracks HEAD, so uncommitted work isn't reflected in the firmware filename |
| 4 | On-device unit tests (`unit_tests` app) not executed | Info | Per `documentation/UnitTests.md`: build `updater_package`, ship resources to board, run over serial — requires hardware |

### Environment notes (for reproducibility)
- **Windows + MSYS2:** bare `python`/`gcc`/`scons` are often the wrong build. Always prepend the vendored Python + ARM toolchain to `PATH`; use `/c/msys64/ucrt64/bin` for host gcc.
- **Temp dirs:** MSYS `/tmp` and `C:\WINDOWS` are unreliable for gcc intermediates — the project's `test_host.sh` correctly uses a writable `build/test-tmp.*` dir.
- **Builds are long (>10 min).** Use background execution; check `firmware.elf` mtime vs source mtimes to confirm a rebuild actually happened (the dist hash comes from git, not ELF content).

---

## 10. Conclusion

The project is in a **healthy, buildable state**. The complete firmware compiles and links for STM32WB55, all four host unit-test modules pass (including the real signal-encoder integration test), and the custom code is lint-clean. Remaining findings are cosmetic (upstream formatting, third-party folder names) or require hardware (on-device unit tests). The active `subghz_jammer` app is well-structured with a working host-test harness that proves feature math before device flash.
