#include <furi_hal_vibro.h>
#include <furi_hal_gpio.h>
#include <furi_hal_pcf8574.h>

#define TAG "FuriHalVibro"

void furi_hal_vibro_init(void) {
    // Use PCF8574 pin 6 for the vibro motor
    const uint8_t vibro_pin = 6;
    if(!furi_hal_pcf8574_init()) {
        FURI_LOG_E(TAG, "PCF8574 init failed");
    } else {
        // Configure pin as output and clear it
        // PCF8574 has no direction register, so we just write the pin
        if(!furi_hal_pcf8574_write_pin(vibro_pin, false)) {
            FURI_LOG_E(TAG, "Failed to clear vibro pin");
        }
        FURI_LOG_I(TAG, "Vibro bound to PCF8574 pin 6");
    }
}

void furi_hal_vibro_on(bool value) {
    const uint8_t vibro_pin = 6;
    if(!furi_hal_pcf8574_write_pin(vibro_pin, value)) {
        FURI_LOG_E(TAG, "Failed to set vibro pin");
    }
}
