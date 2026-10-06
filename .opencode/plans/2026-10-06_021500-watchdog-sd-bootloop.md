# Plan: Fix Watchdog Reset Loop During SD Mount (Cold Boot with Card Inserted)

## Goal
Eliminate the reset that happens as soon as an SD card is present at boot by preventing the independent watchdog from firing while the storage service retries the slow SD mount.

## Evidence

From the captured hot-swap log (device boots fine without SD; SD hot-swap does NOT hang — it reports `Status [2]` and shows the SdBad animation):
- `E][Loader] Status [2]: Application "/ext/..." not found` and `AnimationManager] Select 'L0_SdBad_128x51'` — the SD card never mounts.
- `E][Thread] Stack watermark is too low 112/196` — low stack watermarks in some thread.
- The crash-to-loop only happens at **cold boot with the card inserted**.

`targets/f7/furi_hal/furi_hal_watchdog.c` enables an IWDG with **~4 s timeout** refreshed **only in `vApplicationIdleHook`** (`targets/f7/furi_hal/furi_hal_os.c:218`). The IDLE hook only runs when every other task has blocked. `furi_hal_sd_init` is called from the storage service and retries up to 32 times with `SD_TIMEOUT_MS = 1000` (each SPI trx can block ~1 s, `furi_hal_sd.c:18`, `furi_hal_sd.c:976`). When an inserted card responds slowly/invalidly, the storage task spins through its retries long enough that the idle task never runs → IWDG expires → reset → cold boot again → **bootloop as long as the SD is in**.

This explains the symptom perfectly: without a card, SD detect/init fails in milliseconds, idle hook gets CPU time, IWDG is fed, the device boots. Hot-swap reaching idle between short failing mounts also avoids the reset, while a cold boot with a marginal/unreadable card drives the storage task through the long retry loop before the GUI/idle environment settles.

## Architecture / Proposed Approach
Do not rely on the idle hook as the only IWDG feeder. Feed the IWDG from the blocking SD path itself (each retry iteration and each timeout retry) and extend the IWDG window slightly so one mount cycle cannot starve it. Secondary: raise the low thread stack flagged by the watermark warning if it becomes a stack-overflow after this fix.

## Step-by-Step Tasks

### Task 1 — Feed IWDG while SD init retries (targets/f7/furi_hal/furi_hal_sd.c)
Add, near the top of `furi_hal_sd_init` retry loop and inside `sd_spi_init_spi_mode` duration loops:
In `furi_hal_sd_init` (around line ~976), change:
```c
    for(uint8_t i = 0; i < 32; i++) {
        status = sd_spi_init_spi_mode();
        if(status == FuriStatusOk) { ... }
    }
```
to:
```c
    for(uint8_t i = 0; i < 32; i++) {
        furi_hal_watchdog_iwdg_refresh();   // SD mount can block for many seconds; keep IWDG alive
        status = sd_spi_init_spi_mode();
        if(status == FuriStatusOk) { ... }
    }
```
Add at the top of the file (after includes):
```c
#include <furi_hal_watchdog.h>
```
(If the header name differs, check `targets/furi_hal_include/furi_hal_watchdog.h`.)

Also refresh inside the per-frame waits: in `sd_device_read`/`sd_device_write` loops in `furi_hal_sd_read_blocks`/`furi_hal_sd_write_blocks` (around lines 1030/1068), add the same call before each `sd_device_read`/`sd_device_write` retry.

### Task 2 — Feed IWDG inside the SD command/data timeout waits (furi_hal_sd.c)
Anywhere that polls with `furi_hal_cortex_timer_is_expired(timer)` worth > ~3 s (e.g. SD_TIMEOUT_MS·1000 waits at lines ~364, ~836, ~862) may also starve idle. Inside each such polling loop add:
```c
        furi_hal_watchdog_iwdg_refresh();
```
(as the first statement in the `while` body).

### Task 3 — Widen the IWDG window (furi_hal_watchdog.c)
```c
    LL_IWDG_SetReloadCounter(IWDG, 2000);  // ~16s
```
(2000 at 125 Hz ≈ 16 s). A storage/FatFS hiccup longer than 16 s with no SD-refresh means a real deadlock — crashing is then acceptable.

### Task 4 — Confirm the failing thread's stack warning
The log shows `E][Thread] Stack watermark is too low 112` and `196`. Identify which task via a `log debug` or by checking the new crash name in `vApplicationStackOverflowHook` output; then increase that task's `stack_size` in its `.fam`/service start. Do this only if Task 1–3 still crash.

### Task 5 — Build & flash
```bash
cd "C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532"
.\fbt.cmd firmware_all
```
Expected: clean build, new `build/f7-firmware-C/firmware.{bin,dfu}`.

### Task 6 — Validate
1. Cold boot with SD disconnected → normal boot (regression check, same as before).
2. Cold boot with SD inserted → desktop reached (this is the bug case).
3. Hot-swap SD while running → mounts, or shows SdBad — never a reset loop for 60 s.

## Risks / Tradeoffs
- Refreshing the IWDG from the SD path means a genuinely stuck mount no longer resets the device; the retry bound (32) caps the hang at roughly 32 s worst case, which is acceptable.
- If the bootloop still happens with SD present at cold boot after this fix, the remaining suspect is the cold-start hardware path itself (SD rail not powered, shared SPI bus state left by bootloader) — capture the bootlog immediately after cold boot in that case.
```

## Tests / Validation
- `fbt firmware_all` clean build.
- No SD at cold boot: boots.
- SD at cold boot: boots, no loop.
- SD hot-insert: SdBad or success, no loop.
