#include "furi_hal_ina219.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <stdint.h>

#define TAG "FuriHalINA"

#define INA_I2C_ADDR_BASE 0x40

// INA219 Registers
#define INA_REG_CONFIG        0x00
#define INA_REG_SHUNT_VOLTAGE 0x01
#define INA_REG_BUS_VOLTAGE   0x02
#define INA_REG_POWER         0x03
#define INA_REG_CURRENT       0x04
#define INA_REG_CALIBRATION   0x05

#ifndef INA219_SHUNT_OHMS
#define INA219_SHUNT_OHMS 0.1f
#endif

static bool s_detected = false;
static uint8_t s_address = INA_I2C_ADDR_BASE;

static float s_cached_voltage_v = 0.0f;
static float s_cached_current_a = 0.0f;
static uint32_t s_last_read_tick = 0;

#define INA_READ_PERIOD_MS 5000

static bool ina_read_reg16(uint8_t reg, uint16_t* out) {
    const FuriHalI2cBusHandle* handle = &furi_hal_i2c_handle_power;
    uint8_t addr8 = (uint8_t)(s_address << 1);
    return furi_hal_i2c_read_reg_16(handle, addr8, reg, out, 5);
}


bool furi_hal_ina219_init(void) {
    s_detected = false;

    // Fast probe: INA219 modules default to 0x40. Check 0x40 first with 2ms timeout,
    // falling back to alternate addresses in a single fast pass without sleep delays.
    const uint8_t probe_addrs[] = {0x40, 0x41, 0x44, 0x45};
    for(size_t i = 0; i < sizeof(probe_addrs) / sizeof(probe_addrs[0]); i++) {
        s_address = probe_addrs[i];
        uint8_t addr8 = (uint8_t)(s_address << 1);
        uint16_t cfg = 0;

        furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
        bool ready = furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_power, addr8, 2);
        bool ok = false;
        if(ready) {
            ok = furi_hal_i2c_read_reg_16(
                &furi_hal_i2c_handle_power, addr8, INA_REG_CONFIG, &cfg, 5);
        }

        if(ok) {
            s_detected = true;
            // Configure INA219: 32V range, +/-320mV PG, 12-bit continuous (0x399F)
            // Calibrate for 0.1 Ohm shunt (CAL = 0x1000 = 4096)
            furi_hal_i2c_write_reg_16(
                &furi_hal_i2c_handle_power, addr8, INA_REG_CONFIG, 0x399F, 5);
            furi_hal_i2c_write_reg_16(
                &furi_hal_i2c_handle_power, addr8, INA_REG_CALIBRATION, 0x1000, 5);
            furi_hal_i2c_release(&furi_hal_i2c_handle_power);
            FURI_LOG_I(TAG, "Detected INA219 at 0x%02X (CONFIG=0x%04X)", s_address, cfg);
            break;
        } else {
            furi_hal_i2c_release(&furi_hal_i2c_handle_power);
        }
    }

    if(!s_detected) {
        FURI_LOG_I(TAG, "INA219 power monitor not detected on I2C bus");
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
        // INA219 Bus Voltage LSB = 4.0 mV (0.004 V), bits 13..3
        uint16_t v = (uint16_t)(bus_raw >> 3);
        voltage = (float)v * 0.004f;
    }

    float current = s_cached_current_a;
    if(ok2) {
        // INA219 Shunt Voltage LSB = 10 uV (10e-6 V)
        int16_t s = (int16_t)shunt_raw;
        float shunt_v = (float)s * 10e-6f;
        current = -(shunt_v / INA219_SHUNT_OHMS);
    }

    s_cached_voltage_v = voltage;
    s_cached_current_a = current;
    s_last_read_tick = now;

    *voltage_v = s_cached_voltage_v;
    *current_a = s_cached_current_a;
    return true;
}

const char* furi_hal_ina219_get_model_name(void) {
    return s_detected ? "INA219" : "None";
}
