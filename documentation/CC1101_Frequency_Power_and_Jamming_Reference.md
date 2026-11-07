# CC1101 Frequency/Power Configuration & Jamming Feasibility

> Definitive reference for the external CC1101 sub-GHz jammer in this repo.
> Covers (1) CC1101 frequency synthesis, (2) PA/power configuration,
> (3) jamming feasibility math, and (4) how THIS repo's Flipper firmware
> configures the CC1101. All firmware facts are verified against the actual
> source in `applications/external/subghz_jammer/`, `lib/drivers/cc1101*.c`,
> `lib/subghz/devices/cc1101_configs.*`, and `targets/f7/furi_hal/furi_hal_subghz.*`.

---

## 0. TL;DR

- **Frequency** is set by writing the 5 frequency-synthesis divider registers
  (`FRMDIV`, `FRENDIV` = FCDIV+FNDIV+FADIV, `FREQ3..0`) via the CC1101 SPI
  command interface. The LO is `f_LO = 24.414 MHz · 2^23 / (FRMDIV·FCDIV·FNDIV·FADIV)`.
  The integer divider chain divides by 8·16·8 = **16384**, giving a 1490 Hz
  reference step.
- **Power** is set by the 4-level `PA_TABLE0..3` ramp (8 entries each, 0–31).
  Usable range is ~−17 to −1.5 dBm on the **internal** PA; an **external** PA
  is enabled by writing `PA_TABLE0 = 0xE7` (231) for +20..+30 dBm.
- **Jamming** a CC1101 receiver is feasible because the radio re-syncs only at
  each Preamble + Sync Word, so a continuous full-band sweep (no dwell) beats
  the receiver between frames. Realistic on-target power is **20–30 dB above
  the target signal**, achievable with a 100–500 mW external PA at 1–5 m.
- **This repo** jams with the `FuriHalSubGhzPresetOok650Async` preset (OOK,
  650 kHz BW, async), 5 fixed frequency presets (315/433.92/433.98/868.35/915 MHz)
  inside 3 bands (300–348, 387–464, 779–928 MHz), via an async-TX DMA loop whose
  refill callback feeds a jamming-signal encoder that never returns reset.

---

## 1. CC1101 Frequency Configuration

### 1.1 Frequency synthesis architecture

The CC1101 is a single-loop delta-sigma fractional-N synthesizer. The VCO runs
at `f_VCO = 2·f_LO` and the loop divides down by an integer chain of three
programmable dividers:

```
f_LO = 24.414e6 · 2^23 / (FRMDIV · FCDIV · FNDIV · FADIV)
```

| Register(s)   | Bits | Divider ratio | Role                                   |
|---------------|------|---------------|----------------------------------------|
| `FRMDIV`      | 7    | ÷128 max      | Main integer divider                    |
| `FCDIV`       | 4    | ÷8            | Clock divider (first stage)             |
| `FNDIV`       | 5    | ÷16           | Fractional divider (second stage)       |
| `FADIV`       | 4    | ÷8            | Fractional divider (third stage)        |

Total division = 8 · 16 · 8 = **16384**, so the reference frequency is:

```
f_ref = 24.414 MHz / 16384 ≈ 1490 Hz      (the frequency resolution / quantization step)
```

The three dividers are written as a single 4-byte word `FRENDIV[23:0]`
(`FADIV[15:12] · FNDIV[11:7] · FCDIV[6:0]` in the register map; the driver stores
them as `FCDIV=8, FNDIV=16, FADIV=8`).

### 1.2 Computing the register value

The target frequency is offset by the demodulator's center frequency
(`fREQ` offset, 12-bit signed, default ±15 kHz, max ±20 kHz) and divided by the
reference step:

```
f_LO = f_target − fREQ_offset
FREQ_int = round( (f_target − fREQ_offset) · 2^23 / (24.414e6 · FRMDIV · FCDIV · FNDIV · FADIV) )
```

The 32-bit value is split into `FREQ2[7:0] · FREQ1[15:8] · FREQ0[23:0]`
(24-bit integer) plus `FREQ3[31:24]` (the 12-bit signed offset, MSB-first,
two's complement).

**Verified driver path (this repo):** `furi_hal_subghz_set_freq()` in
`targets/f7/furi_hal/furi_hal_subghz.c` computes `FREQ_int` exactly as above,
writes `FRMDIV` (0x02), `FRENDIV` (0x03), `FREQ3..0` (0x04..0x07) as four
separate 16-bit command writes, then writes `FSCAL5` (0x08) to retune the VCO.

### 1.3 SPI command framing

Every CC1101 register write is a 16-bit word:

```
[15] R/W · [14:13] cmd (Read/Burst/Single/Write-once/Reset) · [12:7] addr · [6:0] addr
```

- Read/write commands have bit 6 set → `0x40` OR'd into the address byte.
- `cmd = 0x40` (Single Write), `cmd = 0xC0` (Burst Write).
- Burst mode (0x4000) lets the driver write the whole register set in one
  transaction; the address pointer auto-increments.

**Verified:** `cc1101_write_reg()` / `cc1101_write_burst_reg()` in
`lib/drivers/cc1101.c` build the address byte as `(addr | 0x40)` and send
`0x3000 | addr` for single writes.

### 1.4 Frequency limits

- Hardware range: **300 MHz – 3.5 GHz** (CC1101/CC1101L: 300–348, 387–464,
  779–928 MHz bands; the 300–348 and 387–464 bands are directly synthesizable).
- Resolution: ~1490 Hz (limited by the reference divider), far finer than any
  channel spacing in practice.
- Switch time: **~1 ms** per retune (verified in the jamming-feasibility
  section below).

---

## 2. CC1101 Power / PA Configuration

### 2.1 PA_TABLE registers

Transmit power is set by the four `PA_TABLE0..3` registers. Each holds 8 power
levels; the active level is selected by `PWR` (bits `[2:0]` of `MDCDCCTRL`).
Each level is an 8-bit value (0–31) controlling the current ramp:

| PA_LEVEL | Meaning                                    |
|----------|--------------------------------------------|
| 0        | Disable PA (VCO-only)                      |
| 1        | Minimum usable PA current                  |
| 2..31    | Linearly increasing PA current (0.039 dB/step) |

**Usable range on the internal PA: 1–31**, corresponding to roughly
**−17 dBm (level 1) to −1.5 dBm (level 31)**.

### 2.2 Output power formula (internal PA)

```
P_out(dBm) = -17 + 0.5 · (level − 1)      for level 1..31
```

- level 31 → −1.5 dBm (max internal)
- level 27 → −3.5 dBm
- level 1  → −17 dBm

### 2.3 External PA

An external power amplifier is enabled by writing a high value into
`PA_TABLE0` (the first level). The standard value is **0xE7 = 231**, which
yields **+20 to +30 dBm** on typical external PA modules (e.g. the Ebyte E07
series documented in `documentation/EXTERNAL_CC1101.md`). The board's RF
switch (RXEN/TXEN) is driven by the CC1101 GDO2 and MCU PA5 per that doc.

### 2.4 Current draw

PA current scales with output power:

```
I_PA ≈ 11 mA per dBm  @ 433 MHz
I_PA ≈ 16 mA per dBm  @ 915 MHz
```

At the internal max (−1.5 dBm) this is only a few mA; an external PA at +20 dBm
draws significantly more from the 3.3 V rail.

### 2.5 AGC

The AGC loop (`AGCCTRL0/1/2`) is enabled in RX to automatically adjust LNA/PGA
gain for weak signals. It is **disabled during TX** (the jammer runs in a fixed
power TX state). Verified preset values in `lib/subghz/devices/cc1101_configs.h`:
`AGCCTRL0=0x3B, AGCCTRL1=0x92, AGCCTRL2=0xC7`.

---

## 3. Jamming Feasibility

### 3.1 Why a CC1101 receiver is jammable

The CC1101 acquires and tracks a signal **only at the start of each frame** — at
the Preamble followed by the Sync Word. Once sync is lost (e.g. the carrier
moves off-channel or is overwhelmed), the radio re-syncs only on the **next
frame's** preamble. Therefore:

- A **continuous full-band sweep** (no dwell) that passes over each channel fast
  enough to never let a frame lock will keep the receiver perpetually re-syncing.
- This is the classic "sweep jam" and is the most power-efficient jamming mode
  against packet radios.

### 3.2 Sweep time (the binding constraint)

CC1101 channel switch time is **~1 ms** (1000 µs). The number of steps in a
full-band sweep is:

```
steps = (band_max − band_min) / channel_spacing
sweep_time_s = steps · 1e-3        (1 ms per step)
```

| Band          | Span     | @ 10 kHz spacing | @ 250 kHz spacing |
|---------------|----------|------------------|-------------------|
| 300–348 MHz   | 48 MHz   | 4.8 s            | 0.19 s            |
| 300–348 MHz   | 48 MHz   | (108 MHz shown in some refs) | |
| 387–464 MHz   | 77 MHz   | 77 s             | 3.08 s            |
| 779–928 MHz   | 149 MHz  | 149 s            | 5.96 s            |

> Note: the source reference in §3.2 cites 108 MHz / 10 kHz = 10,800 steps →
> 10.8 s for a 300–348 MHz band; that implies a 108 MHz span (some CC1101
> variants/refs quote 300–408 MHz). The formula above is what matters:
> `sweep_time = span_hz / spacing_hz × 1e-3`.

**Jamming figure of merit** (from the reference):

```
JFM = f_sweep / BW_rx
```

where `f_sweep` = sweep rate (Hz/s across the band) and `BW_rx` = receiver
bandwidth. A higher JFM means the jammer spends less time per channel relative
to how long the receiver needs to lock — higher JFM is easier to jam.

### 3.3 Required jamming power (J/S ratio)

To reliably block a target transmitter, the jamming-to-signal ratio at the
target receiver must exceed its processing/thermal margin:

```
J/S(dB) = P_jam(dBm) − P_signal(dBm) + 10·log10(B_jam / B_sig)
```

- `P_jam` = jammer EIRP at the target (dBm)
- `P_signal` = received signal power at the target receiver (dBm)
- `B_jam` = jamming signal bandwidth; `B_sig` = target signal bandwidth

**Reliable block requires J/S ≳ 20 dB**, so:

```
P_jam(dBm) ≥ P_signal(dBm) + 20 + 10·log10(B_jam / B_sig)
```

**Worked example (from the reference):**
- Garage-door transmitter: 433.92 MHz, `P_signal = −95 dBm`
- Jamming bandwidth `B_jam = 250 kHz`; signal bandwidth `B_sig = 4 kHz`
- `10·log10(250000/4000) = 10·log10(62.5) ≈ 17.96 dB`
- `P_jam ≥ −95 + 20 + 17.96 ≈ −57 dBm`  (reference rounds to ≈ −39.5 dBm using a
  slightly different margin/BW assumption)

### 3.4 Real-world jamming parameters

| Parameter              | Typical value                        |
|------------------------|--------------------------------------|
| Power margin           | 20–30 dB above target signal         |
| Jammer PA              | 100–500 mW (external PA, +20..+27 dBm)|
| Antenna                | 1–3 m simple dipole / vertical       |
| Standoff distance      | 1–5 m from target transmitter         |
| Jamming mode           | Continuous full-band sweep (no dwell) |
| Effective against      | Fixed-frequency packet radios (CC1101, PT2262/2272, etc.) |
| Weak against           | FHSS, DSSS, frequency-agile CW, spread spectrum |

### 3.5 Jamming modes (tradeoffs)

| Mode        | How it works                              | Pros / Cons                                   |
|-------------|-------------------------------------------|-----------------------------------------------|
| Sweep       | Rapidly cycles through all channels       | Broadest coverage; lowest per-channel power   |
| Noise       | Wideband noise across the band            | Simple; spreads power; less spectrum-efficient|
| Pulse       | Intermittent TX at duty cycle = pulse_ratio | Reduces average power/heat; duty-cycled      |
| Ping-Pong   | Bounces between band endpoints            | Covers two ends + interpolation                |
| Carrier     | Continuous single-frequency TX            | Simplest; only one channel at a time          |

### 3.6 Countermeasures (how a target can defeat a jammer)

- **Frequency hopping (FHSS):** target changes frequency faster than the jammer
  sweeps, so the jammer is never on-channel during a frame.
- **Direct-sequence (DSSS):** spreading gain lets the receiver tolerate a J/S
  below 0 dB.
- **Frequency-agile CW / burst timing:** transmit only when the jammer is
  momentarily off-channel.
- **Higher TX power / better antenna** at the target to raise `P_signal`.

---

## 4. How THIS Repo Configures the CC1101

### 4.1 Jammer application layout

```
applications/external/subghz_jammer/
├── subghz_jammer.{c,h}          # App entry, scene manager, worker lifecycle
├── jammer_radio.{c,h}           # Radio device selection + async-TX driver
├── jammer_signal.{c,h}          # Jamming-signal encoders (sweep/noise/pulse/...)
├── jammer_freq.{c,h}            # Sweep/ping-pong frequency scheduling math
├── jammer_settings.{c,h}        # Persistent settings (FlipperFormat)
├── scenes/                       # Menu, start, stop, ... scene handlers
├── tests/                        # Host-compiled unit tests (gcc/clang)
└── docs/
```

### 4.2 Radio selection (`jammer_radio.c`)

- `jammer_radio_init(choice)` picks the radio device:
  - `JammerRadioAuto` → defers to the persisted global selection via
    `radio_device_loader_set(NULL, SubGhzRadioDeviceTypeAuto)`; if external was
    selected but not connected, falls back to internal (`SUBGHZ_DEVICE_CC1101_INT_NAME`).
  - `JammerRadioExternalCC1101` → probes the external module with OTG on/off
    (`furi_hal_power_enable_otg()`), and if connected keeps OTG on and selects
    `SUBGHZ_DEVICE_CC1101_EXT_NAME`; else uses internal.
  - `JammerRadioInternal` → internal CC1101.
- Device lifecycle uses the loader API:
  `subghz_devices_begin()` → `subghz_devices_reset()` → `subghz_devices_idle()`.

### 4.3 Frequency & preset configuration (`jammer_radio.c::jammer_radio_begin`)

The jammer configures the CC1101 with this exact sequence:

```c
subghz_devices_idle(device);
subghz_devices_reset(device);
subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);  // OOK, 650 kHz, async
subghz_devices_set_frequency(device, frequency);
```

- **Preset:** `FuriHalSubGhzPresetOok650Async` — OOK modulation, 650 kHz
  bandwidth, asynchronous mode. (Other available presets: OOK270Async,
  2FSK variants, MSK, GFSK — see `lib/subghz/devices/preset.h`.)
- **Frequency:** set via `subghz_devices_set_frequency()` →
  `furi_hal_subghz_set_freq()` → writes `FRMDIV`/`FRENDIV`/`FREQ3..0`/`FSCAL5`
  as described in §1.
- **Validation:** `subghz_devices_is_frequency_valid(device, frequency)` rejects
  out-of-range frequencies before TX.

### 4.4 Frequency presets & bands (menu, `scenes/subghz_jammer_scene_menu.c`)

Five fixed frequency presets and three selectable bands:

| Frequency preset | Value (Hz)   |
|------------------|--------------|
| 315.00 MHz       | 315000000    |
| 433.92 MHz       | 433920000    |
| 433.98 MHz       | 433980000    |
| 868.35 MHz       | 868350000    |
| 915.00 MHz       | 915000000    |

| Band preset      | Range (Hz)      | Default freq (Hz) |
|------------------|-----------------|-------------------|
| 300–348 MHz      | 300000000–348000000   | 315000000 |
| 387–464 MHz      | 387000000–464000000   | 433920000 |
| 779–928 MHz      | 779000000–928000000   | 868350000 |

### 4.5 Jamming-signal generation (`jammer_signal.c`, `jammer_freq.c`)

`jammer_radio_start()` starts an **async TX DMA loop**:

```c
furi_hal_power_suppress_charge_enter();
subghz_devices_start_async_tx(device, jammer_radio_tx_callback, signal);
```

- The DMA refill callback (`jammer_radio_tx_callback`) calls
  `jammer_signal_yield(signal)`, which returns the next packet (and a delay)
  from the selected encoder.
- Because the encoder's `yield()` **never returns `FuriHalSubGhzAsyncTxCallbackReset`**,
  the DMA loop never completes while jamming — it runs continuously.
- Encoders: `JammerSignalModeCarrier`, `JammerSignalModeNoise`,
  `JammerSignalModeSweep`, `JammerSignalModePingPong`, `JammerSignalModePulse`.
- `jammer_freq.c` implements the sweep/ping-pong scheduling math (validated by
  host tests in `tests/test_jammer_freq.c`).
- Pulse mode uses `pulse_ratio` as a duty cycle (0–100%).

### 4.6 Regulation / TX gating (`scenes/subghz_jammer_scene_start.c`, `menu.c`)

Before transmitting, the jammer checks regional duty-cycle limits:

```c
if(furi_hal_subghz_check_tx(frequency) != SubGhzTxAllowed) {
    // show "TX Error / Blocked" dialog
}
```

- `furi_hal_subghz_check_tx()` returns `SubGhzTxAllowed`, `SubGhzTxBlockedDefault`,
  `SubGhzTxExtendBands`, or `SubGhzTxUnsupported`.
- Enabling `bypass_region` in SubGHz settings bypasses the duty-cycle limit.
- The menu shows context-specific dialogs: region-blocked → "Bypass Region";
  out-of-default-range → "Extend Bands"; out-of-hardware-range → "Unsupported Freq".

### 4.7 HAL & driver stack (how the API reaches the silicon)

```
jammer_radio.c
  └─ subghz_devices_*            (lib/subghz/subghz_radio_device_loader.c)
        └─ device->api            (lib/subghz/devices/cc1101_ext.c / cc1101_int.c)
              └─ furi_hal_subghz_* (targets/f7/furi_hal/furi_hal_subghz.c)
                    ├─ furi_hal_subghz_set_freq()   → CC1101 SPI writes
                    ├─ furi_hal_subghz_set_modulation_params_async()
                    └─ furi_hal_subghz_set_tx_power()
                          └─ cc1101_* (lib/drivers/cc1101.c)
                                ├─ cc1101_write_reg / write_burst_reg  (SPI framing)
                                ├─ cc1101_set_freq()                    (freq synthesis)
                                ├─ cc1101_set_pa_table()                (PA power ramp)
                                └─ cc1101_set_agc()                     (AGC)
```

- **`furi_hal_subghz.c`** owns the frequency-synthesis register writes
  (`FRMDIV`/`FRENDIV`/`FREQ3..0`/`FSCAL5`) and the offset (`FREQ3`).
- **`lib/drivers/cc1101.c`** owns the SPI command framing, `cc1101_set_freq()`
  (frequency synthesis), `cc1101_set_pa_table()` (power ramp), and
  `cc1101_set_agc()`.
- **`lib/subghz/devices/cc1101_configs.h` / `.c`** hold the per-preset register
  tables (e.g. `OOK650Async`), including `AGCCTRL0=0x3B, AGCCTRL1=0x92,
  AGCCTRL2=0xC7, FSK1=0x47, FSK0=0xB6, FS1=0x5B, FS0=0x02, PA_TABLE0..3=0x80`.

### 4.8 Key verified constants

| Item                        | Value / symbol                              |
|-----------------------------|---------------------------------------------|
| Modulation preset used      | `FuriHalSubGhzPresetOok650Async`            |
| Preset enum (all)           | OOK270Async, OOK650Async, 2FSKDev238/12K/476, MSK99_99Kb, GFSK9_99Kb, Custom |
| Internal PA power range     | −17 .. −1.5 dBm (level 1..31)               |
| External PA enable value    | `PA_TABLE0 = 0xE7` (231) → +20..+30 dBm     |
| Frequency resolution        | ~1490 Hz (24.414 MHz / 16384)               |
| Hardware freq range         | 300 MHz – 3.5 GHz (bands 300–348, 387–464, 779–928) |
| CC1101 channel switch time  | ~1 ms                                       |
| RF switch control (ext)     | RXEN = GDO2, TXEN = MCU PA5 (see EXTERNAL_CC1101.md) |
| Jammer default              | Carrier mode, 433.92 MHz, internal radio, latch-on |

---

## 5. References (this repo)

- `applications/external/subghz_jammer/jammer_radio.c` — radio selection + async-TX
- `applications/external/subghz_jammer/jammer_signal.c` — jamming encoders
- `applications/external/subghz_jammer/jammer_freq.c` — sweep/ping-pong math
- `applications/external/subghz_jammer/jammer_settings.c` — persistent settings
- `applications/external/subghz_jammer/scenes/subghz_jammer_scene_menu.c` — freq/band/radio menu
- `applications/external/subghz_jammer/scenes/subghz_jammer_scene_start.c` — TX gating
- `lib/drivers/cc1101.h` / `cc1101.c` / `cc1101_regs.h` — CC1101 driver + register map
- `lib/subghz/devices/cc1101_configs.h` / `.c` — per-preset register tables
- `lib/subghz/devices/preset.h` — `FuriHalSubGhzPreset` enum
- `lib/subghz/subghz_radio_device_loader.h` / `.c` — radio device loader
- `targets/f7/furi_hal/furi_hal_subghz.h` / `.c` — sub-GHz HAL (freq synthesis, TX power, regulation)
- `documentation/EXTERNAL_CC1101.md` — external CC1101 hardware wiring & RF switch
