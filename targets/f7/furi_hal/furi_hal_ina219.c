#include "furi_hal_ina219.h"
#include "furi_hal_resources.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <stm32wbxx_ll_cortex.h>
#include <stdint.h>

#define TAG "FuriHalINA"

#define INA_I2C_ADDR_BASE 0x40

// Common & INA219 Registers
#define INA_REG_CONFIG        0x00
#define INA_REG_SHUNT_VOLTAGE 0x01
#define INA_REG_BUS_VOLTAGE   0x02
#define INA_REG_POWER         0x03
#define INA_REG_CURRENT       0x04
#define INA_REG_CALIBRATION   0x05

// INA226 Unique Identification Registers
#define INA226_REG_MANUFACTURER_ID 0xFE
#define INA226_REG_DIE_ID          0xFF

#define INA226_MANUFACTURER_ID_VAL 0x5449 // "TI"
#define INA226_DIE_ID_VAL          0x2260

#ifndef INA219_SHUNT_OHMS
#define INA219_SHUNT_OHMS 0.1f
#endif

static bool s_detected = false;
static bool s_is_ina226 = false;
static uint8_t s_address = INA_I2C_ADDR_BASE;

static float s_cached_voltage_v = 0.0f;
static float s_cached_current_a = 0.0f;
static uint32_t s_last_read_tick = 0;

#define INA_READ_PERIOD_MS 500

static bool ina_read_reg16(uint8_t reg, uint16_t* out) {
    const FuriHalI2cBusHandle* handle = &furi_hal_i2c_handle_power;
    uint8_t addr8 = (uint8_t)(s_address << 1);
    return furi_hal_i2c_read_reg_16(handle, addr8, reg, out, 20);
}

static bool ina_write_reg16(uint8_t reg, uint16_t val) {
    const FuriHalI2cBusHandle* handle = &furi_hal_i2c_handle_power;
    uint8_t addr8 = (uint8_t)(s_address << 1);
    return furi_hal_i2c_write_reg_16(handle, addr8, reg, val, 20);
}

bool furi_hal_ina219_init(void) {
    s_detected = false;
    s_is_ina226 = false;
    furi_delay_ms(200);

    const int max_attempts = 1;
    for(int attempt = 0; attempt < max_attempts && !s_detected; ++attempt) {
        if(attempt > 0) {
            FURI_LOG_I(TAG, "Retrying INA scan (%d/%d)", attempt + 1, max_attempts);
            furi_delay_ms(100);
        }

        uint8_t probe_addrs[] = {0x40, 0x41, 0x44, 0x45};
        for(size_t i = 0; i < sizeof(probe_addrs) / sizeof(probe_addrs[0]); i++) {
            s_address = probe_addrs[i];
            uint16_t cfg = 0;

            furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
            bool ready =
                furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_power, s_address << 1, 10);
            bool ok = false;

            if(ready) {
                ok = ina_read_reg16(INA_REG_CONFIG, &cfg);
            }

            if(ok) {
                s_detected = true;
                // Check if device is INA226 by reading Manufacturer ID (0xFE) and Die ID (0xFF)
                uint16_t mfg_id = 0, die_id = 0;
                bool is_226 = ina_read_reg16(INA226_REG_MANUFACTURER_ID, &mfg_id) &&
                              ina_read_reg16(INA226_REG_DIE_ID, &die_id);
                furi_hal_i2c_release(&furi_hal_i2c_handle_power);

                if(is_226 && mfg_id == INA226_MANUFACTURER_ID_VAL) {
                    s_is_ina226 = true;
                    // Calibrate INA226 for 0.1 Ohm shunt: CAL = 512 (0x0200)
                    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
                    ina_write_reg16(INA_REG_CALIBRATION, 0x0200);
                    furi_hal_i2c_release(&furi_hal_i2c_handle_power);
                    FURI_LOG_I(
                        TAG,
                        "Detected INA226 at 0x%02X (Die=0x%04X, MFG=0x%04X)",
                        s_address,
                        die_id,
                        mfg_id);
                } else {
                    s_is_ina226 = false;
                    FURI_LOG_I(TAG, "Detected INA219 at 0x%02X (CONFIG=0x%04X)", s_address, cfg);
                }
                break;
            } else {
                furi_hal_i2c_release(&furi_hal_i2c_handle_power);
            }
        }
    }

    if(!s_detected) {
        FURI_LOG_I(TAG, "INA219/INA226 power monitor not detected on I2C bus");
    }

    return s_detected;
}

bool furi_hal_ina219_is_ready(void) {
    return s_detected;
}

bool furi_hal_ina219_get_voltage_current(float* voltage_v, float* current_a) {
    if(!voltage_v || !current_a) return false;
    if(!s_detected) return false;

    uint32_t now = furi_get_tick();
    uint32_t period_ticks = furi_ms_to_ticks(INA_READ_PERIOD_MS);

    if(s_last_read_tick != 0 && (now - s_last_read_tick) < period_ticks) {
        *voltage_v = s_cached_voltage_v;
        *current_a = s_cached_current_a;
        return true;
    }

    uint16_t bus_raw = 0;
    uint16_t shunt_raw = 0;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    bool ok1 = ina_read_reg16(INA_REG_BUS_VOLTAGE, &bus_raw);
    bool ok2 = ina_read_reg16(INA_REG_SHUNT_VOLTAGE, &shunt_raw);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);

    if(!ok1 && !ok2) {
        if(s_last_read_tick != 0) {
            *voltage_v = s_cached_voltage_v;
            *current_a = s_cached_current_a;
            return true;
        }
        return false;
    }

    float voltage = s_cached_voltage_v;
    if(ok1) {
        if(s_is_ina226) {
            // INA226 Bus Voltage LSB = 1.25 mV (0.00125 V), full 16-bit register
            voltage = (float)bus_raw * 0.00125f;
        } else {
            // INA219 Bus Voltage LSB = 4.0 mV (0.004 V), bits 13..3
            uint16_t v = (uint16_t)(bus_raw >> 3);
            voltage = (float)v * 0.004f;
        }
    }

    float current = s_cached_current_a;
    if(ok2) {
        int16_t s = (int16_t)shunt_raw;
        if(s_is_ina226) {
            // INA226 Shunt Voltage LSB = 2.5 uV (2.5e-6 V)
            float shunt_v = (float)s * 2.5e-6f;
            current = -(shunt_v / INA219_SHUNT_OHMS);
        } else {
            // INA219 Shunt Voltage LSB = 10 uV (10e-6 V)
            float shunt_v = (float)s * 10e-6f;
            current = -(shunt_v / INA219_SHUNT_OHMS);
        }
    }

    s_cached_voltage_v = voltage;
    s_cached_current_a = current;
    s_last_read_tick = now;

    *voltage_v = s_cached_voltage_v;
    *current_a = s_cached_current_a;
    return true;
}

const char* furi_hal_ina219_get_model_name(void) {
    if(!s_detected) return "None";
    return s_is_ina226 ? "INA226" : "INA219";
}

#define INA226_REG_MASK_ENABLE 0x06
#define INA226_REG_ALERT_LIMIT 0x07

static GpioExtiCallback s_ina_alert_cb = NULL;
static void* s_ina_alert_ctx = NULL;

static void furi_hal_ina226_alert_isr(void* ctx) {
    UNUSED(ctx);
    if(s_ina_alert_cb) {
        s_ina_alert_cb(s_ina_alert_ctx);
    }
}

// The INA226 alert-limit registers are 16-bit (max 0xFFFF). A raw (uint16_t)
// cast of a larger value silently wraps: e.g. a 2.0 A limit on a 0.1 ohm shunt
// computes to 80000, which truncates to ~0.36 A. Clamp to the representable
// range instead.
static uint16_t ina226_clamp_alert_limit(float value_v, float lsb_v) {
    if(value_v < 0.0f) {
        value_v = 0.0f;
    }
    float max_value_v = 65535.0f * lsb_v;
    if(value_v > max_value_v) {
        value_v = max_value_v;
    }
    return (uint16_t)(value_v / lsb_v);
}

bool furi_hal_ina226_set_overcurrent_limit(float max_current_a) {
    if(!s_detected || !s_is_ina226) return false;

    // Convert max current in Amperes to INA226 Shunt Voltage register value.
    // Shunt Voltage LSB = 2.5 uV; V_shunt = I * R_shunt (R_shunt = 0.1 Ohm).
    // With a 0.1 Ohm shunt the 16-bit register can represent at most
    // 0xFFFF * 2.5e-6 / 0.1 = 1.638 A; larger requests are clamped.
    const float max_representable_a = (65535.0f * 2.5e-6f) / INA219_SHUNT_OHMS;
    if(max_current_a > max_representable_a) {
        FURI_LOG_W(
            TAG,
            "Overcurrent limit %.2f A exceeds INA226 range (max %.2f A), clamping",
            (double)max_current_a,
            (double)max_representable_a);
    }
    uint16_t limit_val = ina226_clamp_alert_limit(max_current_a * INA219_SHUNT_OHMS, 2.5e-6f);

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    // Write Shunt Over-Limit threshold to ALERT_LIMIT (0x07)
    bool ok1 = ina_write_reg16(INA226_REG_ALERT_LIMIT, limit_val);
    // Enable SOL (Shunt Voltage Over-Limit) bit in MASK_ENABLE (0x06) -> 0x8000
    bool ok2 = ina_write_reg16(INA226_REG_MASK_ENABLE, 0x8000);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);

    return ok1 && ok2;
}

bool furi_hal_ina226_configure_protection(float overcurrent_a, float undervoltage_v) {
    if(!s_detected || !s_is_ina226) return false;

    bool ok = true;
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);

    if(overcurrent_a > 0.0f) {
        // Set Shunt Over-Limit (SOL = 0x8000); clamped to the 16-bit register.
        uint16_t limit_val = ina226_clamp_alert_limit(overcurrent_a * INA219_SHUNT_OHMS, 2.5e-6f);
        ok = ok && ina_write_reg16(INA226_REG_ALERT_LIMIT, limit_val);
        ok = ok && ina_write_reg16(INA226_REG_MASK_ENABLE, 0x8000);
    } else if(undervoltage_v > 0.0f) {
        // Set Bus Under-Limit (BUL = 0x1000); bus-voltage LSB is 1.25 mV.
        uint16_t limit_val = ina226_clamp_alert_limit(undervoltage_v, 0.00125f);
        ok = ok && ina_write_reg16(INA226_REG_ALERT_LIMIT, limit_val);
        ok = ok && ina_write_reg16(INA226_REG_MASK_ENABLE, 0x1000);
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    return ok;
}

void furi_hal_ina226_enable_alert_interrupt(GpioExtiCallback cb, void* ctx) {
    s_ina_alert_cb = cb;
    s_ina_alert_ctx = ctx;

    if(!s_detected) return;

    furi_hal_gpio_init_ex(
        &gpio_ina_alert, GpioModeInterruptFall, GpioPullUp, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_add_int_callback(&gpio_ina_alert, furi_hal_ina226_alert_isr, NULL);
    furi_hal_gpio_enable_int_callback(&gpio_ina_alert);

    // gpio_ina_alert is PB1 = EXTI line 1. The furi_hal_gpio_* helpers only set
    // the EXTI IT bit; without NVIC enablement the EXTI1_IRQHandler never runs
    // and the emergency overcurrent callback never fires.
    NVIC_SetPriority(EXTI1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI1_IRQn);
}
