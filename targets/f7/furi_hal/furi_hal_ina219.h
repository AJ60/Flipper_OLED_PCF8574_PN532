#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool furi_hal_ina219_init(void);
bool furi_hal_ina219_is_ready(void);
bool furi_hal_ina219_get_voltage_current(float* voltage_v, float* current_a);

#ifdef __cplusplus
}
#endif
