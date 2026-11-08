#include "jammer_thermal.h"

JammerThermalDecision jammer_thermal_decide(
    uint32_t junction_temp_c,
    uint32_t avg_current_ma,
    uint32_t vbat_mv,
    uint32_t max_tx_duration_ms,
    uint32_t elapsed_ms,
    bool duty_already_reduced) {
    /* 5. Watchdog: hard stop on the TX-timeout cap (safety, highest priority). */
    if(max_tx_duration_ms != 0u && elapsed_ms >= max_tx_duration_ms) {
        return JammerThermalForceStop;
    }
    /* 4. Critical battery: hard stop. vbat 0 means "unknown" -> don't stop. */
    if(vbat_mv != 0u && vbat_mv <= JAMMER_THERMAL_V_CRIT_MV) {
        return JammerThermalForceStop;
    }
    /* 1. Overheat or over-current: reduce duty cycle (once per session). */
    if(junction_temp_c > JAMMER_THERMAL_T_WARN_C || avg_current_ma > JAMMER_THERMAL_I_THERMAL_MA) {
        if(!duty_already_reduced) {
            return JammerThermalReduceDuty;
        }
    }
    /* 2/3. Still too hot, over max current, or low battery voltage: reduce power. */
    if(junction_temp_c > JAMMER_THERMAL_T_WARN_C ||
       avg_current_ma > JAMMER_THERMAL_I_MAX_MA ||
       (vbat_mv != 0u && vbat_mv < JAMMER_THERMAL_V_MIN_MV)) {
        return JammerThermalReducePower;
    }
    return JammerThermalOk;
}

uint32_t jammer_thermal_est_current_ma(uint32_t freq_hz, uint32_t power_level, bool external_pa) {
    /* power_level 0..100 -> PA level 1..31 (0 = auto -> max). */
    uint32_t level = (power_level == 0u) ? 31u : ((power_level * 31u + 99u) / 100u);
    if(level < 1u) level = 1u;

    /* mA per PA level, rising with frequency: ~11 @<=433 MHz, ~16 @>=915 MHz;
     * external PA adds load across the band. */
    uint32_t ma_per_level;
    if(freq_hz <= 434000000u) {
        ma_per_level = external_pa ? 16u : 11u;
    } else if(freq_hz >= 915000000u) {
        ma_per_level = external_pa ? 22u : 16u;
    } else {
        uint32_t frac = (freq_hz - 434000000u) * 100u / (915000000u - 434000000u);
        uint32_t low = external_pa ? 16u : 11u;
        uint32_t high = external_pa ? 22u : 16u;
        ma_per_level = low + ((high - low) * frac / 100u);
    }

    const uint32_t QUIESCENT_MA = 5u;
    return QUIESCENT_MA + (ma_per_level * level) / 3u;
}

uint32_t jammer_thermal_est_temp_c(uint32_t t_amb_c, uint32_t avg_current_ma) {
    /* junction = ambient + R_th * I_avg; R_th ~ 0.3 degC/mA so ~100 mA adds
     * ~30 C above ambient. */
    const uint32_t R_TH_PER_MA_NUM = 3u;
    const uint32_t R_TH_PER_MA_DEN = 10u;
    uint32_t delta = (avg_current_ma * R_TH_PER_MA_NUM) / R_TH_PER_MA_DEN;
    return t_amb_c + delta;
}
