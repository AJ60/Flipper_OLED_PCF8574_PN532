#include <furi_hal_vibro.h>
#include <furi_hal_gpio.h>
#include <core/common_defines.h>
#include "furi_hal_pcf8574.h"

#define TAG "FuriHalVibro"

void furi_hal_vibro_init(void) {
    furi_hal_pcf8574_init();
    furi_hal_pcf8574_write_pin(6, false);
    FURI_LOG_I(TAG, "Vibro initialized on PCF8574 Pin P6 (active-HIGH)");
}

void furi_hal_vibro_on(bool value) {
    furi_hal_pcf8574_write_pin(6, value);
}
