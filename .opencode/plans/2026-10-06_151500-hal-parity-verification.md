# HAL-Parity Revert Verification + Remaining Plan (SD-Bootloop)

**Date:** 2026-10-06
**Branch:** `revert/hal-parity` @ `10e2bf771`
**Reference:** `d858c1ee1` (known-good, boots same SD card)
**Supersedes:** `.opencode/plans/2026-10-06_142407-sd-bootloop-revert-to-reference.md`
(its Task 2–4 revert rounds are now DONE by the parallel session — this plan verifies that
work and defines what remains).

## Goal

Ship a firmware at HAL parity with the reference for every boot-critical file, keeping only
load-on-demand feature work, then prove on-device that the SD-present cold-boot loop is gone.

## Verification results (all checked this session, read-only)

### 1. Pin collision — CONFIRMED real, now fixed
- `4b6530dc7` set `PC1_Pin` `LL_GPIO_PIN_4` → `0` (`furi_hal_resources.h`) so a new
  `gpio->pin == 0` branch in `furi_hal_gpio.c` (`furi_hal_gpio_write_pin0/read_pin0`)
  could route header-PC1 to PCF8574 P7.
- Consequence: `gpio_ext_pc1` = `{GPIOB, 0}` became identical to `gpio_pcf_int` =
  `{GPIOB, 0}` (`furi_hal_resources.c:26,58`); init of one reconfigures PB0 and fights the
  PCF_INT EXTI0 setup (`furi_hal_pcf8574.c:328` region, `resources.c:307`).
- Consequence: every real pin-0 GPIO read (`SD_CD` = PC0, `IR_RX` = PA0, `PCF_INT` = PB0 —
  `furi_hal_resources.h`) returned hardcoded `true` instead of the IDR.
- `10e2bf771` reverted `furi_hal_gpio.c/.h`, `furi_hal_resources.c/.h` (plus `debug.c`,
  `serial.c`, `mpu.c/.h`) to reference. Verified: `git diff d858c1ee1..HEAD` on all 8
  paths is EMPTY; no `write_pin0/read_pin0` references remain anywhere; `api_symbols.csv`
  contains no pin0 symbols (only `pcf8574_*`, which still exist). No dangling references.
- `gpio_ext_pc1` is back to `{GPIOB, PIN_4}`, `debug=true` (hidden from GPIO app), and
  nothing drives it by name — the PN532-isolation motive of `4b6530dc7` has no remaining
  consumer. Phase 2-Option-B (exact reference parity) is effectively DONE.

### 2. SD-mount-callback file write — CONFIRMED deadlock path, now fixed
- `asset_packs_init()`'s missing-pack `else` called `momentum_settings_save()` (FlipperFormat
  file write) from `flipper_mount_callback()` on the storage pubsub thread — the exact path
  `furi/flipper.c:123-130` documents as deadlocking the storage service.
- `57b35519a` drops the write, keeps the in-RAM clear + fallback. Binary-verified by author.

### 3. Watchdog — stays dead
- No `furi_hal_watchdog.*`, hooks 0/0, MPU 0, `findstr IWDG` clean on last built ELF.
  (Watchdog explains the ORIGINAL Graphics-pack loop; the mount-callback write + pin
  collision explain why the loop persisted after its removal.)

### 4. Protective changes correctly KEPT (do NOT revert)
- `furi_hal_sd.c` soft-error returns (reference `furi_check`s would HardFault→reset→loop).
- `furi_hal_power.c` (no battery auto-shutdown; ISR log fix).
- `asset_packs.c` malloc/NULL guards + in-RAM clear; `desktop.c` fault-data validation +
  3 s auto-dismiss; `storage_processing.c` pop-on-failed-open (close-on-untracked is a
  no-op `INVALID_PARAMETER`, no double-pop — verified in code); `settings.*` colour refactor.

### 5. Gaps / open items found during verification
- **(a) `dist/` is STALE.** `dist/f7-C/*fix-asset-subsystem-976d0706*` built 14:45; HEAD
  (`10e2bf771`, 15:08) with the HAL revert + mount-callback fix was never built. There is
  NO flashable artifact containing the fix.
- **(b) `furi_hal_pcf8574.c` and `furi_hal_vibro.c` still differ from reference** (init
  mutex/ISR guards, P6-default-0 vs `0xFF`, active-HIGH vibro). Coherent with kept code
  (nothing calls removed symbols), but `vibro_init()` runs at boot and writes the expander
  — needs a decision: keep or revert (see open question 1).
- **(c) BOOT DIAG markers (`976d0706f`) are ancestors of HEAD** — the next build carries
  them for free; keep them until the loop is proven gone, then revert
  `furi/flipper.c` markers in a cleanup commit.
- **(d) `furi_hal_power_sleep()` `__WFI()` and real-SHUTDOWN `power_shutdown()` remain**
  (from `988dcd11f` lineage). Not yet implicated, but SHUTDOWN-mode entry on a spurious
  `power_off()` looks exactly like a reset loop — if the loop persists after this plan's
  build, `power.c` is suspect #1.

## Step-by-step tasks (for the implementer)

### Task 1 — Revert pcf8574/vibro to reference (USER DECISION: revert both)
```powershell
git checkout d858c1ee1 -- targets/f7/furi_hal/furi_hal_pcf8574.c targets/f7/furi_hal/furi_hal_vibro.c
```
Rationale per user: full HAL parity over guard retention. Accepted consequences: early-boot
PCF8574 I2C returns to unguarded form (as in the known-good build — safe because the
reference boots fine), and vibro P6 returns to default-high (`0xFF`) init, i.e. the
reference's brief boot-buzz behavior may return. If buzz or expander flakiness appears
on-device, re-apply ONLY the P6-default-0 line + init guards in a follow-up commit.
Verify afterward: `git diff d858c1ee1..HEAD -- targets/f7/furi_hal/furi_hal_pcf8574.c
targets/f7/furi_hal/furi_hal_vibro.c` must be EMPTY. Commit as
`fix(hal): restore pcf8574/vibro to reference state (full parity)`.
HOLD: user chose REVIEW ONLY — do NOT run Task 2 build until the two revert commits
(`10e2bf771`, `57b35519a`) plus the Task 1 revert have been reviewed. Build/flash/test
(Tasks 2–4) proceed only on explicit go-ahead.

### Task 2 — Rebuild + package (10–20 min)
```powershell
cd C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532
git status --short   # must show only intended files; reconcile with parallel session first
.\fbt.cmd firmware_all
.\fbt.cmd fw_dist
```
Expected: both exit 0; `dist\f7-C\` gains ONE fresh `*revert-hal-parity-*.dfu`;
`findstr /C:"IWDG" dist\f7-C\*.elf` prints nothing;
`git diff --stat d858c1ee1..HEAD -- targets/` shows only the agreed keep-list
(SubGHz/NFC/RFID/region/ina219/ibutton/pwm/light/speaker/usb/version/memory/infrared/
serial_control/pcf8574/vibro-if-kept/sd/power + api_symbols).

### Task 3 — Flash + on-device acceptance (needs hardware + SD card)
1. Flash the fresh DFU (NOT any older file in `dist\`).
2. Cold boot WITH SD ×3 → desktop reached, no red-blink loop, `BOOT DIAG: early init
   done` visible on serial.
3. Cold boot WITHOUT SD ×3 → normal boot.
4. Graphics → Asset Pack → select pack → reboot → boots WITH pack applied (the original
   symptom, now with watchdog gone AND mount-callback write gone).
5. Hot-swap SD → mount or SdBad, never a reset loop. NFC smoke test (field detect) to
   confirm dropping the PC1 virtualization broke nothing.
6. If ANY case loops: capture serial (`BOOT DIAG` last line + `sd init error` status +
   crash message) and STOP — that evidence selects the next suspect (`power.c` first).

### Task 4 — Cleanup commit (after green matrix)
Revert the `BOOT DIAG` markers: `git checkout 401966bf2 -- furi/flipper.c` (or hand-remove
markers), rebuild, re-verify Task 3 case 2 once, commit. Never ship diagnostic logs.

## Tests / validation
- Build gate per Task 2 (exit codes + artifact checks, exact commands above).
- Acceptance = Task 3 matrix, each ×3, results recorded in the final commit message.
- No code change in Tasks 1–4 alters behavior except Task 1's optional revert.

## Risks / open questions
1. **Keep or revert `pcf8574.c`/`vibro.c`?** Recommendation: keep (safer init, no consumer
   of reference behavior). Reverting risks reintroducing the boot-buzz and unguarded
   early I2C. → Needs user call (asked below).
2. If the loop persists post-fix, prime suspect is `furi_hal_power.c` SHUTDOWN path
   (item 5d) — Task 3 step 6 evidence will confirm.
3. The parallel session on `fix/boot-parity` + this branch must not both land conflicting
   `dist/` files — build and flash from ONE branch (`revert/hal-parity`).
4. Device-side hashes: confirm Settings → Info reports the new build hash, ruling out a
   stale flash (the Task-0 trap from the previous plan).
