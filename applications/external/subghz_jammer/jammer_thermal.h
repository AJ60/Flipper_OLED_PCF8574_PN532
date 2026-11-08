#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Thermal throttle model for the Sub-Ghz jammer.
 *
 * The jammer has no INA219, so junction temperature and average current are
 * ESTIMATED from the transmit frequency, power level, and duty cycle. The
 * estimate drives a conservative throttle decision (reduce duty -> reduce
 * power -> stop) so the radio cannot thermally damage itself.
 *
 * All functions here are pure and host-testable (no Furi dependency).
 */

/* Thresholds. */
#define JAMMER_THERMAL_T_WARN_C     60u  /* junction temp warning (deg C) */
#define JAMMER_THERMAL_I_THERMAL_MA 100u /* avg current warning (mA) */
#define JAMMER_THERMAL_I_MAX_MA     150u /* avg current hard limit (mA) */
#define JAMMER_THERMAL_V_MIN_MV     3500u /* battery low-voltage warning (mV) */
#define JAMMER_THERMAL_V_CRIT_MV    3300u /* battery critical (mV) */

typedef enum {
    JammerThermalOk,        ///< No action needed.
    JammerThermalReduceDuty, ///< Reduce duty cycle (reuse pulse_ratio).
    JammerThermalReducePower,///< Step power level down.
    JammerThermalForceStop,  ///< Stop TX now (battery critical / watchdog).
} JammerThermalDecision;

/**
 * Pure thermal-throttle decision (the decision table from the plan).
 *
 * Evaluated by the worker between hops. Priority: watchdog stop > battery
 * stop > reduce duty (once) > reduce power > ok.
 *
 * @param junction_temp_c   estimated junction temperature (deg C)
 * @param avg_current_ma    estimated average TX current (mA, duty-scaled)
 * @param vbat_mv           battery voltage (mV); 0 => unknown (ignore)
 * @param max_tx_duration_ms watchdog cap; 0 => disabled
 * @param elapsed_ms        TX elapsed time (ms)
 * @param duty_already_reduced true if duty was already reduced this session
 * @return the action the worker should take
 */
JammerThermalDecision jammer_thermal_decide(
    uint32_t junction_temp_c,
    uint32_t avg_current_ma,
    uint32_t vbat_mv,
    uint32_t max_tx_duration_ms,
    uint32_t elapsed_ms,
    bool duty_already_reduced);

/**
 * Estimate the peak (100%-duty) average TX current (mA) at a frequency/power.
 *
 * ~11 mA/level @433 MHz, ~16 mA/level @915 MHz (external PA adds load), with a
 * linear rise across the band. Monotonic in power level and frequency.
 *
 * @param freq_hz       transmit frequency (Hz)
 * @param power_level   power level 0..100 (0 = auto -> max)
 * @param external_pa   true for boards with an external power amplifier
 * @return estimated peak current (mA)
 */
uint32_t jammer_thermal_est_current_ma(uint32_t freq_hz, uint32_t power_level, bool external_pa);

/**
 * Estimate junction temperature (deg C) from ambient and average current.
 *
 * junction = ambient + R_th * I_avg, with R_th chosen so ~100 mA adds ~30 C.
 *
 * @param t_amb_c        ambient temperature (deg C)
 * @param avg_current_ma estimated average TX current (mA, duty-scaled)
 * @return estimated junction temperature (deg C)
 */
uint32_t jammer_thermal_est_temp_c(uint32_t t_amb_c, uint32_t avg_current_ma);
