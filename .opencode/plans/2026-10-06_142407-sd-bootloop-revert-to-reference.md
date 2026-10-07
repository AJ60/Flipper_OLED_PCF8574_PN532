# SD-Present Cold-Boot Bootloop Persists After Watchdog Removal — Revert-to-Reference Plan

**Date:** 2026-10-06
**Status:** watchdog hypothesis FALSIFIED by on-device test. New plan: evidence first, then
ordered revert bisection back to the known-good reference state.
**Supersedes:** `.opencode/plans/2026-10-06_123400-old-vs-new-bootloop-rootcause.md`
(its fix was built, flashed, and did NOT resolve the loop).

## Goal

Find and revert the change(s) introduced between the working reference state
(`d858c1ee1`, old project at
`../New folder/Flipper_OLED_PCF8574_PN532/Flipper_OLED_PCF8574_PN532`, boots the same SD card
fine) and current HEAD (`401966bf2`, 37 commits later) that cause a cold-boot reset loop with
the SD card inserted and the red LED blinking, while the device boots fine without the SD.

## Current context / assumptions (all verified this session)

- 37 commits between `d858c1ee1..HEAD` (`git log --oneline d858c1ee1..HEAD`). Full file list in
  this plan's appendix. Tree is clean (`git status` empty at HEAD `401966bf2`).
- The flashed `fix-boot-parity` build contains NO watchdog (`findstr IWDG` on the new ELF:
  no match; `furi_hal_watchdog.c/.h` deleted; idle/tick hooks 0; MPU back to 0) — yet the
  device still bootloops with SD present, red LED blinking.
- Red LED blink is driven by `applications/services/storage/storages/sd_notify.c:19,81`
  (`sd_sequence_error` → `notification_message` on SD error events). So the loop involves a
  real SD error/crash path, not just a watchdog timeout.
- Step-0 risk: two DFU files existed in `dist/f7-C/` (stale `...-mntm-main-8ca77a4b.dfu`
  WITH watchdog, new `...-mntm-fix-boot-parity-34861943.dfu` without). If the wrong file was
  flashed, the watchdog theory is untested. Rule this out before touching code.
- ONGOING CONFOUNDER: a parallel session keeps committing to this repo (`fix/boot-parity`
  branch). Before each task below, run `git status --short` and `git log --oneline -3` and
  reconcile; do not revert files the other session is actively editing without coordinating.

## What changed vs reference (boot-relevant only; apps excluded)

| # | File(s) | Change vs `d858c1ee1` | Why suspicious for SD-boot loop |
|---|---|---|---|
| 1 | `targets/f7/furi_hal/furi_hal_sd.c` | `furi_check(spi_trx)` → soft-error flag `sd_spi_io_error`; mount retries 3→10 | Error semantics of every SD transfer changed; partial-init states now propagate instead of aborting. First suspect if a failed transfer corrupts mount state. |
| 2 | `applications/services/storage/storage_processing.c` | `storage_pop_storage_file()` on failed file/dir open | Fixes a leak, but if push/pop accounting is off by one, later storage ops can use a freed slot → HardFault on SD access only. |
| 3 | `targets/f7/furi_hal/furi_hal_power.c` | Real `LL_PWR_MODE_SHUTDOWN` shutdown; `__WFI()` sleep; `get_pct` floor; OTG faked on; INA limit 1.8A | Shutdown path now truly powers off; any spurious `power_off()` = reset loop. Sleep `WFI` changes idle behavior. (`is_shutdown_requested` already forced `false`.) |
| 4 | `targets/f7/furi_hal/furi_hal_pcf8574.c`, `furi_hal_gpio.c/.h`, `furi_hal_resources.c/.h`, `furi_hal_vibro.c` | PCF8574 init/mutex guards, `gpio_ext_pc1` virtualization via P7, PC1/PC3 now GPIO-inited at boot, vibro active-HIGH P6 | Extra early-boot I2C traffic on the shared bus; wrong P6/P7 level at boot can brownout or wedge the expander. |
| 5 | `targets/f7/furi_hal/furi_hal_i2c_config.c`, `furi_hal_spi_config.c` | I2C3 PullNo→PullUp; SPI external/NFC bus pin handling rework, CS handling | Shared-bus electrical changes; SD + NFC + display share pins on this board. |
| 6 | `applications/services/gui/canvas.c`, `lib/u8g2/u8g2_glue.c/.h`, `furi_hal_light.c`, `view_port.c`, `popup.c`, `notification_app.c` | Display-presence probing + settle delays, canvas callback assert→skip, viewport mutex 2→100 ticks, LCD contrast guard | Display init now probes I2C at boot; a probe hang/fault during the SD-heavy boot window resets the device. |
| 7 | `applications/services/desktop/desktop.c`, `desktop_scene_fault.c` | Fault-data validation + 3 s auto-dismiss | Protective; KEEP. (Also means a crash screen may now vanish in 3 s — watch closely.) |
| 8 | `lib/momentum/asset_packs.c`, `settings.c/.h`, `rpc_gui.c` | malloc/NULL guards, struct refactor | Protective; KEEP. |
| 9 | `applications/services/power/power_service/power*.c` | Tick 1 s→10 s, `is_charging` from cache | Unlikely; low priority. |
| 10 | SubGHz/NFC/RFID/IR/lib changes (`furi_hal_subghz.c`, `furi_hal_nfc*.c`, `lib/drivers/*`, `lib/subghz/*`) | Large, but these services start AFTER storage/desktop | Revert only if rounds 1–3 fail. |

`furi/flipper.c`, `targets/f7/src/main.c`, `recovery.c`, `FreeRTOSConfig.h`,
`animation_storage.c` are identical to reference — NOT suspects.

## Step-by-step tasks

### Task 0 — Confirm which binary is on the device (15 min, do first)

The stale `dist/f7-C/flipper-z-f7-full-mntm-main-8ca77a4b.dfu` still contains the watchdog.
- On device: Main menu → Settings → System/Info (or `version` in CLI via USB serial) and
  record the git hash shown. Expected if correct: `34861943`/`401966bf` lineage
  (`fix-boot-parity`), NOT `8ca77a4b`.
- Host cross-check: `findstr /C:"IWDG initialized" dist\f7-C\*.dfu` — the flashed file must
  be one with NO match.
- If the device reports `8ca77a4b`: reflash
  `dist\f7-C\flipper-z-f7-full-mntm-fix-boot-parity-34861943.dfu`, retest cold boot with SD
  ×3. If the loop is gone, STOP — the watchdog was the cause and this plan is moot.
  Delete the stale `*mntm-main-8ca77a4b*` artifacts from `dist\f7-C\` so nobody flashes them
  again.

### Task 1 — Capture the fault evidence (30 min, do not skip)

Without this, every revert is a guess.
1. USB serial (qFlipper / PuTTY, 115200): cold boot WITH SD, capture full log. Wanted lines:
   last `BOOT`/`[I][Storage]`/`[E]` line before reset, any `FuriCrash`/`HardFault`/`furi_check`
   message, any `sd_sequence_error`/red-blink trigger.
2. Watch the OLED across resets: fault popup text (note: it auto-dismisses after 3 s per
   `desktop_scene_fault.c` — record video with a phone if needed).
3. Record backup-register fault data if shown, and whether the loop period is constant
   (~same seconds each cycle = same crash site) or variable (power/brownout-like).
4. Decision rule:
   - `furi_crash` message present → the message names the file:line; fix THAT, skip to Task 5.
   - No message + constant period → HardFault/reset in early init; proceed to Task 2.
   - No message + irregular period or LED-only blink → suspect power (Task 4 first).

### Task 2 — Revert Round A: SD driver + storage accounting (highest suspicion)

```powershell
cd C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532
git checkout d858c1ee1 -- targets/f7/furi_hal/furi_hal_sd.c applications/services/storage/storage_processing.c
.\fbt.cmd firmware_all; .\fbt.cmd fw_dist
```
Flash, cold boot with SD ×3.
- Rationale: restores reference `furi_check` aborts. If the device now shows a CRASH SCREEN
  naming an SD transfer, that message IS the diagnosis (keep the revert, fix the named
  transfer, then re-apply soft-errors around it).
- If it boots clean: the bug is inside the soft-error rework (likely a partial-init or
  sticky-flag path — `07d5d39fe` fixed one instance; there may be another, e.g. check that
  `sd_spi_io_clear()` runs at the top of `furi_hal_sd_read_blocks` as well as write).
  Keep reference files, close the plan, and re-harden surgically.
- If it still loops identically: `git checkout HEAD -- <same two files>` to restore, go to Task 3.

### Task 3 — Revert Round B: power HAL

```powershell
git checkout d858c1ee1 -- targets/f7/furi_hal/furi_hal_power.c targets/f7/furi_hal/furi_hal_ina219.c
.\fbt.cmd firmware_all; .\fbt.cmd fw_dist
```
Flash, cold boot with SD ×3. Restores no-op sleep + empty shutdown loop + always-false
shutdown request + 2.0 A limit. If clean, the culprit is a spurious `power_off()`/SHUTDOWN
entry (bisect inside `power.c` next: shutdown body first, then sleep, then pct floor).
If still looping, restore and go to Task 4.

### Task 4 — Revert Round C: expander/shared-bus changes

```powershell
git checkout d858c1ee1 -- targets/f7/furi_hal/furi_hal_pcf8574.c targets/f7/furi_hal/furi_hal_gpio.c targets/f7/furi_hal/furi_hal_gpio.h targets/f7/furi_hal/furi_hal_resources.c targets/f7/furi_hal/furi_hal_resources.h targets/f7/furi_hal/furi_hal_vibro.c targets/f7/furi_hal/furi_hal_i2c_config.c targets/f7/furi_hal/furi_hal_i2c.c targets/f7/furi_hal/furi_hal_spi_config.c
.\fbt.cmd firmware_all; .\fbt.cmd fw_dist
```
Flash, cold boot with SD ×3. If clean, re-apply file-by-file (`spi_config` first — shared
pins with SD; then `pcf8574`; then `gpio`) to isolate.

### Task 5 — Fix forward (once the guilty file is identified)

- Apply the MINIMAL change that fixes the crash while keeping every protective hardening
  from rows 7–8 of the table (fault-data validation, asset-pack guards).
- Rebuild BOTH `firmware_all` and `fw_dist`; verify `findstr IWDG` clean; name the dist file
  check into the commit message.
- Regression matrix, each ×3: cold boot SD-in with asset pack selected; cold boot SD-out;
  hot-swap mount/SdBad; Graphics → change pack → reboot → boots with pack applied.
- Commit per revert-round (one commit per round, message cites this plan), so `git bisect`
  stays usable later.

## Tests / validation

- Build gate: `.\fbt.cmd firmware_all` then `.\fbt.cmd fw_dist`, both exit 0; `dist\f7-C\`
  contains exactly one fresh `*fix-boot-parity*` DFU; stale `*mntm-main-8ca77a4b*` files removed.
- On-device matrix in Task 5 is the acceptance test. Record pass/fail per case in the final
  commit message. No "test it works" without the exact case list above.

## Risks, tradeoffs, open questions

- Reverting `furi_hal_sd.c` reintroduces `furi_check` aborts on transient SPI errors — that
  is INTENTIONAL in Task 2 as a diagnostic; do not ship that state if SD errors are routine
  on this board.
- Reverting `power.c` loses the ISR-logging fix and repeatedly re-enters review; re-apply
  the `furi_hal_power_alert_active` flag pattern on top of the reference file instead of
  keeping the whole new file if power.c proves guilty.
- The parallel session on `fix/boot-parity` may conflict with these checkouts — coordinate
  or pause it during Tasks 2–4; each round must be exactly one variable.
- Open: what does the fault screen / serial log actually say (Task 1 decides everything)?
- Open: was the correct DFU flashed (Task 0)? A "yes, wrong file" answer ends the
  investigation immediately.
- Open: does the OLD reference build still boot this exact SD card TODAY (regression check
  that the card itself didn't degrade)? If OLD now also loops, the SD card/filesystem is
  the variable, not the firmware — run `chkdsk` on the card and try a second card.

## Appendix — all files changed d858c1ee1..HEAD (for revert reference)

Firmware/boot-relevant: `targets/f7/furi_hal/{gpio,gpio.h,i2c,i2c_config,ibutton,ina219,
infrared,light,memory,mpu,nfc,nfc_event,nfc_felica,nfc_i.h,nfc_iso14443a,nfc_iso14443b,
nfc_iso15693,pcf8574,power,pwm,region,resources,resources.h,rfid,sd,serial,serial_control,
speaker,spi_config,subghz,subghz.h,subghz_i.h,usb_cdc,usb_hid,version_device,vibro}.c`,
`targets/f7/api_symbols.csv`, `targets/furi_hal_include/{mpu,nfc,region}.h`,
`applications/services/{desktop/desktop.c,desktop/scenes/desktop_scene_fault.c,
desktop/views/desktop_view_lock_menu.c,storage/storage_processing.c,gui/canvas.c,
gui/view_port.c,gui/modules/popup.c,notification/notification_app.c,
power/power_service/power.c,power/power_service/power_api.c,power/power_service/power_i.h,
rpc/rpc.c,rpc/rpc_app.c,rpc/rpc_gui.c,rpc/rpc_property.c}`,
`applications/settings/storage_settings/scenes/storage_settings_scene_format_confirm.c`,
`applications/main/{momentum_app (4 scenes),nfc/*,subghz/*,infrared/scenes/infrared_scene_learn.c}`,
`lib/{drivers/cc1101.c,drivers/pn532.c,drivers/pn532_reg.h,infrared/worker/infrared_worker.c,
momentum/asset_packs.c,momentum/settings.c,momentum/settings.h,nfc/*,subghz/*,
toolbox/md5_calc.c,u8g2/u8g2_glue.c,u8g2/u8g2_glue.h,update_util/update_operation.c}`,
plus app-level changes under `applications/{drivers/subghz,external/*,system/js_app}`,
`resources/*`, docs, scripts, `.opencode/plans/*`, `debug.cfg`, `resume`.
Identical to reference (excluded from suspicion): `furi/flipper.c`, `targets/f7/src/main.c`,
`targets/f7/src/recovery.c`, `targets/f7/inc/FreeRTOSConfig.h`,
`applications/services/desktop/animations/*`.
