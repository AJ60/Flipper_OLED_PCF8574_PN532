#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "furi_hal_gpio.h"
#include "furi_hal_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// Set which I2C bus to use. Call BEFORE init(). If not called, the power bus
// (I2C1) is used.
void furi_hal_pcf8574_set_i2c_bus(const FuriHalI2cBusHandle* bus_handle);

// Initialize PCF8574. Idempotent: subsequent calls are no-ops that return true.
// Detection uses a functional write/read-back probe so other devices on the bus
// (INA219 at 0x40, OLED at 0x3C) cannot be mistaken for the expander.
bool furi_hal_pcf8574_init(void);
bool furi_hal_pcf8574_init_ex(uint8_t i2c_addr);

// Read 8-bit port. On success also syncs the internal shadow state used for
// read-modify-write updates (see write_pin). Input pins (declared via
// configure_interrupts) are never adopted into the shadow — their read levels
// reflect external logic such as pressed buttons.
bool furi_hal_pcf8574_read_port(uint8_t* port_state);

// Write 8-bit port. Pins declared as inputs (configure_interrupts) are always
// forced high on the wire; writing them low is ignored so a button can never
// be latched. On success updates the internal shadow state.
bool furi_hal_pcf8574_write_port(uint8_t port_state);

// Write single PCF8574 pin (0-7). Pins declared as inputs are forced high and
// cannot be driven low.
bool furi_hal_pcf8574_write_pin(uint8_t pin, bool value);

// Attach callback for INT pin
void furi_hal_pcf8574_attach_int(GpioExtiCallback cb, void* ctx);

// Configure interrupts (for PCF8574 this just sets the pins to 1 so they can act as inputs)
bool furi_hal_pcf8574_configure_interrupts(uint8_t gpios_to_input_mask);
bool furi_hal_pcf8574_check_and_restore(uint8_t expected_mask);

// Stubs for LED control so we don't break furi_hal_light
bool furi_hal_pcf8574_led_init(void);
bool furi_hal_pcf8574_led_set_red(bool on);
bool furi_hal_pcf8574_led_set_green(bool on);
bool furi_hal_pcf8574_led_set_blue(bool on);
bool furi_hal_pcf8574_led_set_color(bool red, bool green, bool blue);
bool furi_hal_pcf8574_led_set(uint8_t red, uint8_t green, uint8_t blue);
bool furi_hal_pcf8574_led_off(void);

#ifdef __cplusplus
}
#endif
