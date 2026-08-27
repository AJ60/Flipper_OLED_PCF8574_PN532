#include <furi_hal_power.h>
// Keep necessary includes for types used in function signatures
#include <furi.h> // For FURI_NORETURN, basic types, PropertyValueCallback
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include <furi_hal_clock.h>
#include <furi_hal_bt.h>
#include <furi_hal_vibro.h>
#include <furi_hal_resources.h>
#include <furi_hal_adc.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_rtc.h>
#include <furi_hal_debug.h>
#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_pwr.h>
#include <stm32wbxx_ll_hsem.h>
#include <stm32wbxx_ll_cortex.h>
#include <stm32wbxx_ll_gpio.h>
#include <hsem_map.h>
#include <bq27220.h>
#include <bq27220_data_memory.h>
#include <bq25896.h>

#ifdef USE_INA219
#include <furi_hal_ina219.h>
#include <string.h>
#endif

#define TAG "FuriHalPower"

// Remove TAG definition as logging won't happen

// Remove debug GPIO defines
// #ifndef FURI_HAL_POWER_DEBUG_WFI_GPIO
// #define FURI_HAL_POWER_DEBUG_WFI_GPIO (&gpio_ext_pb2)
// #endif
// #ifndef FURI_HAL_POWER_DEBUG_STOP_GPIO
// #define FURI_HAL_POWER_DEBUG_STOP_GPIO (&gpio_ext_pc3)
// #endif

// Remove STOP_MODE define
// #ifndef FURI_HAL_POWER_STOP_MODE
// #define FURI_HAL_POWER_STOP_MODE (LL_PWR_MODE_STOP2)
// #endif

typedef struct {
    volatile uint8_t insomnia;
    volatile uint8_t suppress_charge;
    bool gauge_ok;
    bool charger_ok;
} FuriHalPower;

static volatile FuriHalPower furi_hal_power = {
    .insomnia = 0,
    .suppress_charge = 0,
    .gauge_ok = false,
    .charger_ok = false,
};

const int32_t BATTERY_CAPACITY = 1200;
#ifdef USE_INA219
// INA219 wrapper state is tracked in its module
float curr_soc_percent = 100.0f;
const float R_INTERNAL = 0.25f;
#endif

static void furi_hal_power_ina_alert_isr(void* ctx) {
    UNUSED(ctx);
    FURI_LOG_E(TAG, "INA226 ALERT Triggered on PB1! Emergency overcurrent event");
}

void furi_hal_power_init(void) {
#ifdef USE_INA219
    FURI_LOG_I(TAG, "Initializing INA219/INA226 power sensor");
    furi_hal_ina219_init();
    if(furi_hal_ina219_is_ready()) {
        furi_hal_ina226_set_overcurrent_limit(2.0f);
        furi_hal_ina226_enable_alert_interrupt(furi_hal_power_ina_alert_isr, NULL);
    }
    FURI_LOG_I(TAG, "INA219/INA226 initialization complete");
#else
    FURI_LOG_I(TAG, "INA219 support not enabled at build time");
#endif
    // Initialize ADC so fallback path is ready
    furi_hal_adc_init();
}

bool furi_hal_power_gauge_is_ok(void) {
    // Return a default "OK" state
    return true;
}

bool furi_hal_power_is_shutdown_requested(void) {
    // Return a default "not requested" state
    return false;
}

uint16_t furi_hal_power_insomnia_level(void) {
    // Return a default "no insomnia" state
    // return 0;
    return furi_hal_power.insomnia;
}

void furi_hal_power_insomnia_enter(void) {
    // Do nothing
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia < UINT8_MAX);
    furi_hal_power.insomnia++;
    FURI_CRITICAL_EXIT();
}

void furi_hal_power_insomnia_exit(void) {
    // Do nothing
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia > 0);
    furi_hal_power.insomnia--;
    FURI_CRITICAL_EXIT();
}

bool furi_hal_power_sleep_available(void) {
    // Return a default "always available" state
    // return true;
    // return false;
    return furi_hal_power.insomnia == 0;
}

// Remove internal static functions as they are no longer needed
// static inline bool furi_hal_power_deep_sleep_available(void) { ... }
// static inline void furi_hal_power_light_sleep(void) { ... }
// static inline void furi_hal_power_suspend_aux_periphs(void) { ... }
// static inline void furi_hal_power_resume_aux_periphs(void) { ... }
// static inline void furi_hal_power_deep_sleep(void) { ... }

void furi_hal_power_sleep(void) {
    // Do nothing (don't actually sleep)
}

// Non-linear Li-ion discharge curve: voltage fraction (V_MIN..V_MAX) -> SOC %.
static float furi_hal_power_voltage_to_soc(float v, float v_min, float v_max) {
    float v_clamped = v;
    if(v_clamped < v_min) v_clamped = v_min;
    if(v_clamped > v_max) v_clamped = v_max;

    float v_norm = (v_clamped - v_min) / (v_max - v_min);
    if(v_norm < 0.1f) {
        return v_norm * 50.0f; // steep drop at low voltage (0-5%)
    } else if(v_norm < 0.3f) {
        return 5.0f + (v_norm - 0.1f) * 75.0f; // 5-20%
    } else {
        return 20.0f + (v_norm - 0.3f) * 114.3f; // 20-100%
    }
}

uint8_t furi_hal_power_get_pct(void) {
    const float V_MIN = 3.00f; // 0%
    const float V_MAX = 4.20f; // 100%
    static float soc_percent = 100.0f; // runtime state-of-charge estimate
    static uint32_t last_ms = 0;
    static float smoothed_v = 0.0f;
    static float smoothed_i = 0.0f;
    static uint32_t transient_since_ms = 0;

#ifdef USE_INA219
    if(furi_hal_ina219_is_ready()) {
        float v = 0.0f, i = 0.0f;
        if(furi_hal_ina219_get_voltage_current(&v, &i)) {
            // Implausible measurement (sensor glitch/unplugged): keep the last
            // estimate instead of jumping to an arbitrary value.
            if(v < 3.0f || v > 4.25f) {
                return (uint8_t)(curr_soc_percent + 0.5f);
            }

            // IR-compensated open-circuit voltage estimate. Software current
            // convention: i > 0 = charging, i < 0 = discharging.
            const float v_oc = v - (i * R_INTERNAL);

            uint32_t now = furi_get_tick();
            if(last_ms == 0) {
                // First sample: seed from the voltage curve.
                last_ms = now;
                soc_percent = furi_hal_power_voltage_to_soc(v_oc, V_MIN, V_MAX);
                curr_soc_percent = soc_percent;
                smoothed_v = v_oc;
                smoothed_i = i;
                transient_since_ms = 0;
            }

            uint32_t dt_ms = now - last_ms;
            last_ms = now;

            // EMA smoothing to reject measurement noise.
            const float ALPHA_EMA = 0.12f;
            float prev_smoothed_v = smoothed_v;
            smoothed_v = (ALPHA_EMA * v_oc) + ((1.0f - ALPHA_EMA) * smoothed_v);
            smoothed_i = (ALPHA_EMA * i) + ((1.0f - ALPHA_EMA) * smoothed_i);

            // Sudden voltage jump (charger plug/unplug) opens a transient window
            // during which instant voltage is not trusted.
            if(fabsf(v_oc - prev_smoothed_v) > 0.07f) {
                transient_since_ms = now;
            }
            bool in_transient = (transient_since_ms != 0) && ((now - transient_since_ms) < 8000);

            // Too soon for a meaningful coulomb update.
            if(dt_ms < 100) {
                return (uint8_t)(curr_soc_percent + 0.5f);
            }

            // Coulomb counting: dQ = I * dt  (I[A] * dt[ms] / 3600 = mAh)
            float delta_mAh = i * ((float)dt_ms / 3600.0f);
            float coulomb_soc = soc_percent + (delta_mAh / (float)BATTERY_CAPACITY) * 100.0f;

            // Adaptive blend of coulomb counting vs the voltage curve.
            float abs_i = (i < 0.0f) ? -i : i;
            float weight_coulomb = 0.85f;
            if(abs_i < 0.005f) {
                weight_coulomb = 0.25f; // taper/float — trust voltage more
            } else if(abs_i < 0.02f) {
                weight_coulomb = 0.50f;
            } else if(abs_i < 0.1f) {
                weight_coulomb = 0.75f;
            }
            // Near full charge with negligible current -> voltage dominates.
            if((smoothed_v > (V_MAX - 0.03f)) && (abs_i < 0.005f)) {
                float near_full = (V_MAX - smoothed_v) / 0.03f; // 0..1
                if(near_full < 0.0f) near_full = 0.0f;
                if(near_full > 1.0f) near_full = 1.0f;
                weight_coulomb *= near_full;
            }
            // During transients / charger plug-in, suppress instant voltage jumps.
            if(in_transient) {
                weight_coulomb = (i > 0.01f) ? 0.98f : 0.92f;
            } else if((i > 0.01f) && (fabsf(v_oc - prev_smoothed_v) > 0.03f)) {
                weight_coulomb = 0.95f;
            }
            float weight_voltage = 1.0f - weight_coulomb;

            float v_soc = furi_hal_power_voltage_to_soc(v_oc, V_MIN, V_MAX);
            float blended = (weight_coulomb * coulomb_soc) + (weight_voltage * v_soc);
            if(blended < 0.0f) blended = 0.0f;
            if(blended > 100.0f) blended = 100.0f;

            // Rate-limit instantaneous changes (avoid jumps on every sample).
            float max_delta_per_sec = in_transient ? 0.5f : 2.0f;
            float max_delta = max_delta_per_sec * ((float)dt_ms / 1000.0f);
            float delta = blended - curr_soc_percent;
            if(delta > max_delta) {
                blended = curr_soc_percent + max_delta;
            } else if(delta < -max_delta) {
                blended = curr_soc_percent - max_delta;
            }

            // Monotonic enforcement: SOC must not rise while discharging, nor
            // fall while charging.
            if((i < -0.01f) && (blended > curr_soc_percent)) {
                blended = curr_soc_percent;
            } else if((i > 0.01f) && (blended < curr_soc_percent)) {
                blended = curr_soc_percent;
            }

            // Low-pass filter the reported value.
            const float ALPHA_OUT = 0.25f;
            soc_percent = blended;
            curr_soc_percent = (ALPHA_OUT * soc_percent) + ((1.0f - ALPHA_OUT) * curr_soc_percent);
            if((i > 0.01f) && (curr_soc_percent > 99.0f)) {
                curr_soc_percent = 99.0f; // charging-only cap
            }

            return (uint8_t)(curr_soc_percent + 0.5f);
        }
    }
#endif

    // Default ADC fallback (used when the INA219/INA226 is not present)
    FuriHalAdcHandle* handle = furi_hal_adc_acquire();
    if(!handle) return 90;
    furi_hal_adc_configure(handle);
    uint16_t raw_vbat = furi_hal_adc_read(handle, FuriHalAdcChannelVBAT);
    uint16_t raw_vref = furi_hal_adc_read(handle, FuriHalAdcChannelVREFINT);
    float vref_mV = furi_hal_adc_convert_vref(handle, raw_vref);
    float adc_input_mV = ((float)raw_vbat) * vref_mV / 4095.0f;
    float vbat_mV = adc_input_mV * 3.0f;
    float vbat = vbat_mV / 1000.0f;
    furi_hal_adc_release(handle);
    return (uint8_t)(furi_hal_power_voltage_to_soc(vbat, V_MIN, V_MAX) + 0.5f);
}

uint8_t furi_hal_power_get_bat_health_pct(void) {
    // Return a default battery health percentage
    return 100;
}

bool furi_hal_power_is_charging(void) {
    // Get charge and discharge current status from power IC when available
#ifdef USE_INA219
    if(furi_hal_ina219_is_ready()) {
        float v = 0.0f, i = 0.0f;
        if(furi_hal_ina219_get_voltage_current(&v, &i)) {
            // Software current convention (see furi_hal_ina219_get_voltage_current,
            // which compensates for the reversed shunt wiring):
            //   i > 0 -> charging, i < 0 -> discharging.
            bool charging = (v > 3.3f) && (i > 0.005f);
            FURI_LOG_D(
                TAG,
                "INA219 voltage=%.3f V, current=%.3f A, charging=%s",
                (double)v,
                (double)i,
                charging ? "YES" : "NO");
            return charging;
        }
    }
#endif
    // Return a default "not charging" state (consistent with not charging)
    return false;
}

bool furi_hal_power_is_charging_done(void) {
    // Return a default "not charged" state (consistent with not charging)
    return false;
}

void furi_hal_power_shutdown(void) {
    // Must not return
    // TODO: Clear and deinit the screen first

    // TODO: Then deinit peripherals

    // Then Prepare Wakeup pin (boot0 pin)

    // Then Release RCC semaphore

    // Finally, vibrate briefly to indicate shutdown

    while(1) {
    }
}

void furi_hal_power_off(void) {
    // Do nothing
    furi_hal_power_shutdown();
}

FURI_NORETURN void furi_hal_power_reset(void) {
    NVIC_SystemReset();
}

bool furi_hal_power_enable_otg(void) {
    return false; // OTG is not supported on DIY board
}

void furi_hal_power_disable_otg(void) {
    // OTG is not supported on DIY board
}

bool furi_hal_power_is_otg_enabled(void) {
    return false; // OTG is not supported on DIY board
}

static float furi_hal_power_battery_charge_voltage_limit = 4.208f;

float furi_hal_power_get_battery_charge_voltage_limit(void) {
    return furi_hal_power_battery_charge_voltage_limit;
}

void furi_hal_power_set_battery_charge_voltage_limit(float voltage) {
    uint16_t voltage_mv = (uint16_t)roundf(voltage * 1000.0f);
    if(voltage_mv < 3840) {
        voltage_mv = 3840;
    } else if(voltage_mv > 4208) {
        voltage_mv = 4208;
    }
    uint8_t steps = (uint8_t)((voltage_mv - 3840) / 16);
    furi_hal_power_battery_charge_voltage_limit = (float)(3840 + steps * 16) / 1000.0f;
}

bool furi_hal_power_check_otg_fault(void) {
    // Return a default "no fault" state
    return false;
}

void furi_hal_power_check_otg_status(void) {
    // Do nothing
}

uint32_t furi_hal_power_get_battery_remaining_capacity(void) {
    // Return a default capacity (e.g., in mAh)
    return BATTERY_CAPACITY;
}

uint32_t furi_hal_power_get_battery_full_capacity(void) {
    // Return a default capacity (e.g., in mAh)
    return BATTERY_CAPACITY;
}

uint32_t furi_hal_power_get_battery_design_capacity(void) {
    // Return a default capacity (e.g., in mAh)
    return BATTERY_CAPACITY;
}

float furi_hal_power_get_battery_voltage(FuriHalPowerIC ic) {
    // Read battery voltage via ADC when available. Returns voltage in volts.
    (void)ic; // Suppress unused parameter warning

    // Try INA219 first when available
#ifdef USE_INA219
    if(furi_hal_ina219_is_ready()) {
        float v = 0.0f, i = 0.0f;
        if(furi_hal_ina219_get_voltage_current(&v, &i)) {
            float vbat = v - (i * R_INTERNAL);
            if(vbat < 0.0f) vbat = 0.0f;
            if(vbat > 4.2f) vbat = 4.2f;
            return vbat;
        }
    }
#endif

    // ADC fallback
    FuriHalAdcHandle* handle = furi_hal_adc_acquire();
    if(!handle) {
        return 3.7f; // fallback
    }

    furi_hal_adc_configure(handle);
    uint16_t raw_vbat = furi_hal_adc_read(handle, FuriHalAdcChannelVBAT);
    uint16_t raw_vref = furi_hal_adc_read(handle, FuriHalAdcChannelVREFINT);
    float vref_mV = furi_hal_adc_convert_vref(handle, raw_vref);
    float adc_input_mV = ((float)raw_vbat) * vref_mV / 4095.0f;
    float vbat_mV = adc_input_mV * 3.0f;
    float vbat = vbat_mV / 1000.0f;
    furi_hal_adc_release(handle);

    if(vbat < 3.2f) vbat = 0.0f;
    if(vbat > 4.2f) vbat = 4.2f;

    return vbat;
}

float furi_hal_power_get_battery_current(FuriHalPowerIC ic) {
    // Heuristic current estimator based on voltage change over time.
    // This treats the battery as an equivalent capacitor with effective
    // capacitance derived from nominal capacity. Result is returned in A (or mA based on context).
    (void)ic; // Suppress unused parameter warning

    // If INA219 is available, return its measured current (in A)
#ifdef USE_INA219
    if(furi_hal_ina219_is_ready()) {
        float v = 0.0f, i = 0.0f;
        if(furi_hal_ina219_get_voltage_current(&v, &i)) {
            FURI_LOG_D(TAG, "INA219 voltage=%.3f V, current=%.3f A", (double)v, (double)i);
            return i;
        }
    }
#endif

    // Fallback: non-blocking heuristic current estimator based on voltage change over time.
    static float last_v = 3.7f;
    static uint32_t last_tick = 0;
    static float last_current = 0.0f;

    uint32_t now = furi_get_tick();
    if(last_tick == 0) {
        last_v = furi_hal_power_get_battery_voltage(ic);
        last_tick = now;
        return 0.0f;
    }

    uint32_t dt_ms = now - last_tick;
    // Calculate new estimate only if at least 500 ms has passed to filter noise
    if(dt_ms >= 500) {
        float current_v = furi_hal_power_get_battery_voltage(ic);
        float dv = current_v - last_v;
        float dt = (float)dt_ms / 1000.0f; // seconds

        const float V_MIN = 3.00f;
        const float V_MAX = 4.20f;
        float capacity_mAh = (float)furi_hal_power_get_battery_full_capacity();
        float capacity_Ah = capacity_mAh / 1000.0f;

        float Ceq = (capacity_Ah * 3600.0f) / (V_MAX - V_MIN);
        float dvdt = dv / dt;
        float i_a = Ceq * dvdt;
        if(i_a > 5.0f) i_a = 5.0f;
        if(i_a < -5.0f) i_a = -5.0f;

        last_v = current_v;
        last_tick = now;
        last_current = i_a;
    }

    return last_current;
}

// Remove internal static function
// static float furi_hal_power_get_battery_temperature_internal(FuriHalPowerIC ic) { ... }

float furi_hal_power_get_battery_temperature(FuriHalPowerIC ic) {
    // Return a default room temperature (in Celsius)
    (void)ic; // Suppress unused parameter warning
    return 25.0f;
}

float furi_hal_power_get_usb_voltage(void) {
    // Return a default voltage (0.0f assumes disconnected)
    return 0.0f;
}

void furi_hal_power_enable_external_3_3v(void) {
    // Do nothing
}

void furi_hal_power_disable_external_3_3v(void) {
    // Do nothing
}

void furi_hal_power_suppress_charge_enter(void) {
    // Do nothing
}

void furi_hal_power_suppress_charge_exit(void) {
    // Do nothing
}

void furi_hal_power_info_get(PropertyValueCallback out, char sep, void* context) {
    // Do nothing, don't call the callback
    (void)out;
    (void)sep;
    (void)context;
}

void furi_hal_power_debug_get(PropertyValueCallback out, void* context) {
    // Do nothing, don't call the callback
    (void)out;
    (void)context;
}
