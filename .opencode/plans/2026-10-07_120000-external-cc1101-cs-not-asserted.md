# External CC1101 not detected — log analysis and fix plan

**Date:** 2026-10-07
**Branch:** `revert/hal-parity` @ `7ca59818` (device build: `7ca59818-dirty`, built 06-10-2026)
**Symptom:** Radio Settings → Module → External does not stick; module reports "not detected"; Frequency
Analyzer / Raw read / Read afterwards run on the **internal** radio (selector fell back). Serial trace
captured with `log trace`.

## 1. What the log lines mean (log → code map)

| Log line | Origin | Meaning |
|---|---|---|
| `[I][SubGhzDeviceCc1101Ext] SPI bus init done, handle=080B2148` | `cc1101_ext.c` `alloc()` | full re-probe (alloc is called again after every failure) |
| `[E][CC1101] write_reg 0x00: SPI timeout (MISO stuck high)` | `lib/drivers/cc1101.c:45` via `cc1101_spi_trx()` (`:11-19`) | pre-transfer wait for MISO-low expired after `CC1101_TIMEOUT` = 250 ms (`cc1101_regs.h:17`). 0x00 = `IOCFG2`, i.e. the `cc1101_ext_rxen_high()` pre-drive present in the flashed build |
| `[E][CC1101] strobe 0x30: SPI timeout (MISO stuck high)` | `cc1101.c:29` | 0x30 = `SRES` reset strobe, one per retry in `check_init` |
| `[D] check_init: SPI released ret=0 (failed)` | `cc1101_ext.c` | probe loop exhausted (`probed == false`) |
| `[D] check_init: SPI released ret=0` + `Init failed (external CC1101 not detected)` | `cc1101_ext.c` | reset "succeeded", a later step failed |
| `[E][CC1101] write_reg 0x02: CHIP_RDYn error rx0=1 rx1=1` + `check_init: IOCFG0 HighImpedance write failed CHIP_RDYn=1` | `cc1101.c:49`, `cc1101_ext.c` | post-reset `IOCFG0` write reported not-ready → hard abort |
| `[D][SubGhzTxRx] Device 'cc1101_ext' first probe failed, retrying… / probe failed after retry` | `subghz_txrx.c:733-737` | 2 probes per `is_external_connected()` call |
| `[I][SubGhzRadioSettings] Device selector changed: index=1 text=External external_connected=0` | scene (uncommitted debug log) | returned false → **falls back to Internal** |
| `[I][CC1101] val: …` / `[T][SubghzFrequencyAnalyzerWorker] RSSI…` / `[I][FuriHalSubGhz] set_path finished: 1/2/3` | internal driver | analyzer/raw read ran on the **internal** CC1101 because the selector stayed Internal |

Two failure modes in the trace:
- **t≈40k (first attempt):** every transaction times out — MISO held high for 250 ms per transaction,
  `rxen` write + 3 reset retries = ~1 s per probe, ×2 probes.
- **t≈104k / 140k (later attempts):** MISO goes low, transfers "complete", but status bytes are garbage
  (`rx0/rx1` flapping, reset reads "ready", the next write reads "not ready"). This is not a settling
  problem — the bytes are not from the CC1101 at all.

## 2. Root cause: external SPI chip-select is never asserted

- `furi_hal_spi_bus_external_handle_event_callback()` — `targets/f7/furi_hal/furi_hal_spi_config.c:147-190`:
  drives `handle->cs` (= `gpio_ext_pa4`) **HIGH** on Init/Deactivate, and on Activate only sets it high +
  configures the SPI pins. It never writes CS low.
- `furi_hal_spi_bus_trx()` / `..._dma()` — `targets/f7/furi_hal/furi_hal_spi.c:158-232`: **do not touch CS**
  (no `cs` reference in the whole file).
- `cc1101_ext.c` never touches `spi_bus_handle->cs`.

→ The external module is **never selected**: all its "SPI traffic" happens while its CS input is high, so
MISO is whatever the shared bus shows (internal CC1101 SO/GDO1, SD card, or floating) — hence "MISO stuck
high" (nothing driving it low) or plausible-looking garbage (internal chip driving low). The chip itself
is fine; it never sees a clocked transaction.

Proof of the intended convention:
- The internal handle asserts CS at acquisition: `furi_hal_spi_bus_handle_subghz_event_callback`
  (`spi_config.c:345-360`, `furi_hal_gpio_write(handle->cs, false)` on Activate at `:350`) — that is why
  the internal radio works.
- Upstream Momentum's external callback has exactly the missing line at the end of its Activate branch:
  `furi_hal_gpio_write(handle->cs, false);` (and uses `GpioPullDown` on MISO/MOSI/SCK, not `PullNo`).
  This fork's copy dropped it. (`git log` shows the external callback was rewritten in this DIY fork,
  commits `5401ea8fa` / `f3308c6b5`; the reference `d858c1ee1` has the same omission, so the external
  CC1101 never worked in this fork.)
- Other users of the external bus work around it by driving CS themselves: nrf24
  (`nrf24.c:74-76`), SD (`furi_hal_sd.c:209-217`), unitemp (`SPISensor.c:49`).

## 3. Secondary flaws to fix in the same pass

1. **Post-reset readiness is checked too strictly.** `cc1101_write_reg()` returns the *2nd* byte's bit7 as
   `CHIP_RDYn` (`cc1101.c:49`), and `check_init` aborts the whole probe when it is non-zero right after
   `SRES`. The chip legitimately reports not-ready while the crystal restarts. The authoritative presence
   checks are the GD0 low/high self-test (PB2) and PARTNUM/VERSION (`cc1101_ext.c`, `is_connect`).
2. **GDO2/RXEN handling (hardware confirmed 2026-10-07: module RXEN is wired to the CC1101 GDO2 pin,
   no MCU pin involved).** E07-family control pins are active-high — RXEN high + TXEN low = receive,
   TXEN high + RXEN low = transmit, both low = idle — which matches the firmware's `rx()`
   (`IocfgHW | INV` = drive high, `:562`) and `tx()` (`IocfgHW` = drive low, `:579`) after the
   break-before-make switch. The remaining problem: `check_init()` (`:211`), `sleep()` (`:381`),
   `reset()` (`:521`) and `idle()` (`:544`) write `CC1101IocfgHighImpedance` (0x2E = 3-state), so RXEN
   **floats** whenever the radio is not actively receiving/transmitting. Change those four writes to
   `CC1101IocfgHW` (drive low = RXEN off) so the LNA/RF switch always has a defined level. Keep the
   pre-reset `cc1101_ext_rxen_high()` removed (flashed build): `SRES` wipes registers anyway and it was
   the first 250 ms timeout when the bus is stuck.
3. **Scene probes twice per selection** (`subghz_scene_radio_settings.c:118` for the debug log and `:121`
   for the condition) → 2× `is_external_connected`, each with 2 probes and up to 4×250 ms stalls.
4. **Uncommitted worktree state blocks any build:** `applications/drivers/subghz/cc1101_ext/cc1101_ext.c`
   (md5 `170a1b01371cf7c624992ba563885779`) contains **two definitions** of `cc1101_ext_amp_write`
   (lines 31 and 37) → redefinition error. It also re-enables the E07 amp on `gpio_ext_pc3`, which on this
   board is MCU PA5 (LF-RFID carrier, `furi_hal_resources.h:189`) — resolve deliberately, not by accident.

## 4. Fix plan (ordered)

### Step 1 — assert CS for the external handle (the actual fix)
`targets/f7/furi_hal/furi_hal_spi_config.c`, `furi_hal_spi_bus_external_handle_event_callback()`:

```c
} else if(event == FuriHalSpiBusHandleEventActivate) {
    furi_hal_gpio_write(handle->cs, true);
    furi_hal_gpio_init(handle->cs, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    LL_SPI_Init(...); LL_SPI_Enable(...);
    furi_hal_gpio_init_ex(handle->miso, ..., GpioPullNo /* upstream: GpioPullDown */, ...);
    furi_hal_gpio_init_ex(handle->mosi, ..., GpioPullNo /* upstream: GpioPullDown */, ...);
    furi_hal_gpio_init_ex(handle->sck,  ..., GpioPullNo /* upstream: GpioPullDown */, ...);
    furi_hal_gpio_write(handle->cs, false);   // <-- ADD: select the external device for the acquisition
}
```
Rationale: matches upstream Momentum and the internal subghz handle. `Deactivate` already returns CS high.
`GpioPullDown` on the SPI pins (upstream) additionally prevents floating-MISO 250 ms stalls when the
module is absent; adopt it unless a scope shows a reason not to.

### Step 2 — resolve the worktree cc1101_ext.c before building
- Delete the duplicated `cc1101_ext_amp_write` (keep the concurrent session's real one, line 31).
- Keep `cc1101_ext_rxen_high()` removed (do not re-add the pre-reset write).
- GDO2/RXEN: replace the four `CC1101IocfgHighImpedance` writes on `IOCFG2` with `CC1101IocfgHW`
  (`check_init` `:211`, `sleep()` `:381`, `reset()` `:521`, `idle()` `:544`) so RXEN is never left
  floating; leave `rx()` `:562` (high) and `tx()` `:579` (low) as they are.
- Decide the amp/TXEN pin: `gpio_ext_pc3` = PA5 = LF-RFID carrier on this board → restore the no-op/analog
  handling unless the module's TXEN is confirmed wired to header pin 7 and RFID mutual exclusion is
  guaranteed (see the 2026-10-07 wiring report).

### Step 3 — make `check_init` tolerant of post-reset not-ready
In `cc1101_ext.c` `subghz_device_cc1101_ext_check_init()`, after the reset retry loop succeeds:
```c
// wait for crystal/state machine ready before interpreting write statuses
FuriHalCortexTimer ready = furi_hal_cortex_timer_get(50 * 1000);
while(cc1101_get_status(handle).CHIP_RDYn != 0 && !furi_hal_cortex_timer_is_expired(ready)) { }
```
and replace the `write_reg(...) → if(CHIP_RDYn != 0) break;` hard aborts with: log at debug and continue;
the GD0 toggle test and final PARTNUM/VERSION read (part `0x00`, version non-0/non-0xFF) remain the
acceptance criteria.

### Step 4 — probe once per selection
`subghz_scene_radio_settings.c` `set_device()`: compute
`bool ext = subghz_txrx_radio_device_is_external_connected(...)` once, log it, reuse it in the `if`.

### Step 5 — optional log hygiene
`lib/drivers/cc1101.c`: the `CHIP_RDYn error rx0/rx1` lines are emitted on every transient status during
analyzer sweeps (the fork replaced upstream `assert`s with logs in `c98cbafcf`). Consider downgrading the
non-fatal cases to debug so future traces are readable.

## 5. Verification

- Build gate: `./fbt firmware_all` (after Step 2 — the duplicate definition must be gone).
- With module attached, serial trace on:
  - expected: `check_init: part=0x00 ver=0x14`-style `Init OK`; **no** `MISO stuck high`, no multi-second gaps.
  - `Radio Settings → Module → External` stays selected (`external_connected=1`).
  - Frequency Analyzer on External shows RSSI; Raw read on External captures samples.
- Scope/logic analyzer on PA4 (if available): CS low for the whole acquisition, high after release.
- Scope/logic analyzer on the module's RXEN (or GDO2): driven high during RX capture, low in TX/idle —
  never floating while the CC1101 is powered.
- With module removed: fast failure (thanks to `GpioPullDown`), correct "not detected".
- Regression: internal radio (analyzer/internal read), SD mount, NFC (PN532 is I2C), and one external-SPI
  app that toggles CS itself (nrf24-style) plus `spi_terminal` (now sees CS low during its session, same
  as upstream).

## 6. Risks / coordination

- Step 1 changes behavior for **all** `furi_hal_spi_bus_handle_external` users. This is upstream Momentum
  semantics; apps that drive CS themselves keep working, apps that never touched CS (cc1101_ext, the
  analyzer's external path) start working.
- `cc1101_ext.c` is being edited by another session (md5 changed twice during this analysis). Coordinate
  before touching it; Steps 1 and 4 are safe to do independently.
- Hardware unknown: whether the module is on 5V/3V3 and whether OTG (`Postponing OTG enable: VBUS(5.0)`)
  affects its supply. Not required for the CS fix, but if the probe still fails after Step 1, capture a
  trace with the module powered from a stable supply and re-check.
- Open hardware question: **where does the module's TXEN go?** RXEN ↔ GDO2 is confirmed; if TXEN is tied
  to a fixed level the PA is either always on or never on, and if it is wired to header pin 7 it lands on
  MCU PA5 (LF-RFID) on this board — which decides the amp-pin policy in Step 2. Until that is answered,
  do not enable `gpio_ext_pc3` output.

## 7. Status — 2026-10-07

- **Step 1 DONE** (`targets/f7/furi_hal/furi_hal_spi_config.c`): `furi_hal_gpio_write(handle->cs, false);`
  at the end of the external handle's `Activate` branch, and `GpioPullDown` on MISO/MOSI/SCK (upstream
  Momentum parity; keeps a tri-stated MISO from reading high so the `cc1101_spi_trx` pre-check does not
  burn 250 ms per transaction). Verified: compiles with the project's own compile command from
  `build/f7-firmware-C/compile_commands.json` (arm-none-eabi-gcc 12.3.1), rc=0.
- **Step 4 DONE** (`applications/main/subghz/scenes/subghz_scene_radio_settings.c`): the external probe
  result is cached in a local instead of being called twice, halving the probe stalls on every
  selector change (2 probes per call × 2 calls, each failed probe ≈ 1 s). Same TU compiled, rc=0.
- **Step 2 DONE** (`applications/drivers/subghz/cc1101_ext/cc1101_ext.c`): RXEN is wired to GDO2
  (confirmed by the user), so every `IOCFG2` write now drives the pin instead of tri-stating it: `CC1101IocfgHW`
  (driven low = RXEN off) at check_init :207, sleep :376, reset :515, idle :537 and TX :572;
  `CC1101IocfgHW | CC1101_IOCFG_INV` (driven high = RXEN on) in RX :555. No `CC1101IocfgHighImpedance`
  write to `IOCFG2` remains. Verified: compiles with the project's compile command for the
  `radio_device_cc1101_ext` .fal, rc=0.
- **Step 3 DONE (amp = TXEN)**: user confirmed the full E07-433M20S pin map — TXEN is on PA5, i.e.
  exactly the fork's `gpio_ext_pc3` "amp" pin, so `.amp_and_leds` stays `true` (`devices.c:40`, comment
  added) and the driver's TXEN sequencing is already correct: PA5 low in idle/sleep/reset/shutdown/rx,
  high in `tx()` (also used by `start_async_tx`) after GDO2/RXEN goes low. The analyzer worker no longer
  forces PA5 analog (it floated TXEN): new helper `subghz_frequency_analyzer_txen_off()` drives it low
  at all four of its call sites.
- **Step 5 DONE (log hygiene)**: `lib/drivers/cc1101.c` keeps `SPI timeout (MISO stuck high)` at error
  level but downgrades the transient `CHIP_RDYn` status logs (strobe/write_reg/read_reg/set_pa_table)
  to debug, so analyzer sweeps no longer flood the trace.
- **Build verified 2026-10-07 01:47** (`fbt.cmd`, in-repo toolchain): full firmware link EXIT=0 →
  `dist/f7-C/flipper-z-f7-full-mntm-revert-hal-parity-7ca59818.dfu`; `fap_subghz` →
  `build/f7-firmware-C/.extapps/subghz.fap` (141812 B); `fap_radio_device_cc1101_ext` →
  `build/f7-firmware-C/.extapps/radio_device_cc1101_ext.fal` (19816 B; a stale 380-byte stub was
  replaced). Deploy: flash the DFU, copy subghz.fap to /ext/apps/Sub-GHz/, copy the .fal to
  /ext/apps_data/subghz/plugins/, reboot.
- Not verified on hardware (RF/PTT behaviour, RSSI validity, GD0 self-test with the real module).

## 8. Follow-up bug sweep — 2026-10-07 02:12 (all fixed, all compiled + linked)

1. **Analyzer external exit floated RXEN**: teardown wrote `IOCFG2 = CC1101IocfgHighImpedance` →
   GDO2/RXEN floating after every analyzer run on the external radio. Fixed to `CC1101IocfgHW`
   (driven low) in `subghz_frequency_analyzer_worker.c`.
2. **Dangling external device / plugin heap leak on app exit**: `subghz_txrx_free()` never called
   `subghz_devices_end()` when the app exited while Internal was selected (the deliberate
   "no end on switch" note only covered runtime switches), so the malloc'd `SubGhzDevice` leaked on
   every SubGHz app run with the plugin loaded (plugins are not unloaded on exit). Fixed: `end()` for
   the ext device in `subghz_txrx_free()` before `subghz_devices_deinit()`. Also fixed a mis-scoped
   first attempt that would have hit the deinit'd registry (`furi_check`).
3. **Probe path left the module unusable**: `is_connect()` on the not-initialized path ran
   `alloc(NULL)` → `check_init()` (ends in `SPWD` shutdown) → `free()` without ever waking the chip;
   a successful probe left the module asleep with SPI deinit'd and state `Idle` ("connected"), so a
   later `rx()`/`tx()` talked to a sleeping chip. Fixed: `sleep()` after a successful probe alloc to
   park TXEN/CSN/GDO2 properly, then `free()`.
4. **TXEN/CSN floating after free() and failed alloc**: `free()` and the alloc-fail path set PA5
   (`gpio_ext_pc3` = TXEN) and PA4 (CSN) to analog/float. With the confirmed E07 pin map (TXEN = PA5)
   this left the module's PA undefined. Fixed: both paths now park TXEN output-low and CSN
   output-high; `alloc()` also parks TXEN low unconditionally before probing (covers the
   `alloc(NULL)` path where `amp_and_leds` is false).
5. **`dump_state()` NULL deref**: `subghz_device_cc1101_ext_dump_state()` acquired SPI on a NULL
   handle if called before any alloc. Fixed with a `not initialized` guard.
6. **Known accepted issue (documented, not fixed)**: PA5 is shared between E07 TXEN and the LF-RFID
   125 kHz carrier (TIM2_CH1); LF-RFID read/emulate with the module attached toggles its PA. Both
   drivers agree on output-low in idle, so the conflict is bounded to active LF-RFID use.

Verification: `cc1101_ext.c`, `subghz_txrx.c` (8 TUs incl. tests), `subghz_frequency_analyzer_worker.c`
compile rc=0; `fap_radio_device_cc1101_ext` + `fap_subghz` build + APPCHK EXIT=0; full firmware
link EXIT=0 → `dist/f7-C/flipper-z-f7-full-mntm-revert-hal-parity-7ca59818.dfu`.
- Not yet verified on hardware: needs `./fbt` + flash; first log lines to look for are
  `read_reg 0x70` / `SPI bus init done` **without** `SPI timeout (MISO stuck high)`.
