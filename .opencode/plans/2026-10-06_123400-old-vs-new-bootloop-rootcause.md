# Why Current Firmware Bootloops but the Old Version Works — Root Cause & Fix

**Date:** 2026-10-06
**Supersedes:** `2026-10-06_123005-graphics-bootloop-investigation.md` (that plan's hypothesis 1 is now confirmed with evidence).

## Goal

Explain exactly why the current project (`Flipper_OLED_PCF8574_PN532`, HEAD `8ca77a4b`) bootloops
with the SD card present / after a Graphics asset-pack change, while the old working copy at
`../New folder/Flipper_OLED_PCF8574_PN532/Flipper_OLED_PCF8574_PN532` (HEAD `d858c1ee1`) boots the
same SD card and same Momentum screens fine — and land the minimal fix.

## Verified facts (diffed OLD vs NEW)

| Question | Answer |
|---|---|
| Is OLD an ancestor of NEW? | Yes. `OLD@d858c1ee1 == merge-base`; NEW = OLD + 27 commits. No upstream divergence. |
| Watchdog (IWDG) driver in OLD? | **Never existed** (no file, no git history). |
| Watchdog in NEW committed HEAD? | **Yes**, added in `3525cbfa5` (Oct 3): `targets/f7/furi_hal/furi_hal_watchdog.c` — `LL_IWDG_Enable`, prescaler 256, reload 500 ≈ **4 s**; refreshed **only** from `vApplicationIdleHook` (`furi_hal_os.c:218-226`), tick hook calls a no-op WWDG refresh. Called from `furi_hal.c:46` before the scheduler starts. |
| Watchdog in NEW working tree? | **Files deleted + hooks disabled (uncommitted).** `git status`: `D furi_hal_watchdog.c/.h`, `M furi_hal_os.c`, `M furi_hal.c`, `FreeRTOSConfig.h` hooks set to 0. |
| Does the flashed binary contain the watchdog? | **Yes.** `build/f7-firmware-C/.../furi_hal_watchdog.o` (Oct 6 02:49) and `dist/f7-C/*mntm-main-8ca77a4b*.elf/.dfu` contain `"IWDG initialized"` / `FuriHalWatchdog` strings. The running firmware enables a 4 s IWDG. |
| `furi/flipper.c`, `animation_storage.c`, `animation_manager.c` | Byte-identical OLD vs NEW. Asset-pack/boot flow is NOT the difference. |
| `lib/momentum/asset_packs.c` | Nearly identical; adds a `.bmx` size guard, `>=` font floor, and a self-heal SD write when the pack dir is missing (NEW-only, benign). |
| `furi_hal_sd.c` | NEW inflates retries: mount retry count 3→10, init retry loop 3→32, +250 ms settle, removed ext-3.3 V power-cycle. Lengthens the idle-starved window. |
| `furi_hal_power.c` | NEW makes `is_shutdown_requested()` return `pct<=3` and real `LL_PWR_MODE_SHUTDOWN` shutdown (OLD: always false, empty loop). On a misread battery → boot→90 s→shutdown→reset→boot cycle. |
| `FreeRTOSConfig.h` | `configENABLE_MPU 0→1` plus a new MPU stack-guard region — new boot-fault vector absent in OLD. |
| `furi_hal_resources.c`/`furi_hal_gpio.c` | PC3/PC1 now GPIO-initialized each boot; PC1 adds an early-boot PCF8574 I2C transaction. |

## Why the old version works

OLD has no IWDG and no MPU. A slow boot (SD mount retries, `flipper_migrate_files()`,
`momentum_settings_load()`, `asset_packs_init()` reading the whole pack) is merely slow —
nothing times out, so the same SD card and same asset packs boot the Momentum screen fine.

## Why the current firmware bootloops

1. Flashed NEW build enables the 4 s IWDG from the very top of `furi_hal_init()`.
2. With an SD card + any persisted `asset_pack` (set by changing Graphics), boot runs a long,
   synchronous SD-heavy sequence on the boot thread (mount retries up to 32×~1 s, migration,
   settings load, pack load of hundreds of icons/fonts — and NEW may also write settings at
   `asset_packs.c:226`).
3. Storage/SPI blocking + critical sections starve the idle task (the only IWDG feeder) for
   > 4 s → hardware reset → boot repeats the same sequence → permanent loop.
4. Without the SD, `flipper.c:190` skips the whole early-init block, boot is fast, idle hook
   runs, no reset — matching the symptom.
5. A corrupt/missing pack is separately masked because NEW's self-heal clears the setting on
   the first SD-less boot and `animation_storage.c:35-47` falls back to stock assets.

Everything else (NFC PicoPass, PCF8574 hardening, settings struct field rename) does not cause
this loop. The power-shutdown logic and MPU are *additional* loop risks, not the primary one.

## The actual fix

The bootloop is not an asset-pack problem. It is the locally-added, idle-hook-fed 4 s IWDG in
a build whose cold-boot SD path can legitimately take much longer. The reference projects keep
the identical boot/asset code **without** a hardware watchdog.

Recommended resolution, in order:

1. **Stop shipping the IWDG** (preferred; matches OLD and both references):
   - Keep the current working-tree state: delete `targets/f7/furi_hal/furi_hal_watchdog.c`,
     `targets/furi_hal_include/furi_hal_watchdog.h`, remove `furi_hal_watchdog_init()` from
     `furi_hal.c`, remove both refresh calls from `furi_hal_os.c`, set `configUSE_IDLE_HOOK`/
     `configUSE_TICK_HOOK` to 0 in `FreeRTOSConfig.h`, and remove the
     `furi_hal_watchdog*` entries from `targets/f7/api_symbols.csv`.
   - `git add -A && git commit` this removal; **then** rebuild.
2. **Rebuild and reflash from the working tree** (the flashed `8ca77a4b` predates the
   watchdog removal):
   ```powershell
   cd C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532
   .\fbt.cmd firmware_all
   ```
   Expected: no `furi_hal_watchdog.o` in the build, no `"IWDG initialized"` in the DFU/ELF:
   ```powershell
   Select-String -Path dist\f7-C\*.elf -Pattern "IWDG initialized"   # expect: no match
   ```
3. **Verify on device:** (a) cold boot with SD present + asset pack selected → desktop reached,
   pack applied, no reset for 60 s; (b) cold boot without SD → normal boot; (c) SD hot-swap →
   mount or SdBad, never a reset loop.
4. **Roll back the remaining NEW-only loop risks to match OLD:**
   - `furi_hal_power.c:99-112` — restore `is_shutdown_requested()` to `return false;` until
     battery ADC calibration is trustworthy; and review whether real `LL_PWR_MODE_SHUTDOWN`
     (was empty loop) is wanted on this board.
   - `FreeRTOSConfig.h` — set `configENABLE_MPU` back to `0` (OLD value) unless the MPU guard
     region has been proven on-device.
   - Optional: revert `furi_hal_sd.c` retry inflation (32→3 init, 10→3 mount) if mounts stay
     healthy; otherwise keep, it is orthogonal once the IWDG is gone.
5. If a hardware watchdog is genuinely desired long-term, redesign it later: refresh from a
   dedicated low-priority timer/idle *plus* inside the storage init/mount retry loops, with a
   ≥16 s window, and never rely solely on the idle hook.

## AI-slop / hygiene notes

- The 27-commit window between OLD and NEW stacked several unrelated subsystems (NFC
  PicoPass, PCF8574, power, MPU, watchdog, EMV parsing) and the bootloop fix existed only as
  uncommitted working-tree edits while the flashed binary still shipped the watchdog — the
  classic "fix written but not built/committed" trap. Commit-then-build must be one step.
- ~10 plan docs in `.hermes/plans/` and `.opencode/plans/` chased the same symptom; the
  useful content is consolidated in this file. Earlier "widen IWDG to 16 s" edits never
  reached disk, which is why Task 2 of the previous plan would not have worked.
- Kept in git by accident: `.sconsign.dblite`, `compile_commands.json`, empty `resume`,
  `debug.cfg`, plus a 2.3 GB `oled_flipper.zip` in the parent folder — keep out of the repo.
