# Plan: Diagnose Persistent SD-Insert Bootloop (Post-Momentum-Replace, Post-SD-HAL-Fix)

## Goal
Determine the exact fault that makes the device reset-loop as soon as an SD card is inserted (device boots fine without SD), by capturing the boot/crash log and bisecting the SD-mount code path.

## Current Context / Assumptions
- Already applied: Momentum app replaced with reference implementation; `lib/momentum/asset_packs.c` hardened; `furi_hal_sd.c` rewritten to propagate SPI timeouts as errors, SD "power reset" no longer grounds shared SPI pins, retry budgets restored (mount 3→10, init 3→32). Builds clean via `fbt firmware_all`.
- Symptom unchanged: boots OK with no SD; bootloops immediately when SD connected.
- SD HAL is functionally identical to upstream except the fixes above; SD shares SPI1 with NFC(ST25R3916)/PN532/SubGHz/display/buttons.
- No live debug rig required yet — first step is a serial log.

## Architecture / Proposed Approach
Do NOT change SD code further before seeing a log. One good log beats five guesses. Then, depending on log: either patch the exact fault site, or bisect by stubbing mounts to find the crashing layer.

## Step-by-Step Tasks

### Task 1 — Capture the bootlog during the bootloop (10 min, hardware)
1. Flash the current build.
2. Connect USB, open serial: `python -m serial.tools.miniterm <PORT> 115200`.
3. Type `log trace`, then insert the SD card.
4. Save the output. The repeated crash usually prints a HardFault/exception dump or at least the last `[I]`/`[E]` lines before reset.

Expected evidence patterns:
- `HardFault` / exception dump with PC/LR → note the PC; map it with `arm-none-eabi-addr2line -e build/f7-firmware-C/firmware.elf <PC>`.
- Last line `[E][Thread] Stack watermark is too low ...` or repeated `Storage ...` → stack overflow in storage task.
- Last line in `loader`/`storage`/`animation` → which service crashed.

### Task 2 — Check the on-device crash log (5 min)
Crash dumps are persisted by the desktop fault scene. Over USB CLI run:
```ree
```
and check `/int/.intpersist/` and `/ext/` for fault files; a desktop "fault" screen on boot may also appear. If the screen says e.g. `HardFault`/`assert`, record it.

### Task 3 — Enable SD SPI debug build (10 min)
In `targets/f7/furi_hal/furi_hal_sd.c` top, uncomment/define:
```c
#define FURI_HAL_SD_SPI_DEBUG
```
rebuild (`fbt firmware_all`), flash, boot with SD inserted, capture the trace again. This shows exactly which SD init step runs before the reset (CMD0/CMD8/ACMD41/CMD58/CMD9...), and whether the reset happens after a specific command or during `f_mount`/fatfs.

### Task 4 — Bisect the mount path (15 min)
Temporarily short-circuit `applications/services/storage/storages/storage_ext.c::sd_mount_card()` (or `furi_hal_sd_init`) to return `FSE_OK`/dummy without touching the bus, rebuild, insert SD:
- If the device boots: the crash is inside the SD/SPI init itself → hardware contention (shared bus, missing SD power, card contact) or a remaining `furi_check` in that path. Inspect `sd_spi_send_cmd`, `sd_spi_wait_for_data`, CRC helpers for remaining `furi_check`/`furi_assert`.
- If it still resets: the crash is NOT in SD mount — look at what runs next after mount (loader, animation_manager, desktop, `asset_packs_init`, `namespoof_init`) — those run on SD insert via `flipper_mount_callback`.

### Task 5 — Enable stack-overflow checking (5 min)
In `targets/f7/inc/FreeRTOSConfig.h` set:
```c
#define configCHECK_FOR_STACK_OVERFLOW 2
```
rebuild/flash. If the bootloop now crashes with an explicit "stack overflow" assert (or the log shows which task), that task's stack size in its `.fam`/service start is too small for the mount path (log previously showed watermark 112).

### Task 6 — Hardware elimination (10 min)
- Try a different, known-good (FAT32) SD card.
- Re-seat the card; inspect the SD slot pads/flex for damage.
- Measure 3V3 at the SD slot while inserting — `furi_hal_power_enable_external_3_3v()` may not actually be wired on this board; if the rail never rises, the card holds lines low and can disturb the shared bus. If so, that's the real bug: either wire SD power to a real rail or never let the card pull MISO low (add a series resistor / check slot FC pin).

### Task 7 — From evidence, write the fix (then implement in a new turn)
- HardFault → patch the exact line from Task 1 addr2line output.
- Stack overflow → grow the storage task stack.
- SD rail dead → hardware fix + guard in `furi_hal_power`.
- Remaining `furi_check` in fatfs/user_diskio path → convert to error returns like Task-2 SD helpers.

## Validation
- `fbt firmware_all` builds clean each step.
- Boot with no SD: normal boot.
- Boot with SD inserted (cold boot): desktop appears.
- Hot-insert SD: mount succeeds or fails with "SD bad" animation — never a reset loop for 60 s.

## Risks / Open Questions
- We still have no log: until Task 1 is done, all other fixes are guesses.
- If log shows the reset happens with zero SD-related lines, suspect PCF8574 I2C polling or an EXTI on a shared pin triggered by card insertion — check `furi_hal_i2c` and `furi_hal_interrupt` at that point.
- A marginal card/slot can look like a software bug; Task 6 must not be skipped.
