# Graphics→Asset-Pack Change Bootloops the Device — Investigation & Fix Plan

**Date:** 2026-10-06
**Project:** `Flipper_OLED_PCF8574_PN532` @ `C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532`

## Goal

Determine, with evidence, why changing Momentum Settings → Interface → Graphics → Asset Pack
(Default → any pack) bootloops this firmware until the SD card is removed — while the
reference projects (`../oled_flipper` = a0gsoft/oled_flipper, `New folder\oled_flipper`, and
upstream `next-flip/momentum-firmware`) handle the same setting, same paths, and same resources
correctly. Then fix it so a pack change boots cleanly with the SD inserted.

## Current context / assumptions

- The setting is `momentum_settings.asset_pack` (string), persisted to **internal flash** at
  `/int/.momentum_settings.txt` (`lib/momentum/settings.h:8`), saved by
  `momentum_settings_save()` (`lib/momentum/settings.c:175-209`).
- Flow on change: `applications/main/momentum_app/scenes/momentum_app_scene_interface_graphics.c:15-27`
  sets `momentum_settings.asset_pack`, `app->save_settings=true`, `app->apply_pack=true`;
  `momentum_app.c:119-144` runs `momentum_settings_save()`, `asset_packs_free()`, `asset_packs_init()`.
- Boot path: `furi/flipper.c:190-212` — if `storage_sd_status == FSE_OK`:
  `flipper_migrate_files()` (199), `namespoof_init()` (203), `momentum_settings_load()` (207),
  `asset_packs_init()` (212). On SD hot-mount: `flipper_mount_callback()` lines 150-154 rerun
  the same sequence. `furi/flipper.c` is **byte-identical** to the reference project.
- `lib/momentum/asset_packs.c` differs from the reference only by: (a) a `.bmx` size guard
  (lines 102-108), (b) `>=` vs `>` font-size floor (line 154), (c) a self-heal `else` that
  clears `asset_pack` and re-saves when the pack directory is missing (lines 222-227).
- `lib/momentum/settings.c`, `settings.h`, `animation_storage.c`, `animation_manager.c` are
  **byte-identical** to the reference (verified with `git diff --no-index`).
- Reference B (`../oled_flipper`) has **no watchdog driver at all**; there is no
  `furi_hal_watchdog.c` in upstream Momentum either.
- This project added `targets/f7/furi_hal/furi_hal_watchdog.c` (IWDG, `LL_IWDG_SetReloadCounter(IWDG, 500)`
  ≈ 4 s at 125 Hz, prescaler 256), called from `targets/f7/furi_hal/furi_hal.c:46`,
  `furi_hal_os.c:219-226` (idle hook), and several places in `furi_hal_sd.c`.
  `vApplicationTickHook` only calls a **no-op** WWDG refresh.
- Removing the SD works "because" two fallback paths cover a pack whose files are
  unavailable: `asset_packs_init` clears the missing-pack setting (asset_packs.c:218-223) and
  `animation_storage.c:35-47` falls back to `BASE_ANIMATION_DIR`. With the SD in and a pack
  selected, the slow/corrupt-load path always runs → loop. That is exactly the observed
  asymmetry.
- Prior plans in `.hermes/plans/2026-10-0{4,5,6}*` record several rounds of this same bug:
  SD watchdog fix, cold-boot SD loop, replace-momentum-app. The current tree still loops, so
  those fixes were necessary but not sufficient.

## Root-cause hypothesis (ranked)

1. **IWDG starvation (primary, ~85%).** The IWDG only refreshes in the FreeRTOS idle hook
   (`furi_hal_os.c:218-221`) plus a few SD retry loops. The cold-boot path
   `flipper_migrate_files → namespoof_init → momentum_settings_load → asset_packs_init` is a
   long, synchronous SD-heavy sequence on the boot thread; with a valid asset pack it reads
   hundreds of icons/fonts from `/ext/asset_packs/<pack>/{Icons,Fonts}` and can easily exceed
   the 4 s IWDG window (no refresh inside `asset_packs_init`, `grep -c iwdg` → 0; no refresh
   in `flipper_migrate_files`). IWDG expires → reset → same pack setting → loop. Reference
   projects never reset because they have **no IWDG** — they would hang at worst.
2. **HardFault from unchecked loads (secondary, ~15%).** `load_icon_animated`
   (asset_packs.c:38-66), `load_icon_static` (:95-127), `load_font` (:145-164), and
   `animation_storage_load_animation`/`_load_frames` (animation_storage.c:294-378) trust
   `meta` fields (`frame_count`, `width`, `height`) and `malloc` results with no NULL checks
   and `furi_check` asserts on frames (lines 368-371). A pack built for a different screen
   geometry/version faults → `furi_crash` → reset → loop. SD removal clears the setting, so
   this also fits the symptom.
3. **LL watchdog reload reverted.** `.opencode/plans/2026-10-06_030000-...md:9` and the
   12:25 plan both note the on-disk `SetReloadCounter` is still `500` (~4 s); an intended
   widening to `2000` (~16 s) never landed. Even if applied, it only raises the budget — it
   does not feed the watchdog during asset load.

## Architecture / approach

Do not copy more reference code (it has fewer guards and "works" only because it lacks the
IWDG). Make the boot-time asset-pack path (a) watchdog-safe, (b) fault-safe, and (c) measured so
the diagnosis is confirmed rather than assumed. Keep the existing self-heal for missing packs.

## Step-by-step tasks

### Task 1 — Establish the discriminating evidence (do first, ~15 min)

1a. Note exactly what the OLED shows between the last boot animation frame and each reset:
    - Full-screen "Flipper crashed and was rebooted / <message>" for ~3 s → `furi_crash`
      (hypothesis 2).
    - Only the logo/animation, then reset, no message → IWDG/hard-fault (hypothesis 1).

1b. Add stage markers and rebuild:
```powershell
cd C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532
```
In `furi/flipper.c`, immediately before each call add
`FURI_LOG_I(TAG, "BOOT stage: migrate");` / `namespoof` / `settings_load` / `asset_packs_init`,
and after the SD-ok branch: `FURI_LOG_I(TAG, "BOOT stage: early init done");`.
In `lib/momentum/asset_packs.c` `asset_packs_init()` add a counter log after each icon/font
load batch (`FURI_LOG_I(TAG, "pack load %d/%d", i, n);`).
Build: `.\fbt.cmd firmware_all`. Flash, boot with SD + selected pack, watch USB serial in
`qFlipper`/PuTTY. The last stage line localizes the reset. If the last line is mid-
`asset_packs_init` and there is **no** crash message, it is the IWDG (hypothesis 1 confirmed).

Verification: the serial log ends with a stage line and **no** crash report; reset recurs.

### Task 2 — Feed the watchdog through the boot path

In `targets/f7/furi_hal/furi_hal_watchdog.c` keep the API, but refresh from the caller instead
of relying on the idle hook.
- Widen the window: `LL_IWDG_SetReloadCounter(IWDG, 2000);` (~16 s) with a comment.
- In `furi/flipper.c` around lines 199-212 add `furi_hal_watchdog_iwdg_refresh();`
  before and after each of `flipper_migrate_files()`, `namespoof_init()`,
  `momentum_settings_load()`, and at the top of each iteration of the pack-load loops in
  `asset_packs_init()` (after the `storage_file_read` of each meta/icon file), include
  `#include <furi_hal_watchdog.h>` there too.
- In `applications/services/storage/storage_processing.c`, refresh inside the card-mount retry
  poll loop if one exists (grep `retry`/`f_mount` sites).

Build, flash, boot with SD + pack. Expected: no reset for 60 s; if the loop disappears,
hypothesis 1 confirmed and this is the fix. Commit: `fix(watchdog): feed IWDG through cold-boot SD/asset load`.

### Task 3 — Harden asset-pack loaders against corrupt/mismatched packs

`lib/momentum/asset_packs.c`:
- `load_icon_animated` (lines ~38-66): validate `meta.frame_count` against
  `file_size / max(frame_size,1)` before allocating; NULL-check every `malloc`; on any
  failure `free` what was allocated, `storage_file_close(file)`, set `*swap_out = NULL`,
  and let caller fall back to built-in icon.
- `load_icon_static` (lines ~95-127) and `load_font` (lines ~145-164): same NULL checks and
  reject `frame_size == 0`.
- `asset_packs_init` (~181-226): on package failure of a single icon, continue with the next
  instead of returning/crashing.

`applications/services/desktop/animations/animation_storage.c`:
- `animation_storage_load_frames` (~294-378): replace `furi_check(...)` at 368-371 with a
  logged error + `break`/`free` path so a bad frame deallocates and falls back to the built-in
  manifest instead of rebooting.

Build, flash, boot with SD + deliberately-corrupt pack (truncate one `.bmx`): expected clean
boot with default graphics, setting auto-cleared, error logged — no reset.

### Task 4 — Validate pack metadata against this hardware at load time

In `asset_packs_init()` (asset_packs.c:181), before loading, read
`manifest`/first icon meta and check width/height == the display geometry this build targets
(found by comparing against a working pack under `assets/packs/` in the repo; OLED builds are
typically 128x64 but confirm from `furi_hal_display`/`canvas` includes). On mismatch: log,
clear `momentum_settings.asset_pack`, save, and fall back to stock assets — never attempt the
load. This converts a whole class of "pack for the wrong screen" into a one-time soft fallback.

### Task 5 — Regression checks

```powershell
.\fbt.cmd firmware_all          # build clean
```
1. Cold boot, SD inserted, pack selected → desktop reached, pack applied, no reset for 60 s.
2. Cold boot, SD removed → normal boot; on next SD insert the setting was auto-cleared.
3. Hot-swap SD → `SdBad` or mount, never a reset loop.
4. Ideal: revert `furi_hal_watchdog.c` reload to `500` temporarily with Tasks 2-4 in place →
   device still boots (proves watchdog no longer starves; widening was only insurance).

### Task 6 — Housekeeping (do with Task 2's commit or separate)

- Delete or archive stray files from the workspace root: empty `resume`, `debug.cfg`,
  check whether `.sconsign.dblite` is tracked.
- `lib/momentum/settings.c`/`settings.h` show modified in git but diff-identical to reference —
  commit the pending changes or revert them so the tree is unambiguous.
- Note in `AGENTS.md`/`ReadMe.md` that this project must **not** add an IWDG without feeding
  it on every SD/asset-load loop (the reference projects' behavior does not transfer).

## Tests / validation

- TDD where applicable: add a host-side check in CI (or a `scripts/` python) that greps
  `asset_packs.c` for a refresh call inside the metadata-read loop before
  `animation_storage_load_*` — a failing check until Task 2 lands.
- On-device matrix in Task 5 is the acceptance test; record results in the commit message body.

## Risks, tradeoffs, open questions

- Widening IWDG to ~16 s means a genuinely dead SD mount now takes up to 16 s to reset —
  acceptable, but document it.
- If Task 1 shows a `furi_crash` message instead of a watchdog reset, Tasks 2/3 priorities
  invert: fix the specific `furi_check`/NULL deref first and skip the watchdog widening.
- Unknown: exact OLED resolution and which packs on the user's SD match it — Task 4 needs a
  working reference pack from `assets/packs/` in this repo, not from the SD.
- The `New folder\oled_flipper` reference has no NFC/PN532 HAL; treat it only as a
  Momentum-baseline comparison, not a drop-in source.
- Upstream Momentum also lacks this watchdog and shares this exact asset-pack code — so the
  bootloop is **introduced by this project's IWDG + slower cold-boot path**, not by the
  Graphics feature itself. Any "fix" that copies reference behavior without addressing the
  IWDG will regress as soon as another long SD loop appears.

## Investigation notes — bugs, gaps, AI-slop

- `furi_hal_watchdog.c` (`targets/f7/furi_hal/`) is a local-only addition upstream references
  and upstream Momentum do not have; its WWDG refresh hook is a literal no-op comment
  (`furi_hal_watchdog.c:31-33`). This is the single structural reason this project bootloops
  where references do not.
- `settings.c` clamps ints via `CLAMP(val_int, entry.i_max, entry.i_min)` — argument order
  looks swapped but matches this project's `CLAMP(x, upper, lower)` definition
  (`furi/core/core_defines.h:41`); intentional, not a bug, but worth a comment because it
  reads as one.
- No validation of `asset_pack` value against existing pack dirs at load time
  (`settings.c:130-173`); only `asset_packs_init` handles the missing-dir case, and only for
  a *missing* dir — present-but-corrupt dirs fall through to the unchecked loaders.
- `animation_storage.c:368-371` uses `furi_check` on runtime data read from the SD — a
  user-controllable input can hard-reboot the device.
- Process slop: ~10 plan documents in `.hermes/plans/` and `.opencode/plans/` about the same
  SD bootloop; several "fixes" (watchdog widening to 2000) were never actually persisted to
  disk, so the working tree kept the 4 s value. Future plan edits should be verified with
  `git diff` before rebuilding.
- Repo hygiene: 2.3 GB `oled_flipper.zip` at the parent directory, `graphify-out/`,
  `resume` (empty), `debug.cfg`, and committed `.sconsign.dblite`/`compile_commands.json`
  bloat the workspace; keep build artifacts out of git.
