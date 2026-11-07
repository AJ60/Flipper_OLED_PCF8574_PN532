# Sub-GHz Jammer Extended — Final Implementer Plan

**Status:** Synthesized from 3 research reports + 3 design reports (each critiqued), then verified against source. One-shot to implement.

**Scope:** Extend `applications/external/subghz_jammer/` with 11 advanced jam modes, frequency hopping, power/thermal management, and safety interlocks — all under the async-TX restart constraint.

**Key reconciliation decisions (resolved from the 3 design reports):**
1. Power → ONE representation: `power_level uint8_t (0..100, 0=auto)`, mapped per-radio to PA_TABLE level. (Task 1 had dBm, Task 2 had pa_gain_dbm, Task 3 had 0..100 — reconciled to 0..100, radio-aware.)
2. Hop trigger → simplified to **worker-owned direct `commit_hop`** (no dual tx_callback flag).
3. `dwell_time_ms` → reuse existing `dwell_ms`; scope the new field to `dwell_hold_ms` (gap between hops).
4. INA219/INA226 thermal → **optional/estimated** (jammer app has no INA219 integration).
5. 11 advanced modes → **new high-level `JammerJamMode` enum** (maps to Carrier/Noise encoder modes); `JammerSignalMode` stays at 5 low-level values.

---

## 0. Verified Ground Truth (from source)

**Async-TX constraint (the crux):**
- `jammer_radio_tx_callback` (jammer_radio.c:25) is the `FuriHalSubGhzAsyncTxCallback` — runs in **DMA-completion ISR context**. It calls `jammer_signal_yield(signal)`.
- `jammer_signal_yield` (jammer_signal.c:43) **NEVER** returns `level_duration_is_reset()` while active → DMA loop runs forever → continuous TX.
- Therefore a TX restart (stop→begin→start) MUST happen in **thread/timer context, never from the callback** (doing it in the ISR re-enters DMA teardown/re-init → hard fault).

**Radio API (jammer_radio.c):**
- `jammer_radio_begin(device, freq)` (124): idle→reset→load_preset(Ook650Async)→set_frequency. ONCE.
- `jammer_radio_start(device, signal)` (138): suppress_charge_enter→start_async_tx(device, jammer_radio_tx_callback, signal). Region-gated.
- `jammer_radio_stop(device)` (153): stop_async_tx→idle.
- `jammer_radio_set_frequency(device, freq)` (163): only rewrites CC1101 FREQ register + set_tx (NO restart). **Insufficient for clean cross-path hops** — a hop needs a full stop→begin→start.

**JammerSignal (jammer_signal.c/h):**
- `jammer_signal_yield` (43): modes Carrier/Noise/Pulse (Sweep/PingPong handled by worker). Never returns reset.
- `jammer_signal_hop_frequency(band_min, band_max, hop_index, hop_count)` (114): EXISTING hop math. Guards: hop_count<2→band_min, band_max≤band_min→band_min.
- `jammer_signal_reset` (99): resets level.

**JammerFreq (jammer_freq.c):**
- `jammer_freq_sweep_point` (5), `jammer_freq_pingpong` (17), `jammer_freq_pulse_on_us` (30). Pure integer, 64-bit intermediates, guards.

**JammerSettings (jammer_settings.c):**
- 9 fields: Mode, PulseRatio, Frequency, BandMin, BandMax, HopCount, DwellMs, Radio, Latch. `applied < 9` guard (116). FlipperFormat pattern: rewind per key, temp_ vars, insert_or_update on save.

**Worker (subghz_jammer.c):**
- `jammer_worker_next_frequency` (21): Noise/Sweep/PingPong scheduling.
- `jammer_worker_thread` (47): begin→start→loop→stop. Loop (91-112) calls `jammer_radio_set_frequency` (98) for sweep modes — **this in-place register write must be replaced with a full restart for clean hops.**

**Bands:** 300-348, 387-464, 779-928, 902-928, 915-928, 917-928 MHz.
**TX gate:** subghz_devices_check_tx / furi_hal_subghz_check_tx → SubGhzTx enum (Allowed/Unsupported/BlockedDefault/BlockedExtended/BlockedRegion).
**PA tables:** internal (PA_TABLE levels 1..31 → -17..-1.5 dBm); external (PA_TABLE0=0xE7 family → +20..+30 dBm, OTG enabled).

---

## 1. Data Model

### 1.1 JammerJamMode (NEW high-level enum — 11 modes)
The worker's scheduling mode. Maps to a low-level JammerSignalMode (Carrier/Noise) for the encoder.

```c
typedef enum {
    JammerJamModeDwellCycle,   // hold fixed list of targets, dwell then gap
    JammerJamModeNoiseFHSS,    // noise bursts on FHSS hop targets
    JammerJamModeNoiseBrute,   // dense noise on every discrete target
    JammerJamModeNoiseScan,    // noise bursts stepped forward
    JammerJamModeNoisePingPong,// noise forward then reverse
    JammerJamModeNoiseCarrier, // pure noise on single freq
    JammerJamModeSweepFHSS,    // carrier tone on FHSS hop targets
    JammerJamModeSweepBrute,   // carrier swept densely over targets
    JammerJamModeSweepDwell,   // carrier swept with dwell at each point
    JammerJamModeSweepScan,    // carrier stepped forward
    JammerJamModeSweepPingPong,// carrier forward then reverse
    JammerJamModeCount,        // sentinel
} JammerJamMode;
```
Map to encoder mode: DwellCycle/Sweep* → Carrier; Noise* → Noise.

### 1.2 JammerSignalMode (UNCHANGED — 5 low-level modes)
Carrier, Noise, Sweep, PingPong, Pulse, Count. The encoder's modulation mode. Sweep/PingPong are handled by the worker, not the encoder.

### 1.3 JammerSettings (EXTENDED — add 8 fields)
Reuse existing fields; add:
- `uint32_t hop_interval_ms` (default 200) — dwell per hop.
- `uint32_t dwell_hold_ms` (default 100) — gap between hops (DwellCycle).
- `uint32_t sweep_step_hz` (default 250000) — sweep step size.
- `uint32_t noise_step_hz` (default 500000) — noise step size.
- `uint32_t power_level` (default 0, 0=auto) — TX power 0..100, mapped per-radio.
- `uint8_t bypass_region` (default 0) — bitmask (bit0=skip EU).
- `uint32_t max_tx_duration_ms` (default 300000) — watchdog.
- `uint8_t tx_gate_mode` (default 0) — 0=continuous, 1=gate-on-input, 2=pulse-gate.

Reuse `dwell_ms` (existing) for dwell time. Reuse `pulse_ratio` for duty cycle.

### 1.4 Freq functions (NEW pure functions — jammer_freq.c)
Mirror existing sweep/pingpong/pulse style (64-bit intermediates, guards):
- `jammer_freq_step_target(base, step_hz, n)`: clamp(base + n*step_hz, base, max). Guard step_hz==0→base, max≤min→base.
- `jammer_freq_dwell_target(dwell_min, dwell_max, dwell_step_hz, dwell_index)`: over [dwell_min,dwell_max]; wraps index.
- `jammer_freq_noise_hop_target(noise_min, noise_max, noise_step_hz, hop_index)`: over [noise_min,noise_max]; guards noise_max≤noise_min.
- `jammer_freq_hop_count_for_span(min, max, step_hz)`: ceil(span/step)+1. Guard step_hz==0→1, max≤min→1.
- `jammer_freq_hop_queue_next(cursor, targets, count)`: FIFO ring iterator. Returns 0 sentinel when count==0.

All in jammer_freq.c (pure, no Furi deps) for host-testability.

### 1.5 Hop queue (NEW ring buffer — jammer_signal.h)
```c
#define JAMMER_HOP_QUEUE_LEN 16
typedef struct {
    uint32_t freq[JAMMER_HOP_QUEUE_LEN];
    uint16_t head, tail;
    uint8_t count;
    uint32_t current_hz;
    bool dirty;
} JammerHopQueue;
```
Add to JammerSignal. Worker enqueues targets; radio drains them (restart at each).

---

## 2. Implementation Architecture

### 2.1 Hop mechanism (worker-owned, direct commit_hop)
**Simplify the dual-trigger to worker-owned direct commit:**
- The worker loop computes the next hop target (jammer_freq_* math) every hop_interval_ms.
- The worker calls `jammer_radio_commit_hop(device, signal, freq)` which does stop→begin→start at the new freq.
- The tx_callback stays pure (just yields modulation). No s_hop_pending flag needed.

Correct because the restart tears down the DMA, so mid-packet restart is fine. The pattern (calling stop_async_tx from the worker thread) is already used at subghz_jammer.c:114.

### 2.2 Restart sequence (jammer_radio_commit_hop)
1. if(s_tx_running){ subghz_devices_stop_async_tx(device); s_tx_running=false; }
2. jammer_radio_begin(device, freq): idle→reset→load_preset(FuriHalSubGhzPresetOok650Async)→set_frequency.
3. pre-TX gate: if(subghz_devices_check_tx(device,freq)!=Allowed || furi_hal_subghz_check_tx(freq)!=Allowed) return false.
4. jammer_radio_set_tx_power(device, power_level): load PA_TABLE per radio.
5. jammer_radio_start(device, signal): suppress_charge_enter→start_async_tx.
6. s_tx_running=true; s_current_freq=freq; hop_count++.

### 2.3 Power model
- `power_level uint8_t (0..100, 0=auto)`.
- Internal PA: level = 1 + power_level*30/100 (1..31 → -17..-1.5 dBm).
- External PA: PA_TABLE0 byte; keep OTG enabled.
- Applied per hop (so each hop can carry its own power).

### 2.4 Thermal model (estimate — no INA219)
- junction_temp_est = T_amb + R_th(C/W) * P_avg(W); P_avg = I_full(freq,power)[mA] * duty/100 * 3.3V.
- I_full: ~11 mA/dBm @433MHz, ~16 mA/dBm @915MHz (external PA).
- Decision table (evaluated in worker between hops):
  1. if junction_temp_est > T_WARN(60C) OR avg_current > I_THERMAL: reduce duty (reuse pulse_ratio).
  2. if still > T_WARN: step power_level down.
  3. if avg_current > I_MAX OR Vbat < V_MIN: reduce power_level.
  4. if Vbat < V_CRIT: force stop.
  5. if elapsed >= max_tx_duration_ms: force stop (watchdog).
- Note: estimate only (no INA219 in jammer app).

### 2.5 Safety interlocks
- `jammer_pre_tx_check(device, freq, power_level)` → SubGhzTx: freq-valid + region + power gate. Called before start and before each hop.
- For sweep-brute (hop_count>1): validate EVERY hop target; if any is Blocked, clamp band or reject.
- Dialogs: Unsupported (Widget error), BlockedDefault (confirm extend range), BlockedRegion (confirm bypass).
- Watchdog: FuriTimer armed at start, re-armed each hop → jammer_radio_stop on expiry.

---

## 3. UI/Scene

### 3.1 Menu structure (hierarchical — restructure from flat)
The existing menu (subghz_jammer_scene_menu.c) is a FLAT VariableItemList. Restructure to a hierarchical menu with a "Configure" submenu (7 submenus) — cleaner for 20+ settings.

Submenus:
1. Mode (11 jam modes).
2. Band (6 bands).
3. Frequency (12 presets).
4. Radio Type (Auto/Internal/External/External+PA).
5. Hop Count (2,4,8,16,32,64).
6. Dwell Time (10-5000ms).
7. Power (0-100%).

Plus top-level: Trigger Mode, Advanced (sub-submenu: sweep_step, noise_step, hop_interval, dwell_hold, bypass_region, max_tx_duration, tx_gate_mode), Start.

### 3.2 Scenes
- scene_menu (configure).
- scene_start (live status: hops, freq, power, temp).
- scene_stop (stop button).

---

## 4. File Targets

| File | Change |
|------|--------|
| jammer_settings.h | Add hop_interval_ms, dwell_hold_ms, sweep_step_hz, noise_step_hz, power_level, bypass_region, max_tx_duration_ms, tx_gate_mode. |
| jammer_settings.c | Add FIELD_ defines, defaults, temp_ vars + read_*, insert_or_update_*, bump applied guard to 17. |
| jammer_signal.h | Add JammerHopQueue to JammerSignal; add jammer_signal_hop_enqueue. |
| jammer_signal.c | Zero-init hop_q; implement jammer_signal_hop_enqueue. |
| jammer_freq.h | Add prototypes for the 5 new pure functions. |
| jammer_freq.c | Implement the 5 new pure functions. |
| jammer_radio.h | Add jammer_radio_commit_hop, jammer_radio_set_tx_power, jammer_radio_pre_tx_check, jammer_radio_get_hop_count, jammer_radio_get_current_freq. |
| jammer_radio.c | Implement commit_hop (restart sequence), set_tx_power (PA_TABLE), pre_tx_check. |
| subghz_jammer.h | Expose JammerSignal* / scheduler state. |
| subghz_jammer.c | Replace jammer_radio_set_frequency (98) with commit_hop at hop_interval_ms; apply thermal throttle; arm watchdog; send hop events. |
| scenes/subghz_jammer_scene_menu.c | Restructure to hierarchical menu. |
| scenes/subghz_jammer_scene_start.c | Live status display. |
| tests/test_jammer_freq.c | Add tests for the 5 new functions. |

---

## 5. Tests & Verify

### 5.1 Host tests (test_host.sh, mirrors test_jammer_freq.c)
- test_jammer_freq.c: jammer_freq_step_target, jammer_freq_dwell_target, jammer_freq_noise_hop_target, jammer_freq_hop_count_for_span, jammer_freq_hop_queue_next.
- test_jammer_hop.c: hop schedule + ring buffer wrap.
- test_jammer_thermal.c: thermal throttle decision table.
- test_jammer_pretx.c: pre-TX validation decision logic.

### 5.2 Firmware build
```bash
export PATH="$PWD/toolchain/x86_64-windows/python:$PWD/toolchain/x86_64-windows/bin:$PATH"
export FBT_NO_SYNC=1
python -m SCons TARGET_HW=7
```
Verify symbols: `arm-none-eabi-nm build/f7-firmware-C/firmware.elf | grep -E 'jammer_radio_commit_hop|jammer_pre_tx_check|jammer_radio_watchdog'`. Confirm no stray IWDG: `arm-none-eabi-nm build/f7-firmware-C/firmware.elf | grep -c 'IWDG initialized'` must be 0.

### 5.3 On-device test
- Unit tests: `FIRMWARE_APP_SET=unit_tests`; send resources; run `unit_tests test_jammer_*`.
- Smoke: flash; launch app; set mode=Sweep with hop_interval_ms small; capture RF with spectrum analyzer/SDR; confirm freq changes at each hop.
- Watchdog: set max_tx_duration_ms low; confirm TX stops at ~max.
- Safety: force out-of-region freq (extend off); confirm pre-TX dialog blocks.
- Radio swap: Internal vs ExternalCC1101 (OTG on); confirm PA table loads.
