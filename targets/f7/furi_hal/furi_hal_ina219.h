#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <furi_hal_gpio.h>

bool furi_hal_ina219_init(void);
bool furi_hal_ina219_is_ready(void);
bool furi_hal_ina219_get_voltage_current(float* voltage_v, float* current_a);
const char* furi_hal_ina219_get_model_name(void);
bool furi_hal_ina226_set_overcurrent_limit(float max_current_a);
bool furi_hal_ina226_configure_protection(float overcurrent_a, float undervoltage_v);
void furi_hal_ina226_enable_alert_interrupt(GpioExtiCallback cb, void* ctx);

#ifdef __cplusplus
}
#endif
