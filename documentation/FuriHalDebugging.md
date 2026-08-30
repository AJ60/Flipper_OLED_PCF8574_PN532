# Furi HAL Debugging {#furi_hal_debugging}

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---



Some Furi subsystems have additional debugging features that can be enabled by adding additional defines to firmware compilation.
Usually, they are used for low level tracing and profiling or signal redirection/duplication.


## FuriHalOs

`--extra-define=FURI_HAL_OS_DEBUG` enables tick, tick suppression, idle and time flow.

There are 3 signals that will be exposed to external GPIO pins:

- `AWAKE`   — `PA7` — High when system is busy with computations, low when sleeping. Can be used to track transitions to sleep mode.

> [!WARNING]
> **DIY Hardware Conflict**: `PA7` is also used as the **PN532 I2C3 SCL** pin on this build (header label "C0"). Do **not** enable `FURI_HAL_OS_DEBUG` with the PN532 connected — the debug signal will conflict with I2C3 bus traffic and corrupt NFC communication. Disconnect the PN532 module before using this debug mode.
- `TICK`    — `PA6` — Flipped on system tick, only flips when no tick suppression in progress. Can be used to track tick skew and abnormal task scheduling.
- `SECOND`  — `PA4` — Flipped each second. Can be used for tracing RT issue: time flow disturbance means system doesn't conform Hard RT.



## FuriHalPower

`--extra-define=FURI_HAL_POWER_DEBUG` enables power subsystem mode transitions tracing.

There are 2 signals that will be exposed to external GPIO pins:

- `WFI`     — `PB2` — Light sleep (wait for interrupt) used. Basically, this is the lightest and most non-breaking things power save mode. All functions and debug should work correctly in this mode.
- `STOP`    — `PC3` — STOP mode used. Platform deep sleep mode. Extremely fragile mode where most of the silicon is disabled or in unusable state. Debugging MCU in this mode is nearly impossible.

## FuriHalSD

`--extra-define=FURI_HAL_SD_SPI_DEBUG` enables SD card SPI bus logging.
