#include <furi_hal_power.h>
// Keep necessary includes for types used in function signatures
#include <furi.h> // For FURI_NORETURN, basic types, PropertyValueCallback
#include <toolbox/property.h>
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
#include <furi_hal_cortex.h>
#include <stm32wbxx_ll_gpio.h>
#include <hsem_map.h>
#include <bq27220.h>
#include <bq27220_data_memory.h>
#include <bq25896.h>
#include <furi_hal_subghz.h>
#include <furi_hal_nfc.h>
#include <furi_hal_speaker.h>
#include <furi_hal_light.h>
#include <furi_hal_pcf8574.h>
#include <furi_hal_i2c.h>
#include <stm32wbxx_ll_exti.h>

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

void furi_hal_power_init(void) {
#ifdef USE_INA219
    FURI_LOG_I(TAG, "Initializing INA219 power sensor");
    furi_hal_ina219_init();
    FURI_LOG_I(TAG, "INA219 initialization complete");
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
    // If USB is connected and powered, never request shutdown
    if(furi_hal_power_get_usb_voltage() >= 4.0f) {
        return false;
    }

    // Never auto-request shutdown during early boot (first 30 seconds) to prevent bootloops
    if(furi_get_tick() < 30000) {
        return false;
    }

    // If measured battery voltage is below 2.0V, no battery is present (running on external power)
    float vbat = furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge);
    if(vbat < 2.0f) {
        return false;
    }

#ifdef USE_INA219
    if(furi_hal_ina219_is_ready()) {
        float v = 0.0f, i = 0.0f;
        if(furi_hal_ina219_get_voltage_current(&v, &i)) {
            static uint8_t overcurrent_count = 0;
            static uint8_t undervoltage_count = 0;
            static uint8_t low_battery_count = 0;

            // Overcurrent protection: if current magnitude exceeds 2.2A persistently
            float abs_i = (i < 0.0f) ? -i : i;
            if(abs_i >= 2.2f) {
                if(++overcurrent_count >= 3) {
                    FURI_LOG_E(TAG, "Emergency: Overcurrent detected (%.2f A)!", (double)abs_i);
                    return true;
                }
            } else {
                overcurrent_count = 0;
            }

            // IR-compensated open-circuit voltage
            float v_oc = v - (i * R_INTERNAL);

            // Critical undervoltage emergency cutoff (3.00V) -> protect cell from deep discharge damage
            if(v_oc <= 3.00f || v <= 2.95f) {
                if(++undervoltage_count >= 3) {
                    FURI_LOG_E(TAG, "Emergency: Battery critical undervoltage (%.2f V)!", (double)v_oc);
                    return true;
                }
            } else {
                undervoltage_count = 0;
            }

            // Normal low-battery cutoff (3.15V) -> triggers graceful OS poweroff
            if(v_oc <= 3.15f) {
                if(++low_battery_count >= 5) {
                    FURI_LOG_W(TAG, "Battery low (%.2f V), shutdown requested", (double)v_oc);
                    return true;
                }
            } else {
                low_battery_count = 0;
            }
        }
    }
#endif

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
    // Bug #8 fix: was a no-op, causing the MCU to spin at full speed during idle,
    // draining the battery 3-5x faster. WFI halts the core until the next IRQ,
    // which is the standard Cortex-M low-power idle technique.
    if(furi_hal_power.insomnia == 0) {
        __WFI();
    }
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

    // If voltage is under 2.0V, no battery is attached (bare board or USB external power)
    if(vbat < 2.0f) {
        return 100;
    }

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
#ifdef USE_INA219
    if(furi_hal_ina219_is_ready()) {
        float v = 0.0f, i = 0.0f;
        if(furi_hal_ina219_get_voltage_current(&v, &i)) {
            // Charging done when USB is connected, battery voltage is full (>= 4.18V),
            // and charge current has tapered down (< 25mA)
            if(furi_hal_power_get_usb_voltage() >= 4.0f && v >= 4.18f && i < 0.025f) {
                return true;
            }
        }
    }
#endif
    return false;
}

static volatile bool furi_hal_power_shutdown_active = false;
extern volatile bool display_is_sleeping;

bool furi_hal_power_is_shutdown(void) {
    return furi_hal_power_shutdown_active;
}

void furi_hal_power_shutdown(void) {
    FURI_LOG_I(TAG, "Entering low-power sleep mode...");
    furi_delay_ms(50);

    furi_hal_power_shutdown_active = true;
    display_is_sleeping = true;

    // 1. Put external OLED display into sleep mode (SSD1306 Display OFF 0xAE)
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    uint8_t oled_off_cmd[] = {0x00, 0xAE};
    furi_hal_i2c_tx(&furi_hal_i2c_handle_power, (0x3C << 1), oled_off_cmd, sizeof(oled_off_cmd), 50);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);

    // 2. Put Sub-GHz CC1101 radio to sleep
    furi_hal_subghz_sleep();

    // 3. Put NFC PN532 module to low power mode
    furi_hal_nfc_low_power_mode_start();

    // 4. Turn off speaker, vibration, and LEDs
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
    }
    furi_hal_vibro_on(false);
    furi_hal_pcf8574_led_off();

    // 5. Cleanly stop Bluetooth advertising
    if(furi_hal_bt_is_active()) {
        furi_hal_bt_stop_advertising();
    }

    // 6. Ensure PCF8574 input pins (P0..P5) are configured as inputs
    furi_hal_pcf8574_configure_interrupts(0x3F);

    // Wait until all buttons (P0..P5) are physically released before sleeping
    for(int wait_release = 0; wait_release < 40; wait_release++) {
        uint8_t pv = 0xFF;
        if(furi_hal_pcf8574_read_port(&pv)) {
            if((pv & 0x3F) == 0x3F) {
                break;
            }
        }
        furi_delay_ms(25);
    }

    // Clear fault data in RTC backup register so subsequent boots are clean
    furi_hal_rtc_set_fault_data(0);

    // Sleep loop: FreeRTOS remains running, tickless idle puts CPU to sleep (__WFI).
    // Device stays dark and quiet until the OK button (P4) is held continuously for ~1.2s.
    while(1) {
        furi_delay_ms(100);

        uint8_t port_val = 0xFF;
        if(!furi_hal_pcf8574_read_port(&port_val)) {
            continue;
        }

        // PCF8574: pin P4 is bit 4 (0x10). Active-LOW (0 when pressed).
        if((port_val & (1 << 4)) == 0) {
            // OK button is pressed! Check if held continuously for ~1.2s (24 * 50ms)
            bool held = true;
            for(int i = 0; i < 24; i++) {
                furi_delay_ms(50);
                port_val = 0xFF;
                if(!furi_hal_pcf8574_read_port(&port_val) || ((port_val & (1 << 4)) != 0)) {
                    held = false;
                    break;
                }
            }

            if(held) {
                // Long-press of OK button confirmed! Wake up device!
                // Brief vibration pulse (50ms) to confirm power-on
                furi_hal_vibro_on(true);
                furi_delay_ms(50);
                furi_hal_vibro_on(false);

                // Clear fault data in RTC backup register
                furi_hal_rtc_set_fault_data(0);

                // Clean hardware reset: boots all subsystems freshly!
                furi_hal_power_reset();
            }
        }
    }
}

void furi_hal_power_off(void) {
    // Do nothing
    furi_hal_power_shutdown();
}

FURI_NORETURN void furi_hal_power_reset(void) {
    NVIC_SystemReset();
}

static bool furi_hal_power_otg_enabled = false;

bool furi_hal_power_enable_otg(void) {
    furi_hal_power_otg_enabled = true;
    return true;
}

void furi_hal_power_disable_otg(void) {
    furi_hal_power_otg_enabled = false;
}

bool furi_hal_power_is_otg_enabled(void) {
    return furi_hal_power_otg_enabled;
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
    // Bug #9 fix: was always returning BATTERY_CAPACITY (constant 1200 mAh),
    // never decreasing. Calculate from current SOC percentage instead.
    uint8_t pct = furi_hal_power_get_pct();
    return (uint32_t)(((uint32_t)BATTERY_CAPACITY * pct) / 100U);
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
            FURI_LOG_T(TAG, "INA219 voltage=%.3f V, current=%.3f A", (double)v, (double)i);
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
    // If VDDUSB is enabled, the device is powered by USB
    if(LL_PWR_IsEnabledVddUSB()) {
        return 5.0f;
    }
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
    furi_assert(out);

    FuriString* value = furi_string_alloc();
    FuriString* key = furi_string_alloc();

    PropertyValueContext property_context = {
        .key = key, .value = value, .out = out, .sep = sep, .last = false, .context = context};

    // Power Info version
    if(sep == '.') {
        property_value_out(&property_context, NULL, 2, "format", "major", "2");
        property_value_out(&property_context, NULL, 2, "format", "minor", "0");
    } else {
        property_value_out(&property_context, NULL, 3, "power", "info", "major", "1");
        property_value_out(&property_context, NULL, 3, "power", "info", "minor", "1");
    }

    uint16_t charge = furi_hal_power_get_pct();
    property_value_out(&property_context, "%u", 2, "charge", "level", charge);

    const char* charge_state;
    if(furi_hal_power_is_charging()) {
        if((charge < 100) && (!furi_hal_power_is_charging_done())) {
            charge_state = "charging";
        } else {
            charge_state = "charged";
        }
    } else {
        charge_state = "discharging";
    }

    property_value_out(&property_context, NULL, 2, "charge", "state", charge_state);
    uint16_t charge_voltage_limit =
        (uint16_t)(furi_hal_power_get_battery_charge_voltage_limit() * 1000.f);
    property_value_out(
        &property_context, "%u", 3, "charge", "voltage", "limit", charge_voltage_limit);
    uint16_t voltage =
        (uint16_t)(furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge) * 1000.f);
    property_value_out(&property_context, "%u", 2, "battery", "voltage", voltage);
    int16_t current =
        (int16_t)(furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge) * 1000.f);
    property_value_out(&property_context, "%d", 2, "battery", "current", current);
    int16_t temperature = (int16_t)furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
    property_value_out(&property_context, "%d", 2, "battery", "temp", temperature);
    property_value_out(
        &property_context, "%u", 2, "battery", "health", furi_hal_power_get_bat_health_pct());
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "remain",
        furi_hal_power_get_battery_remaining_capacity());
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "full",
        furi_hal_power_get_battery_full_capacity());
    property_context.last = true;
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "design",
        furi_hal_power_get_battery_design_capacity());

    furi_string_free(key);
    furi_string_free(value);
}

void furi_hal_power_debug_get(PropertyValueCallback out, void* context) {
    furi_assert(out);

    FuriString* value = furi_string_alloc();
    FuriString* key = furi_string_alloc();

    PropertyValueContext property_context = {
        .key = key, .value = value, .out = out, .sep = '.', .last = false, .context = context};

    // Power Debug version
    property_value_out(&property_context, NULL, 2, "format", "major", "1");
    property_value_out(&property_context, NULL, 2, "format", "minor", "0");

#ifdef USE_INA219
    if(furi_hal_ina219_is_ready()) {
        float v = 0.0f, i = 0.0f;
        if(furi_hal_ina219_get_voltage_current(&v, &i)) {
            property_value_out(&property_context, "%d", 2, "ina219", "voltage", (int)(v * 1000.f));
            property_value_out(&property_context, "%d", 2, "ina219", "current", (int)(i * 1000.f));
        }
    }
#endif

    uint16_t voltage =
        (uint16_t)(furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge) * 1000.f);
    property_value_out(&property_context, "%u", 2, "battery", "voltage", voltage);
    int16_t current =
        (int16_t)(furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge) * 1000.f);
    property_value_out(&property_context, "%d", 2, "battery", "current", current);

    property_context.last = true;
    property_value_out(&property_context, "%u", 2, "battery", "level", furi_hal_power_get_pct());

    furi_string_free(key);
    furi_string_free(value);
}
