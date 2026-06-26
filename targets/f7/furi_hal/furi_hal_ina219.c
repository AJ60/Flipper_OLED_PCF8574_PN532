#include "furi_hal_ina219.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <stdint.h>

#define TAG "FuriHalINA219"

#define INA219_I2C_ADDR_BASE 0x40

#define INA219_REG_CONFIG 0x00
#define INA219_REG_SHUNT_VOLTAGE 0x01
#define INA219_REG_BUS_VOLTAGE 0x02
#define INA219_REG_POWER 0x03
#define INA219_REG_CURRENT 0x04
#define INA219_REG_CALIBRATION 0x05

#ifndef INA219_SHUNT_OHMS
#define INA219_SHUNT_OHMS 0.1f
#endif

static bool s_detected = false;
static uint8_t s_address = INA219_I2C_ADDR_BASE;

static float s_cached_voltage_v = 0.0f;
static float s_cached_current_a = 0.0f;
static uint32_t s_last_read_tick = 0;

#define INA219_READ_PERIOD_MS 500

static bool ina219_read_reg16(uint8_t reg, uint16_t* out) {
    const FuriHalI2cBusHandle* handle = &furi_hal_i2c_handle_power;
    uint8_t addr8 = (uint8_t)(s_address << 1);
    return furi_hal_i2c_read_reg_16(handle, addr8, reg, out, 20);
}

static bool ina219_write_reg16(uint8_t reg, uint16_t val) {
    const FuriHalI2cBusHandle* handle = &furi_hal_i2c_handle_power;
    uint8_t addr8 = (uint8_t)(s_address << 1);
    return furi_hal_i2c_write_reg_16(handle, addr8, reg, val, 20);
}

bool furi_hal_ina219_init(void) {
    s_detected = false;
    furi_delay_ms(200);

    const int max_attempts = 3;
    for(int attempt = 0; attempt < max_attempts && !s_detected; ++attempt) {
        if(attempt > 0) {
            FURI_LOG_I(TAG, "Retrying INA219 scan (%d/%d)", attempt + 1, max_attempts);
            furi_delay_ms(100);
        }

        for(uint8_t a = INA219_I2C_ADDR_BASE; a <= (INA219_I2C_ADDR_BASE | 0x0F); ++a) {
            s_address = a;
            uint16_t cfg = 0;

            furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
            bool ready = furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_power, s_address << 1, 100);
            bool ok = false;

            if(ready) {
                ok = ina219_read_reg16(INA219_REG_CONFIG, &cfg);
            }

            furi_hal_i2c_release(&furi_hal_i2c_handle_power);

            if(ok) {
                s_detected = true;
                FURI_LOG_I(TAG, "Detected INA219 at 0x%02X (CONFIG=0x%04X)", s_address, cfg);
                break;
            }
        }
    }

    if(!s_detected) {
        s_address = INA219_I2C_ADDR_BASE;
        uint16_t cfg = 0;

        furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
        (void)ina219_write_reg16(INA219_REG_CALIBRATION, 4096);
        bool ok = ina219_read_reg16(INA219_REG_CONFIG, &cfg);
        furi_hal_i2c_release(&furi_hal_i2c_handle_power);

        if(ok) {
            s_detected = true;
            FURI_LOG_I(TAG, "Detected INA219 at default 0x%02X after calibration write", s_address);
        }
    }

    if(!s_detected) {
        FURI_LOG_I(TAG, "INA219 not detected on I2C bus");
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
    uint32_t period_ticks = furi_ms_to_ticks(INA219_READ_PERIOD_MS);

    if(s_last_read_tick != 0 && (now - s_last_read_tick) < period_ticks) {
        *voltage_v = s_cached_voltage_v;
        *current_a = s_cached_current_a;
        return true;
    }

    uint16_t bus_raw = 0;
    uint16_t shunt_raw = 0;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    bool ok1 = ina219_read_reg16(INA219_REG_BUS_VOLTAGE, &bus_raw);
    bool ok2 = ina219_read_reg16(INA219_REG_SHUNT_VOLTAGE, &shunt_raw);
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
        uint16_t v = (uint16_t)(bus_raw >> 3);
        voltage = (float)v * 0.004f;
    }

    float current = s_cached_current_a;
    if(ok2) {
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
