#include "furi_hal_mcp23017.h"
#include "furi_hal_i2c.h"
#include "furi_hal_gpio.h"
#include "furi_hal_resources.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <furi.h>

#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPIOA  0x12
#define MCP_GPIOB  0x13

#define MCP_GPPUA   0x0C
#define MCP_GPPUB   0x0D

#define MCP_GPINTENA 0x04
#define MCP_GPINTENB 0x05
#define MCP_DEFVALA 0x06
#define MCP_DEFVALB 0x07
#define MCP_INTCONA 0x08
#define MCP_INTCONB 0x09
#define MCP_IOCON 0x0A

#define TAG "FuriHalMCP23017"

// MCP23017: 0x20 -> 0x40
// OLED:     0x3C -> 0x78
// INA219:   0x40 -> 0x80

static uint8_t mcp_addr = 0x20; // default 7-bit
static uint8_t mcp_i2c_addr(void) {
    return (uint8_t)(mcp_addr << 1);
}

// If true, the driver should use the 8-bit address form (addr<<1) when talking
// to the device. Some environments/devices require the 8-bit form for low-level
// register ops. This is discovered during probe and then cached.

// I2C bus handle - configurable for either power or external bus
// Default to power bus (I2C1) as it's initialized earlier in boot
// Call furi_hal_mcp23017_set_i2c_bus() to switch to external (I2C3)
static const FuriHalI2cBusHandle* mcp_i2c_handle = &furi_hal_i2c_handle_power;
static GpioExtiCallback exti_cb = NULL;
static void* exti_ctx = NULL;
static bool mcp_is_present = false;

static bool mcp_read_reg(uint8_t reg, uint8_t* val);
static bool mcp_write_reg_locked(uint8_t reg, uint8_t val);
static bool mcp_read_reg_locked(uint8_t reg, uint8_t* val);

// Set which I2C bus to use (power or external). Call this before init().
void furi_hal_mcp23017_set_i2c_bus(const FuriHalI2cBusHandle* bus_handle) {
    furi_check(bus_handle != NULL);
    mcp_i2c_handle = bus_handle;
    FURI_LOG_I(TAG, "MCP23017 I2C bus set");
}

// Internal implementation that accepts explicit I2C address
bool furi_hal_mcp23017_init_ex(uint8_t i2c_addr) {
    mcp_is_present = false;

    // Wait for the power rail and chip to fully stabilize
    furi_delay_ms(100);

    FURI_LOG_I(
        TAG,
        "Initializing MCP23017 on I2C bus");

    furi_hal_i2c_acquire(mcp_i2c_handle);

    bool detected = false;
    uint8_t detected_addr = 0;

    // Probe the requested address first
    uint8_t test_addr = i2c_addr;
    if(furi_hal_i2c_is_device_ready(mcp_i2c_handle, (uint8_t)(test_addr << 1), 200)) {
        detected = true;
        detected_addr = test_addr;
    } else {
        uint8_t probe = 0;
        if(furi_hal_i2c_read_reg_8(mcp_i2c_handle, (uint8_t)(test_addr << 1), MCP_IOCON, &probe, 200)) {
            detected = true;
            detected_addr = test_addr;
        }
    }

    // If not detected, fallback to testing the entire 0x20 - 0x27 range
    if(!detected) {
        uint8_t probe_addrs[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27};
        for(size_t i = 0; i < sizeof(probe_addrs) / sizeof(probe_addrs[0]); i++) {
            test_addr = probe_addrs[i];
            if(test_addr == i2c_addr) continue; // already tested

            if(furi_hal_i2c_is_device_ready(mcp_i2c_handle, (uint8_t)(test_addr << 1), 200)) {
                detected = true;
                detected_addr = test_addr;
                break;
            } else {
                uint8_t probe = 0;
                if(furi_hal_i2c_read_reg_8(mcp_i2c_handle, (uint8_t)(test_addr << 1), MCP_IOCON, &probe, 200)) {
                    detected = true;
                    detected_addr = test_addr;
                    break;
                }
            }
        }
    }

    if(!detected) {
        furi_hal_i2c_release(mcp_i2c_handle);

        FURI_LOG_E(
            TAG,
            "MCP23017 not detected at any address");

        return false;
    }

    mcp_addr = detected_addr;
    FURI_LOG_I(TAG, "MCP23017 detected at address 0x%02X", mcp_addr);

    bool io_ok = false;

    for(int attempt = 0; attempt < 3 && !io_ok; ++attempt) {
        if(mcp_write_reg_locked(MCP_IOCON, 0x44)) {
            io_ok = true;
            break;
        }

        furi_delay_ms(10);
    }

    furi_hal_i2c_release(mcp_i2c_handle);

    if(!io_ok) {
        FURI_LOG_E(TAG, "Failed to write IOCON");
        return false;
    }

    mcp_is_present = true;
    FURI_LOG_I(TAG, "MCP23017 initialized");

    return true;
}

// Public no-argument wrapper kept for API compatibility. Calls the
// explicit-address implementation with the default address 0x20.
bool furi_hal_mcp23017_init(void) {
    return furi_hal_mcp23017_init_ex(mcp_addr);
}

static bool mcp_read_reg(uint8_t reg, uint8_t* val) {
    bool ret;

    furi_hal_i2c_acquire(mcp_i2c_handle);

    ret = furi_hal_i2c_read_reg_8(
        mcp_i2c_handle,
        mcp_i2c_addr(),
        reg,
        val,
        200);

    furi_hal_i2c_release(mcp_i2c_handle);

    return ret;
}

static bool mcp_write_reg_locked(uint8_t reg, uint8_t val) {
    return furi_hal_i2c_write_reg_8(
        mcp_i2c_handle,
        mcp_i2c_addr(),
        reg,
        val,
        200);
}

// Read a register under an already acquired bus lock.
static bool mcp_read_reg_locked(uint8_t reg, uint8_t* val) {
    return furi_hal_i2c_read_reg_8(
        mcp_i2c_handle,
        mcp_i2c_addr(),
        reg,
        val,
        200);
}

bool furi_hal_mcp23017_read_gpio(uint16_t* gpio_state) {
    furi_check(gpio_state);

    uint8_t a = 0, b = 0;
    bool ok = true;

    // Read both bytes while holding the bus once.
    furi_hal_i2c_acquire(mcp_i2c_handle);
    ok = mcp_read_reg_locked(MCP_GPIOA, &a) &&
         mcp_read_reg_locked(MCP_GPIOB, &b);
    furi_hal_i2c_release(mcp_i2c_handle);

    if(!ok) return false;

    *gpio_state = (uint16_t)a | ((uint16_t)b << 8);
    return true;
}

bool furi_hal_mcp23017_read_port(uint8_t port_idx, uint8_t* port_state) {
    furi_check(port_state);
    if(port_idx > 1) return false;
    return mcp_read_reg(port_idx == 0 ? MCP_GPIOA : MCP_GPIOB, port_state);
}


bool furi_hal_mcp23017_configure_interrupts(uint16_t gpios_to_input_mask) {
    FURI_LOG_I(TAG, "Configuring MCP23017 interrupts with mask 0x%04X", gpios_to_input_mask);

    // Configure direction: 1=input, 0=output
    uint8_t mask_a = (uint8_t)(gpios_to_input_mask & 0xFF);
    uint8_t mask_b = (uint8_t)((gpios_to_input_mask >> 8) & 0xFF);

    bool ok = true;

    // Perform all writes under one bus lock.
    furi_hal_i2c_acquire(mcp_i2c_handle);

    // Enable MIRROR=1 (INTA/INTB connected) and ODR=1 (Open-Drain)
    ok = ok && mcp_write_reg_locked(MCP_IOCON, 0x44);
    ok = ok && mcp_write_reg_locked(MCP_IODIRA, mask_a);
    ok = ok && mcp_write_reg_locked(MCP_IODIRB, mask_b);
    ok = ok && mcp_write_reg_locked(MCP_GPINTENA, mask_a);
    ok = ok && mcp_write_reg_locked(MCP_GPINTENB, mask_b);

    // Enable internal pull-ups for active-low buttons.
    ok = ok && mcp_write_reg_locked(MCP_GPPUA, mask_a);
    ok = ok && mcp_write_reg_locked(MCP_GPPUB, mask_b);

    // Compare against previous pin state.
    ok = ok && mcp_write_reg_locked(MCP_INTCONA, 0x00);
    ok = ok && mcp_write_reg_locked(MCP_INTCONB, 0x00);

    furi_hal_i2c_release(mcp_i2c_handle);

    if(!ok) {
        FURI_LOG_E(TAG, "Failed to configure MCP23017 interrupts");
        return false;
    }

    // Read back a few registers for diagnostics.
    uint8_t ra = 0, rb = 0, ga = 0, gb = 0, pu_a = 0, pu_b = 0, iocon = 0;
    mcp_read_reg(MCP_IODIRA, &ra);
    mcp_read_reg(MCP_IODIRB, &rb);
    mcp_read_reg(MCP_GPINTENA, &ga);
    mcp_read_reg(MCP_GPINTENB, &gb);
    mcp_read_reg(MCP_GPPUA, &pu_a);
    mcp_read_reg(MCP_GPPUB, &pu_b);
    mcp_read_reg(MCP_IOCON, &iocon);

    FURI_LOG_I(
        TAG,
        "RegDump IODIR A:0x%02X B:0x%02X GPINTENA:0x%02X GPINTENB:0x%02X GPPUA:0x%02X GPPUB:0x%02X IOCON:0x%02X",
        ra, rb, ga, gb, pu_a, pu_b, iocon);

    FURI_LOG_I(TAG, "MCP23017 interrupts configured");
    return true;
}

// Verify the MCP23017 still holds its expected configuration. After a silent
// brown-out/reset the chip reverts IODIR to all-inputs and clears IOCON, which
// silently kills button interrupts. We detect the mismatch and re-apply config.
bool furi_hal_mcp23017_check_and_restore(uint16_t expected_mask) {
    uint8_t mask_a = (uint8_t)(expected_mask & 0xFF);

    // Cheap health probe: a single register read. IOCON reverts to 0x00 after a
    // silent reset, so it is a reliable, low-cost canary. We deliberately avoid
    // reading 7+ registers on every idle tick because this shares the power I2C
    // bus with battery monitoring and would starve the input thread under load.
    uint8_t iocon = 0;
    uint8_t iodira = 0;
    bool ok;

    furi_hal_i2c_acquire(mcp_i2c_handle);
    ok = mcp_read_reg_locked(MCP_IOCON, &iocon);
    if(ok) ok = mcp_read_reg_locked(MCP_IODIRA, &iodira);
    furi_hal_i2c_release(mcp_i2c_handle);

    if(!ok) {
        // I2C probe failed — the MCP23017 may have been disturbed by RF interference
        // from NFC field activity. Wait briefly for the bus to stabilize before re-init.
        FURI_LOG_D(TAG, "check_and_restore: I2C probe failed, attempting re-init");
        furi_delay_ms(50);
        if(furi_hal_mcp23017_init()) {
            return furi_hal_mcp23017_configure_interrupts(expected_mask);
        }
        return false;
    }

    // Healthy if IOCON kept its value and the input pins are still inputs.
    if(iocon == 0x44 && (iodira & mask_a) == mask_a) {
        return true;
    }

    FURI_LOG_W(
        TAG,
        "MCP config lost (IOCON:0x%02X IODIRA:0x%02X), restoring",
        iocon,
        iodira);

    furi_hal_i2c_acquire(mcp_i2c_handle);
    mcp_write_reg_locked(MCP_IOCON, 0x44);
    furi_hal_i2c_release(mcp_i2c_handle);

    return furi_hal_mcp23017_configure_interrupts(expected_mask);
}

static void furi_hal_mcp23017_gpio_isr(void* ctx) {
    UNUSED(ctx);
    if(exti_cb) exti_cb(exti_ctx);
}

void furi_hal_mcp23017_attach_int(GpioExtiCallback cb, void* ctx) {
    exti_cb = cb;
    exti_ctx = ctx;

    if(!mcp_is_present) {
        FURI_LOG_I(TAG, "MCP23017 not present: skipping PB0 EXTI interrupt initialization");
        return;
    }

    // Clear any pending interrupt on MCP chip before enabling EXTI on MCU
    uint16_t dummy = 0;
    furi_hal_mcp23017_read_gpio(&dummy);

    FURI_LOG_I(TAG, "MCP23017 present: enabling PB0 EXTI interrupt");
    furi_hal_gpio_init_ex(
        &gpio_mcp_int,
        GpioModeInterruptFall,
        GpioPullUp,
        GpioSpeedLow,
        GpioAltFnUnused);
    furi_hal_gpio_add_int_callback(&gpio_mcp_int, furi_hal_mcp23017_gpio_isr, NULL);
    furi_hal_gpio_enable_int_callback(&gpio_mcp_int);
}

// This function should be called by the board-specific EXTI ISR when the INT pin fires.
void furi_hal_mcp23017_handle_int(void) {
    if(exti_cb) exti_cb(exti_ctx);
}

// Write full GPIO state: lower byte = GPIOA, upper byte = GPIOB
bool furi_hal_mcp23017_write_gpio(uint16_t gpio_state) {
    uint8_t a = (uint8_t)(gpio_state & 0xFF);
    uint8_t b = (uint8_t)((gpio_state >> 8) & 0xFF);

    // Write GPIOA then GPIOB.
    bool ok = true;
    furi_hal_i2c_acquire(mcp_i2c_handle);
    ok = ok && mcp_write_reg_locked(MCP_GPIOA, a);
    ok = ok && mcp_write_reg_locked(MCP_GPIOB, b);
    furi_hal_i2c_release(mcp_i2c_handle);

    return ok;
}

// Write a single pin (0-15). Pins 0-7 -> A; 8-15 -> B
bool furi_hal_mcp23017_write_pin(uint8_t pin, bool value) {
    if(pin > 15) return false;

    uint8_t reg = (pin < 8) ? MCP_GPIOA : MCP_GPIOB;
    uint8_t bit = (uint8_t)(1u << (pin & 0x7));
    uint8_t cur = 0;
    bool ok;

    furi_hal_i2c_acquire(mcp_i2c_handle);
    ok = mcp_read_reg_locked(reg, &cur);
    if(ok) {
        if(value) cur |= bit;
        else cur &= (uint8_t)~bit;
        ok = mcp_write_reg_locked(reg, cur);
    }
    furi_hal_i2c_release(mcp_i2c_handle);

    return ok;
}

// Set pin direction: true = input, false = output
bool furi_hal_mcp23017_set_pin_direction(uint8_t pin, bool is_input) {
    if(pin > 15) return false;

    uint8_t iodir_reg = (pin < 8) ? MCP_IODIRA : MCP_IODIRB;
    uint8_t bit = (uint8_t)(1u << (pin & 0x7));
    uint8_t cur = 0;
    bool ok;

    furi_hal_i2c_acquire(mcp_i2c_handle);
    ok = mcp_read_reg_locked(iodir_reg, &cur);
    if(ok) {
        if(is_input) cur |= bit;
        else cur &= (uint8_t)~bit;
        ok = mcp_write_reg_locked(iodir_reg, cur);
    }
    furi_hal_i2c_release(mcp_i2c_handle);

    return ok;
}

// LED control functions - RGB LEDs on MCP23017 port B (B1=RED, B2=GREEN, B3=BLUE)
// Initialize RGB LED pins as outputs and turn them off
bool furi_hal_mcp23017_led_init(void) {
    bool ok = true;
    ok = ok && furi_hal_mcp23017_set_pin_direction(9, false);
    ok = ok && furi_hal_mcp23017_set_pin_direction(10, false);
    ok = ok && furi_hal_mcp23017_set_pin_direction(11, false);
    ok = ok && furi_hal_mcp23017_write_pin(9, false);
    ok = ok && furi_hal_mcp23017_write_pin(10, false);
    ok = ok && furi_hal_mcp23017_write_pin(11, false);
    return ok;
}

// Set individual LED colors (on/off only, no PWM)
bool furi_hal_mcp23017_led_set_red(bool on) {
    return furi_hal_mcp23017_write_pin(9, on);
}

bool furi_hal_mcp23017_led_set_green(bool on) {
    return furi_hal_mcp23017_write_pin(10, on);
}

bool furi_hal_mcp23017_led_set_blue(bool on) {
    return furi_hal_mcp23017_write_pin(11, on);
}

// Set all three LED colors at once
bool furi_hal_mcp23017_led_set_color(bool red, bool green, bool blue) {
    bool ok = true;
    uint8_t cur = 0;

    // Atomically update the LED bits on GPIOB.
    furi_hal_i2c_acquire(mcp_i2c_handle);
    ok = mcp_read_reg_locked(MCP_GPIOB, &cur);
    if(ok) {
        uint8_t mask = (1u << 1) | (1u << 2) | (1u << 3);
        cur &= (uint8_t)~mask;
        if(red) cur |= (1u << 1);
        if(green) cur |= (1u << 2);
        if(blue) cur |= (1u << 3);
        ok = mcp_write_reg_locked(MCP_GPIOB, cur);
    }
    furi_hal_i2c_release(mcp_i2c_handle);

    return ok;
}

// Set LED with brightness value (0x00 = off, 0xFF = on)
bool furi_hal_mcp23017_led_set(uint8_t red, uint8_t green, uint8_t blue) {
    return furi_hal_mcp23017_led_set_color(
        red > 0,
        green > 0,
        blue > 0);
}

// Turn all LEDs off
bool furi_hal_mcp23017_led_off(void) {
    return furi_hal_mcp23017_led_set_color(false, false, false);
}