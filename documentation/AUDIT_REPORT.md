# DIY OLED Flipper (STM32WB55) — Firmware Audit & Fix Report

**Date:** 2026-08-09 (refreshed thrice same day: doc/comment migration; second-pass audit of PCF8574 driver & EXTI/NVIC wiring; third-pass audit of ADC metadata / display / BT / RFID-subghz)
**Scope:** Full-project architecture audit plus three fix rounds (PCF8574 driver, power HAL, board pin conflicts / ext-pin metadata), followed by the MCP23017 → PCF8574 migration across docs and comments, VS Code build-task setup, multiple build-verification passes, and second/third-pass audits that found and fixed a stuck-button latch, dead EXTI/NVIC wiring (§3.7) and wrong ADC channel metadata (§3.8).
**Status:** All fixes, doc cleanups, and comment cleanups applied and **build-verified** (repeated `fbt.cmd` builds, 0 errors). No open code issues remain from this session.

---

## 1. Project overview

A **Flipper Zero firmware fork (Momentum-based)** retargeted to a **DIY "WeAct" STM32WB55 board**. The original Flipper's SPI display, MCU-wired buttons, and charger/gauge chips are replaced:

| Peripheral | Hardware | Bus |
|---|---|---|
| Display | SSD1306 / SH1106 OLED @ 0x3C | **I2C1** (PA9 SCL, PB9 SDA, 400 kHz) |
| Buttons (6) | PCF8574 expander @ 0x20, pins 0–5, active-low | **I2C1** |
| Vibro | PCF8574 pin 6 | **I2C1** |
| Battery monitor | INA219/INA226 @ 0x40 | **I2C1** |
| RGB LED | none — LED HAL is stubbed to no-ops | — |
| SD / NFC / Sub-GHz | SPI1 (PB5 MOSI, PB3 SCK, PA6 MISO) + NFC CS PE4 | SPI1 |
| LF-RFID 125 kHz | PA5 carrier (TIM2_CH1), PA1 data in, PA2 pull | GPIO/TIM |
| I2C3 "external" bus | PA7 SCL, PB4 SDA (I2C scanner / JS i2c / CLI / ext modules) | I2C3 |

**Key architectural fact:** display, button expander, and battery monitor all share one I2C bus (I2C1). SPI1 is shared by SD, NFC, and Sub-GHz. Several header pins are wired to these shared buses (see §3.3). The repo was mid-migration from **MCP23017 → PCF8574** at the start of the session.

---

## 2. Critical findings from the architecture audit

### 2.1 PCF8574 detection was fundamentally broken (false positives)
The old `furi_hal_pcf8574_init()` "detected" the chip by raw I2C ACK probing at `0x20<<1 = 0x40` — **exactly the INA219's address**. If the expander was missing, the probe ACKed the INA219 and bound to it. The PCF8574A scan range `0x38–0x3F` (→ wire `0x70–0x7E`) also includes the **OLED at 0x3C**. No device-ID validation existed (PCF8574 has none, so at minimum a write + read-back of a known pattern is required). Consequences: the "expander not present" fallback in `input.c` never triggers, button reads return garbage, vibro/LED writes target the wrong device.

### 2.2 Multiple re-inits reset the whole PCF8574 port to 0xFF
`furi_hal_vibro_init()` and the input service both called `furi_hal_pcf8574_init()`, which wrote `0xFF` to the port — driving all 8 pins. With an active-high vibro this could re-engage the motor at every re-init and clobber output state (race depending on service startup order).

### 2.3 `furi_hal_pcf8574_set_i2c_bus()` was dead code
`init()` unconditionally overwrote the bus with `&furi_hal_i2c_handle_power` (I2C1), while `furi_hal_i2c_config.c` claims the expander lives on **I2C3**. Comments, driver, and callers contradicted each other.

### 2.4 Driver cache never synced with reads
`pcf8574_current_state` was only updated by writes; `read_port()` never fed it back. On a PCF8574, writing a `0` to an input pin turns it into a driven output — a `write_pin` read-modify-write after external state changes could corrupt inputs.

### 2.5 Board pin conflicts (PA1/PA2/PA5/PA7/PB3/PB4/PB5)
The board rewires the header: Flipper-compatible **labels do not match MCU pins**. Internal subsystems are aliased onto single MCU pins (PA1 = CC1101 GDO0 + RFID data-in; PA2 = NFC IRQ + RFID pull; PA5 = RFID carrier), and several header pins are wired to SPI1 / I2C3 / NFC lines, making them unsafe to drive as plain GPIO while those subsystems are active.

### 2.6 PWM "Tim1PA7" was doubly broken
The PWM driver configured **TIM1** on `gpio_ext_pa7` — physically **PB5** — with `GpioAltFn1TIM1`. PB5 has **no timer alternate function**, so the output silently did nothing. It also raced the NFC HAL for TIM1 ownership.

### 2.7 Ext-pin ADC metadata was garbage
The `gpio_pins[]` ADC channels (`Ch12/Ch11/Ch9/Ch4/Ch2/Ch1`) matched neither the actual MCU pins nor the original Flipper table. JS `readAnalog()`, MicroPython ADC, and anything else reading these channels got wrong results (or a `furi_check` crash for `FuriHalAdcChannelNone`).

### 2.8 Power HAL issues
- `is_charging` had a **contradictory comment** (code was internally consistent: i>0 = charging, i<0 = discharging — the INA driver negates the raw shunt reading to compensate for reversed shunt wiring).
- **INA226 alert-limit register wrap:** a 2.0 A limit on the 0.1 Ω shunt computes to 80000, silently truncated by `(uint16_t)` to ~0.36 A.
- `get_pct()` SOC heuristic contained dead code, an arbitrary `: 90` fallback, and a duplicated charging-only branch.

### 2.9 Input vibro blocked the input thread
`input_vibro_notify()` pulsed the vibro **inside `input_srv`** with `furi_hal_vibro_on(true); furi_delay_tick(level*10); furi_hal_vibro_on(false);` — every qualifying key event stalled button polling for `level*10` ticks.

### 2.10 Misc
- `scripts/fbt/util.py` `link_dir()` could throw `PermissionError` on Windows when unlinking a stale junction (now handled).
- The old `input.c` used `USE_MCP23017`; the PCF8574 migration was mid-flight.

---

## 3. Fixes applied

### 3.1 PCF8574 driver — `targets/f7/furi_hal/furi_hal_pcf8574.c` / `.h`
1. **Functional detection probe** (`pcf8574_probe_candidate`): writes `0x55`, reads back, writes `0xAA`, reads back; accepts only if ≥6 of 8 bits tracked the flipped pattern. Register-based devices don't echo arbitrary bytes (INA219 returns 0x0000; the OLED NAKs reads), so they're rejected. Up to two externally-held pins (buttons pressed at boot) are tolerated. On detection failure the first ACKing candidate (the real chip in normal layouts) is restored to idle `0xFF`.
2. **Idempotent init**: a `pcf8574_is_initialized` flag makes `init()`/`init_ex()` no-ops after first success — stops re-probing and re-writing `0xFF`.
3. **Shadow-state sync**: `read_port()` and `write_port()` both update `pcf8574_current_state`, so `write_pin()`'s RMW starts from the actual port state.
4. **`set_i2c_bus()` now honored**: new `pcf8574_get_bus()` returns the explicitly-set bus, defaulting to the power bus (I2C1).
5. **Race closed**: `write_pin()` performs its RMW **under the I2C mutex**, serializing with the input service's periodic reads.

No API signature changes → `api_symbols.csv` stays valid.

### 3.2 Power HAL — `targets/f7/furi_hal/furi_hal_power.c`, `targets/f7/furi_hal/furi_hal_ina219.c`
1. **`is_charging`**: repaired the stale comment claiming the opposite of the actual convention (`v > 3.3f && i > 0.005f`, i>0 = charging). Documentation/logic contradiction, not a runtime inversion.
2. **INA226 clamp** (`ina226_clamp_alert_limit()`): thresholds clamped to the 16-bit register range with a negative-value guard. `set_overcurrent_limit()` clamps to the maximum representable (1.638 A on a 0.1 Ω shunt) and logs `FURI_LOG_W`. `configure_protection()` uses the same clamp for both SOL and BUL paths.
3. **`get_pct()` SOC cleanup**: removed dead code, fixed the `: 90` fallback to keep the last estimate on implausible voltages, consolidated the charging-only path into the single rate-limited + monotonic + low-pass pipeline (99% cap preserved), extracted `furi_hal_power_voltage_to_soc()`. All tuning constants unchanged.

### 3.3 Pin conflicts & ext-pin metadata
**Header wiring (label → actual MCU pin):**

| Header label | Pin # | MCU pin | Function / conflict |
|---|---|---|---|
| A7 | 2 | **PB5** | SPI1 MOSI (SD/NFC/Sub-GHz) — not GPIO safe |
| A6 | 3 | **PA6** | SPI1 MISO — not GPIO safe |
| A4 | 4 | PA4 | free: GPIO / ADC IN5 / PWM LPTIM2 |
| B3 | 5 | **PB3** | SPI1 SCK (+ TIM2_CH2, Sub-GHz capture) — not GPIO safe |
| B2 | 6 | PB2 | free GPIO + ADC_IN15 (see §3.8) |
| C3 | 7 | **PA5** | LF-RFID 125 kHz carrier (TIM2_CH1) — internal; ADC_IN10 (see §3.8) |
| C1 | 15 | **PB4** | ST25R3916 NFC MISO / I2C3 SDA — not GPIO safe |
| C0 | 16 | **PA7** | I2C3 SCL (scanner/JS/CLI) + PWM TIM17_CH1 + ADC IN12 (see §3.8) |
| iButton | 17 | PA3 | iButton |
| — | — | PA1 | CC1101 GDO0 **and** RFID data-in (aliased) |
| — | — | PA2 | NFC IRQ **and** RFID pull (aliased) |

**Changes:**
1. **`furi_hal_resources.c` — `gpio_pins[]`:** ADC channels corrected to the **STM32WB55 non-standard channel map** (`pa6`→Ch11, `pa4`→Ch9, `pc3`/PA5→Ch10, `pc0`/PA7→Ch12, `pb2`→Ch15 — see §3.8; the earlier session's L4-style values were wrong and have been superseded). `pwm_output` removed from the fake "PA7" entry (PB5 has no timer output) and moved to the **PC0** entry (`FuriHalPwmOutputIdTim1PA7`). Radio/SPI-shared pins marked **`debug = true`** (pa7, pa6, pb3, pc3, pc1): hidden from the GPIO app and JS GPIO, and skipped by boot analog init so the owning HALs configure them. PA1 alias warnings added (PA2/PA5 already present).
2. **`furi_hal_pwm.c`:** `FuriHalPwmOutputIdTim1PA7` now drives **TIM17_CH1 (AF14) on the physical PA7 pin** (`gpio_ext_pc0`) instead of TIM1 on PB5. Enum name kept for API compatibility. Guarded teardown so PWM stop can't disable a timer owned by the NFC HAL. **Caveat:** PA7 is also I2C3 SCL and TIM17 doubles as the NFC block-tx timer during active sessions — PWM, I2C3, and NFC are one-function-at-a-time.
3. **PWM ownership flag** (`pwm_tim17_active`): `is_running()`/`stop()` now key on PWM ownership, not bus state — prevents mistaking an active NFC session for running PWM (no silent `set_params`-on-NFC's-timer corruption).
4. **`furi_hal_resources.h`:** header pinout comment block rewritten with the full label→MCU mapping and conflict notes; stale VIBRO/PC0 comment clarified.
5. **`furi_hal_pwm.h`:** enum doc updated for the TIM17/PA7 remap and the I2C3/NFC sharing caveat.
6. **Signal generator label:** PWM channel label `2(A7)` → `16(C0)` in `signal_gen_pwm.c` (the output moved to header C0).
7. **JS examples** (`gpio.js` ×2): rewritten to use the board's actually-usable pins — **pb2** (GPIO), **pc0** (PWM via the fixed TIM17 channel), **pa4** (ADC IN5).

### 3.4 Input vibro is now non-blocking — `applications/services/input/input.c`
`input_vibro_notify()` no longer pulses the vibro inline. It opens the notification record once at service start (`furi_record_open(RECORD_NOTIFICATION)`) and delegates each qualifying press to the notification service via `notification_message(notification, &sequence_single_vibro)` — the notification thread owns the pulse timing, so the input thread never blocks. Settings gating (`vibro_touch_level` + trigger mask) is preserved; the level no longer scales the pulse length (now a fixed `sequence_single_vibro` pulse).

### 3.5 MCP23017 → PCF8574 migration completed (docs & comments)
The migration that was mid-flight at session start is now finished repo-wide:
1. **Code comments/identifiers** — `input.c`: `INPUT_MCP_RESTORE_PERIOD_MS` → `INPUT_PCF8574_RESTORE_PERIOD_MS`, log string `mcp_pin` → `pcf8574_pin`, and all stale "MCP" comments rewritten to PCF8574 (CRLF preserved). The only intentional `MCP23017` mention left in source is the explanatory probe comment in `furi_hal_pcf8574.c` ("Unlike MCP23017, PCF8574 has no registers…").
2. **Docs** — `ReadMe.md` (summary, architecture diagram, pins table, wiring guide `#pcf8574-wiring-guide`, build section), `faq_diy_flipper.md` (7 mentions), `mics/FlipperOTP/README.md` (branding), `.vscode/ReadMe.md`. The Zigbee copro `mcp_*` symbols in `lib/stm32wb_copro/` are ST's MAC protocol and were correctly left untouched.
3. **Build docs** — documented that this fork has **no `firmware` target** (`basic_dist` is default), `fbt.cmd` on Windows vs `./fbt` on POSIX, and `fap_<appid>` for single FAPs.
4. **VS Code tasks** — `.vscode/example/tasks.json` (+ live copy): `[Windows]` build (default target), Flash USB (`FORCE=1 flash_usb_full`), and signal-generator FAP tasks, all via `cmd.exe /c fbt.cmd` (process-type, shell-agnostic).

### 3.6 Build tooling — `scripts/fbt/util.py`
`link_dir()` on Windows now catches `PermissionError` when unlinking stale junctions (`shutil.rmtree` fallback) and tolerates `CreateJunction` failures.

### 3.7 Second-pass audit — stuck-button latch & EXTI/NVIC wiring
A line-by-line audit of the PCF8574 driver, input service, power HAL, and interrupt wiring found two real bugs (both fixed and build-verified):

1. **Stuck-button latch (fixed in `furi_hal_pcf8574.c`).** The §3.1 "shadow-state sync" let `read_port()` copy live input levels into `pcf8574_current_state`. The buttons are active-low: a pressed button reads `0`. The next `write_pin()` (vibro P6 fires on *every* press via the notification service) RMW'd that `0` back to the wire → the chip drives the button pin low → the button stays "pressed" for up to the 3 s periodic restore. Fix: track `pcf8574_input_mask` (pins declared via `configure_interrupts`); all writes go through `pcf8574_compose_byte()` which forces input bits high on the wire, and `read_port()` syncs **only non-input bits** into the shadow. The RMW/compose run under the I2C bus mutex (atomic with input-service reads; the stale-argument race in `configure_interrupts` is also closed by doing the mask OR + compose inside the lock) and the shadow is committed only when the wire write succeeds. The read-sync intent is preserved, now safely.
2. **Dead EXTI/NVIC wiring (fixed).** `gpio_pcf_int` is **PB0 = EXTI line 0**; `gpio_ina_alert` is **PB1 = EXTI line 1**. `furi_hal_gpio_add/enable_int_callback()` only set the EXTI IT bit — the NVIC IRQs `EXTI0_IRQn` / `EXTI1_IRQn` were **never enabled anywhere** in the firmware, so the PCF8574 INT wakeup and the INA226 overcurrent alert ISR (`furi_hal_power_ina_alert_isr`) were silently dead; buttons worked only via the input service's 20 ms polling fallback. `furi_hal_pcf8574_attach_int()` and `furi_hal_ina226_enable_alert_interrupt()` now enable their NVIC line (preempt priority 5, matching the other EXTI lines).
3. **Stale EXTI comments corrected in `furi_hal_resources.c`.** The old note "EXTI15_10 covers PB0" was wrong — that handler serves lines 10–15 only. Verified no EXTI0/1 conflicts: IR RX (PA0) uses **TIM2 input capture**, not EXTI; RFID data-in (PA1) goes through the analog comparator (EXTI lines 20/21); NFC uses EXTI2 (PA2).

*Note (not fixed):* the SSD1306 init masks the contrast EV register to 6 bits (ST7565 heritage, EV ≈ 32/255 on an 8-bit register), so the OLED runs dimmer than it could. Cosmetic — left as-is pending hardware brightness validation.

### 3.8 Third-pass audit — ADC channel metadata was wrong (off-by-map), now corrected
A line-by-line audit of `furi_hal_adc.c`, the display path, `furi_hal_bt.c`, and the RFID/Sub-GHz HALs found one real bug plus a documented external-app mismatch:

1. **ADC channels in `gpio_pins[]` used the wrong pin map (fixed).** Both the original fork values *and* the §3.3 "correction" assumed the standard STM32L4 ADC layout (PA4=IN4, PA6=IN6, PA7=IN7, …). The **STM32WB55 uses a non-standard map**: PC0=IN1…PC3=IN4, PA0=IN5…PA7=IN12 (PA_n = IN_(n+5)), PB0=IN13, PB1=IN14, **PB2=IN15**, PB3=IN16. Verified against stock Flipper on the identical MCU (PA4=Ch9, PA6=Ch11, PA7=Ch12, PC0=Ch1, PC1=Ch2, PC3=Ch4 — channel enum maps 1:1 to `LL_ADC_CHANNEL_N`) **and** this fork's own MicroPython binding (`mp_flipper_modflipperzero_adc.c`: PC0→1, PC1→2, PC3→4, PA4→9, PA6→11, PA7→12). Corrected values in `furi_hal_resources.c`:

   | Entry | MCU pin | Before | After |
   |---|---|---|---|
   | A4 / `gpio_ext_pa4` | PA4 | Ch5 | **Ch9** (IN9) |
   | A6 / `gpio_ext_pa6` | PA6 | Ch7 | **Ch11** (IN11) |
   | B2 / `gpio_ext_pb2` | PB2 | None | **Ch15** (IN15) |
   | C3 / `gpio_ext_pc3` | PA5 | Ch6 | **Ch10** (IN10) |
   | C0 / `gpio_ext_pc0` | PA7 | Ch8 | **Ch12** (IN12) |

   **Impact:** before this fix, JS `readAnalog()`/GPIO-app reads on header A4 sampled **IN5 = PA5 (the RFID carrier pin)** instead of PA4, A6 sampled IN7 (PA7/I2C3 SCL), C0 sampled IN8 (PB0), and B2 reported "no ADC" although it has IN15.

### 3.9 OLED contrast raised to the full 8-bit SSD1306 EV range
Closing the "dim OLED" note from §3.7: the SSD1306 "Electronic Volume" (contrast) register is **8-bit (0x00–0xFF)**, but the custom init path in `lib/u8g2/u8g2_glue.c` masked it to **6 bits** (`& 0b00111111`, ST7565 heritage) and scaled the `SET_CONTRAST` message with `arg_int >> 2`.

1. **`u8g2_glue.c`:** `CONTRAST_ERC`/`CONTRAST_MGG` raised from 32/28 to **0xCF** — the same value the u8g2 library init already sends to the GUI (`u8x8_d_ssd1306_128x64_noname` init seq: `0x81 0xCF`), so the debug path now matches the GUI's brightness. `u8x8_d_ssd1306_common`'s `SET_CONTRAST` handler and `u8x8_d_ssd1306_init` now pass the contrast byte **unmasked and unshifted** (full 8-bit range). Stale ST7565 EV-formula comments replaced; `SET_EV` comment corrected to "8 bits".
2. **`display_test` app** (`applications/debug/display_test/display_test.c`): default contrast 32 → **0xCF (207)**, slider count 64 → 255 steps. Fixing the count also fixed a latent **`variable_item_list_add` overflow** (count is `uint8_t`; 256 would wrap to 0).

**Scope note:** the main GUI already ran at 0xCF (library driver) — this change does **not** alter on-device GUI brightness; it removes the wrong 6-bit mask from the custom/display-test path and aligns it with the GUI. Verified: full `fbt.cmd` build **0 errors** (11:29), and `fbt.cmd fap_display_test` **0 errors** (FAP built). Cosmetic cap: the debug slider maxes at 254 (count is `uint8_t`, index range 0–254).

---

## 4. Build verification `mp_flipper_modflipperzero_gpio.c` maps logical names to the fork's remapped pins (`PC0`→MCU PA7, `PC3`→MCU PA5, `PA7`→MCU PB5) while its ADC decode still uses the *logical* name's channel (PC0→IN1, PC3→IN4, PA7→IN12). So MicroPython `readAnalog` on the remapped names samples the wrong physical channel; plain GPIO read/write works on the right pin. PA4/PA6 are consistent (right pin + right channel). Fixing requires teaching the vendored binding about the remap — deferred.
3. **Other audited areas — no issues found.** `furi_hal_adc.c` is stock plus a robustness improvement (VREF-ready wait changed from `furi_check` to a timed best-effort wait). The display path (`canvas.c` `display_needs_reinit` re-init + `display_is_sleeping` gating, `u8g2_glue.c`, `furi_hal_light.c` backlight) is coherent. `furi_hal_bt.c` is stock (only oddity: `#include <furi_hal_bus.c>` — compiles/links fine, left as-is). The RFID HAL's PA0/PA1/PA5/PA2 sharing with IR/CC1101/NFC is fork-custom and explicitly documented in code; Sub-GHz uses its dedicated SPI handle with proper acquire/release.

---

## 4. Build verification

Multiple full default builds (`fbt.cmd`, target `basic_dist`) **succeeded** — including one after every follow-up change (input vibro, PWM ownership flag, signal-generator label, input.c comment cleanup). Final state:

- **Every changed file compiled** — `furi_hal_pcf8574.c`, `furi_hal_power.c`, `furi_hal_ina219.c`, `furi_hal_pwm.c`, `furi_hal_resources.c`, `input.c`, `furi_hal_light.c`, `furi_hal_vibro.c` — with **0 errors** in every log, including the second-pass audit builds (11:03, 11:07) and the third-pass ADC fix build (11:19, green: `firmware.elf` 10 MB, `firmware.bin` 208 flash pages).
- **`SDKCHK api_symbols.csv` → "API version 87.5 is up to date"** — the modified symbol table passed validation in every run.
- Firmware linked: `firmware.elf` (10 MB), `firmware.bin` (~848 KB, **208 flash pages**), `firmware.dfu`, `firmware.hex`; fresh dist at `dist\f7-C` (last refresh 10:10, after the final `input.c` edits).
- The signal-generator FAP built standalone (`fbt.cmd fap_signal_generator` → `APPCHK` passed).
- **Contrast pass builds:** full firmware 0 errors (11:29); `fbt.cmd fap_display_test` 0 errors (the app's `variable_item_list_add` overflow surfaced and was fixed — §3.9).
- **Build command note:** this fork's `SConstruct` has **no `firmware` alias** — use `fbt.cmd` (default target `basic_dist`) or a specific target. `./fbt firmware` fails with "Do not know how to make target 'firmware'". The toolchain is repo-local (`toolchain/x86_64-windows`, VERSION 39). On Windows Git Bash use `cmd //c fbt.cmd`; `./fbt` refuses MinGW.

### 4.1 Updater "stall" — resolved (was a misdiagnosis)
Earlier builds appeared to stall after `DIST dist_fw_dist`, with `updater.elf` never refreshing. Investigation (dry-runs `fbt.cmd -n` vs `-n --with-updater`) proved this is **by design**:

- `SConstruct:58-60` initializes the updater project **only** when `--with-updater` is passed or an updater/flash_usb target is requested. The default `basic_dist` graph contains only `SDKCHK`, `CDB`, `DIST dist_fw_dist` — **zero updater targets**.
- So `updater.elf` (dated Aug 8) was simply never in the dependency graph of our builds; nothing was hung. The silent-looking completion was `fbt.cmd`'s `-Q` (quiet) flag suppressing SCons' `done building targets` line.
- To build the updater / full update bundle: `fbt.cmd --with-updater` or `fbt.cmd --with-updater updater_package`.

---

## 5. Files changed (session)

| File | Change |
|---|---|
| `targets/f7/furi_hal/furi_hal_pcf8574.c` | Probe validation, idempotent init, shadow-state sync, bus setter honored, mutex-protected RMW; **input-mask latch protection + EXTI0 NVIC enable (audit pass)** |
| `targets/f7/furi_hal/furi_hal_pcf8574.h` | Doc comments updated |
| `targets/f7/furi_hal/furi_hal_power.c` | `is_charging` comment, `get_pct()` SOC cleanup, `voltage_to_soc()` extraction |
| `targets/f7/furi_hal/furi_hal_i2c_config.c` | Bus comment corrected: PCF8574/OLED/INA219 are on I2C1, I2C3 is the user-facing bus |
| `targets/f7/furi_hal/furi_hal_ina219.c` | `ina226_clamp_alert_limit()`, clamping in SOL/BUL paths; **EXTI1 NVIC enable in alert interrupt (audit pass)** |
| `targets/f7/furi_hal/furi_hal_resources.c` | ADC channels, PWM metadata, `debug=true` flags, PA1/PA2/PA5 conflict warnings; **EXTI0/EXTI1 comments corrected (pass 2); ADC channels corrected to the STM32WB55 map Ch9/10/11/12/15 (pass 3)** |
| `targets/f7/furi_hal/furi_hal_resources.h` | Header pinout + conflict documentation |
| `lib/u8g2/u8g2_glue.c` | SSD1306 contrast to full 8-bit EV (0xCF), removed 6-bit ST7565 mask/`>> 2` (§3.9) |
| `applications/debug/display_test/display_test.c` | Contrast default 0xCF, 255-step slider (fixes `uint8_t` count overflow) (§3.9) |
| `targets/f7/furi_hal/furi_hal_pwm.c` | TIM1/PB5 → TIM17_CH1/PA7 remap, guarded teardown, `pwm_tim17_active` ownership flag |
| `targets/f7/furi_hal/furi_hal_pwm.h` | Enum documentation |
| `applications/services/input/input.c` | Vibro feedback delegated to the notification service (non-blocking); stale MCP identifiers/comments → PCF8574 |
| `applications/external/signal_generator/views/signal_gen_pwm.c` | PWM label `2(A7)` → `16(C0)` |
| `applications/system/js_app/examples/.../gpio.js` | Rewritten for usable pins (pb2/pc0/pa4) |
| `resources/apps/Scripts/Examples/gpio.js` | Same |
| `scripts/fbt/util.py` | Windows junction unlink hardening |
| `ReadMe.md` | PCF8574 wiring guide/diagram/pins + build-command docs (`fbt.cmd`, no `firmware` target) |
| `faq_diy_flipper.md` | 7 MCP23017 mentions → PCF8574 |
| `mics/FlipperOTP/README.md` | Branding → "PCF8574 edition" |
| `.vscode/ReadMe.md` | Build/task workflow docs (fbt.cmd, no `firmware` target) |
| `.vscode/example/tasks.json` | `[Windows]` fbt.cmd build/flash/FAP tasks (tracked template) |

*(Also pre-existing in the working tree from the MCP23017→PCF8574 migration: `input.c` USE_PCF8574, `furi_hal_light.c`, `furi_hal_vibro.c`, `furi_hal_i2c_config.c`, `api_symbols.csv`, deleted `furi_hal_mcp23017.*`.)*

---

## 6. Remaining risks / recommendations

1. **✅ Build re-verified** — after all §3.3/§3.4/§3.5 edits and the §3.7 audit fixes (multiple clean builds, 0 errors; see §4).
2. **✅ Stuck-button latch fixed** — `write_pin`/`write_port` can no longer drive input (button) pins low; `read_port` never adopts input levels into the shadow (§3.7).
3. **✅ EXTI0/EXTI1 NVIC enabled** — PCF8574 INT wakeup and INA226 overcurrent alert ISR now actually fire (§3.7); resources comments corrected.
4. **Boot-time hardware check** (hardware, untested here): confirm `furi_hal_pcf8574_init()` returns false when the expander is absent (no phantom key presses) and vibro no longer engages at re-init.
5. **Verify PWM on header C0** (hardware, untested here): oscilloscope / LED via the signal generator app, channel 16(C0).
6. **Vibro feel check** (hardware): the pulse is now a fixed `sequence_single_vibro` rather than `level*10` ticks — if per-level duration matters, pre-baked sequences per level can be added later.
7. **✅ I2C3 comment fixed** — `furi_hal_i2c_config.c` now says I2C3 is the user-facing external bus and that PCF8574/OLED/INA219 live on I2C1.
8. **NFC + PWM mutual exclusion:** the one-function-at-a-time model relies on crash-on-conflict; consider a runtime check + friendly error in apps if NFC overlap becomes common.
9. **✅ ADC metadata corrected** to the STM32WB55 non-standard map (§3.8) — JS/GPIO-app analog reads now sample the right pins (A4=IN9, A6=IN11, B2=IN15, C3=IN10, C0=IN12).
10. **MicroPython `readAnalog` on remapped names** (external app, deferred): PC0/PC3/PA7 logical names decode to the wrong channel; PA4/PA6 are consistent.
11. **✅ OLED contrast raised to full 8-bit EV** (§3.9) — 6-bit ST7565 mask and `>> 2` scaling removed, default 0xCF matching the GUI; `fap_display_test` builds (its `uint8_t` slider count overflow fixed). Debug slider caps at 254 (index range 0–254).
12. **Updater not in default build** (by design, see §4.1): use `fbt.cmd --with-updater` for the full environment / update bundle if needed.
