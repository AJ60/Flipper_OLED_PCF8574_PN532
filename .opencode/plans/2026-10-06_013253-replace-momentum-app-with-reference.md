# Plan: Replace Current Momentum App with Reference `oled_flipper` Momentum App (SD Bootloop)

## Goal
Replace the local firmware's Momentum app (`applications/main/momentum_app`, `lib/momentum`, SD-boot interaction) with the reference `oled_flipper` implementation to eliminate the bootloop that occurs as soon as an SD card is inserted, while preserving all PCF8574/PN532 board-specific code.

## Current Context / Assumptions

| Item | Detail |
|------|--------|
| Local project | `C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532` — STM32F7 Momentum firmware, PCF8574 button expander, PN532 NFC (`lib/drivers/pn532.c`, `applications/services/input/input.c` uses `furi_hal_pcf8574.h`) |
| Reference project | `C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\oled_flipper` — same Momentum base, no PN532 driver, STM32WB55/MCP23017 variant |
| Symptom | Device boots correctly with NO SD card. The moment an SD card is inserted the device enters a bootloop (crash → watchdog reset → reboot, repeating while SD present). Without SD everything works. |
| Root-cause candidates (ranked) | 1) `lib/momentum/asset_packs.c` in local is a divergent, partially-patched copy: on animated-icon frame-load failure it runs `for(; i >= 0; i--) free(swap->frames[i]);` freeing uninitialized/garbage pointers → heap corruption → HardFault → watchdog → bootloop. Local also misorders/misses `storage_file_close()` on error paths. 2) `applications/services/storage/storage_processing.c` local added `storage_pop_storage_file()` on failed open — changes FS error path vs reference. 3) Asset pack on the SD card may be incomplete (missing frames), exactly exercising path (1). |
| Boot path (verified) | `furi/flipper.c::flipper_init()` → if SD not ready skips early init; else `flipper_migrate_files()`, `namespoof_init()`, `momentum_settings_load()`, `asset_packs_init()`. On SD insert: `storage.c` fires `StorageEventTypeCardMount` → `flipper_mount_callback()` (same sequence, plus `asset_packs_free()` first). Local `furi/flipper.c` is byte-identical to reference, so the SD-insert crash is upstream of it: in `asset_packs_init()`/storage handling. |
| Asset pack location | `lib/momentum/asset_packs.h`: `#define ASSET_PACKS_PATH EXT_PATH("asset_packs")` → packs load from the SD card at `/ext/asset_packs/<PackName>/{Icons,Fonts}`. `momentum_settings.asset_pack` selects the pack. |
| Identical dirs (verified by hash diff) | `assets/packs/` (Momentum, WatchDogs), `resources/asset_packs/`, `lib/momentum/settings.c/.h`, `lib/momentum/namespoof.*`, `lib/momentum/asset_packs_i.h`, `furi/` (incl. `flipper.c`), `targets/furi_hal_include/furi_hal_sd.h` (same `furi_hal_sd_max_mount_retry_count` API). |
| Files that actually differ (Momentum-related) | `lib/momentum/asset_packs.c` (heavily), `applications/main/momentum_app/scenes/momentum_app_scene_misc.c`, `..._misc_screen.c`, `..._protocols.c` (formatting only), `applications/services/storage/storage_processing.c` (local has extra `storage_pop_storage_file` on `!ret`). |
| Local asset_packs.c vs reference | Local: added `file_size < sizeof(header)` validation in `load_icon_static`, `size >= U8G2_FONT_DATA_STRUCT_SIZE` in `load_font` (fixes off-by-one where reference uses `>`), added `else { clear asset_pack setting + momentum_settings_save() }` when pack dir missing, but lost reference's `else { storage_file_close(file); i--; }` guard in `load_icon_animated` and reference's unconditional trailing `storage_file_close(file)` in the loaders. |
| Build constraint | No `arm-none-eabi-gcc` on PATH (toolchain dir exists under `toolchain/`); expect to build via the bundled toolchain / user environment: `fbt.cmd f7-firmware-C` (see `dist/`, `build/f7-firmware-C`). |
| Prior work | `.hermes/plans/2026-10-06_100000-replace-momentum-app-sd-bootloop.md` and `.hermes/analysis/` (diff_matrix.csv, asset_init_*.txt, sd_mount_*.txt) contain earlier comparison; this plan supersedes it. |

## Architecture / Proposed Approach

Do NOT bulk-copy the reference tree (it would drop PN532/PCF8574 HAL). Surgically replace only the Momentum app sources and its two support files with the reference versions, then re-apply the small, validated local hardening fixes (`file_size` header check, `>=` font-size check, "pack not found → clear setting") on top of the reference `asset_packs.c`, and keep the local `storage_processing.c` fix unless proven guilty. After each change rebuild with fbt and re-test on-device: boot without SD (must boot normally), then insert SD (must NOT bootloop).

## Step-by-Step Tasks

### Task 1 — Snapshot current state (read-only)
```powershell
cd "C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532"
git status --short | Select-Object -First 30
git stash list
```
Expected: modified `lib/momentum/asset_packs.c`, `storage_processing.c`, 3 scene files. Save `git diff > .hermes\analysis\pre-replace.diff`.

### Task 2 — Back up the files about to be replaced
```powershell
New-Item -ItemType Directory -Force ".hermes\backup\momentum"
Copy-Item "lib\momentum\asset_packs.c" ".hermes\backup\momentum\"
Copy-Item "applications\services\storage\storage_processing.c" ".hermes\backup\momentum\"
Copy-Item -Recurse "applications\main\momentum_app" ".hermes\backup\momentum\"
```
Expected: three items under `.hermes\backup\momentum`.

### Task 3 — Copy reference Momentum app sources over local
```powershell
$ref = "C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\oled_flipper"
$dst = "C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532"
Copy-Item -Recurse -Force "$ref\applications\main\momentum_app\*" "$dst\applications\main\momentum_app\"
Copy-Item -Force "$ref\lib\momentum\asset_packs.c" "$dst\lib\momentum\"
Copy-Item -Force "$ref\lib\momentum\settings.c" "$dst\lib\momentum\"
Copy-Item -Force "$ref\lib\momentum\settings.h" "$dst\lib\momentum\"
Copy-Item -Force "$ref\lib\momentum\namespoof.c" "$dst\lib\momentum\"
```
Verify:
```powershell
git diff --no-index -- "$dst\lib\momentum\asset_packs.c" "$ref\lib\momentum\asset_packs.c"
```
Expected: no output (files identical to reference).

### Task 4 — Re-apply local hardening to the reference asset_packs.c
Edit `lib\momentum\asset_packs.c`, in `load_icon_static`:
```c
        uint64_t file_size = storage_file_size(file);
        if(file_size < sizeof(header)) {
            FURI_LOG_E(TAG, "Invalid .bmx size %llu < header %zu", file_size, sizeof(header));
            storage_file_close(file);
            return;
        }
        uint64_t frame_size = file_size - sizeof(header);
```
In `load_font` change:
```c
        if(size >= U8G2_FONT_DATA_STRUCT_SIZE && storage_file_read(file, swap, size) == size) {
```
In `asset_packs_init` re-add after `storage_file_free(f);`:
```c
    } else {
        // Pack directory not found - clear setting to prevent bootloop on restart
        FURI_LOG_E(TAG, "Asset pack '%s' not found, clearing setting", pack);
        momentum_settings.asset_pack[0] = '\0';
        momentum_settings_save();
    }
```
Keep reference's `else { storage_file_close(file); i--; }` in `load_icon_animated` and reference's trailing `storage_file_close(file)` in all loaders — these fix the local bugs identified above.

### Task 5 — Decide storage_processing.c (keep local for now)
Do NOT overwrite with reference yet. Local added:
```c
            if(!ret) {
                storage_pop_storage_file(file, storage);
            }
```
in `storage_process_file_open` and `storage_process_dir_open`. If the bootloop persists after Task 4, next test is reverting this file to reference's version.

### Task 6 — Scene files
Task 3 already overwrote the 3 formatting-only diffs with reference bytes; no further action.

### Task 7 — Build
```powershell
cd "C:\Users\alojy\OneDrive\Desktop\Flipper-OLED-Indian\oled_flipper\Flipper_OLED_PCF8574_PN532"
.\fbt.cmd f7-firmware-C
```
Expected: build completes; no new compile errors in `lib\momentum` or `momentum_app`. If the toolchain is missing from PATH, use the bundled one under `toolchain\` (see prior build dir `build\f7-firmware-C`) or have the user run the build.

### Task 8 — Flash and smoke-test (no SD)
Flash via the normal updater; power on. Expected: normal boot to Momentum main menu; no resets.

### Task 9 — SD-insert test (the actual bug)
With the device running, insert the SD card that previously triggered the bootloop. Expected: device shows mount/reload activity and keeps running; `asset_packs_init()` reruns without HardFault. If it bootloops, capture the crash trace over USB UART/CLI and check: (a) pack on SD incomplete → reload path in reference `load_icon_animated`, (b) revert `storage_processing.c` per Task 5, (c) re-apply `furi/flipper.c` from reference (currently identical, so deprioritize).

### Task 10 — Commit (only after on-device success)
```powershell
git add lib/momentum applications/main/momentum_app applications/services/storage
git commit -m "Replace momentum app with reference; fix asset pack fd leaks and frame cleanup"
```

## Tests / Validation
- Static: `git diff --no-index lib/momentum` local vs reference shows only the Task-4 hardening hunks.
- Build: `fbt.cmd f7-firmware-C` succeeds.
- Runtime: boot OK without SD; SD insert no longer bootloops; asset pack switching in Momentum app (Interface → Graphics → Pack) still works; fonts/icons render.

## Risks, Tradeoffs, and Open Questions
- Reference `load_font` uses `size > U8G2_FONT_DATA_STRUCT_SIZE` (off-by-one, rejects a font exactly equal to the struct); we intentionally keep local's `>=`.
- Reference `asset_packs_init` lacks the "pack missing → clear setting" block; we intentionally keep it to avoid a stale `asset_pack` setting persisting across SD swaps.
- `storage_processing.c` local hunk may be necessary or a red herring; Task 5 keeps it and Task 9 provides the decision point.
- If the bootloop persists with the reference asset-pack loader, the next most likely culprit is the PN532 driver being poked during storage init or an incomplete asset pack on the user's SD card — then diff `lib/drivers/pn532.c` usage and validate the pack (`/ext/asset_packs/<pack>/Icons/*/meta` frame_count matches actual frame files).
- Reference project is MCP23017/WB55-flavored; only the Momentum sources are portable. Do not copy `targets/`, `furi/`, or `lib/drivers` from reference.
