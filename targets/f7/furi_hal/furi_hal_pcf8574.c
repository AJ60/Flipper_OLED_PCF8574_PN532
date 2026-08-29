#include "furi_hal_pcf8574.h"
#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_gpio.h>
#include <stm32wbxx_ll_cortex.h>

#define TAG "FuriHalPCF8574"

// PCF8574 typical 7-bit address
#define PCF8574_DEFAULT_ADDR 0x20

// Static driver state — not exposed globally
static const FuriHalI2cBusHandle* pcf8574_bus = NULL;
static uint8_t pcf8574_addr = PCF8574_DEFAULT_ADDR;
// Output shadow: the bits the driver last wrote to pins that are used as
// outputs (e.g. vibro P6). Bits of pins declared as inputs (see
// pcf8574_input_mask) are never stored here — adopting an input pin's live
// level (e.g. a pressed active-low button) into this shadow would make the
// next read-modify-write drive that pin low and latch the button.
static uint8_t pcf8574_current_state = 0xFF;
// Pins configured as inputs via furi_hal_pcf8574_configure_interrupts(). Such
// pins must always be written as 1 (quasi-bidirectional weak pull-up); writing
// a 0 turns them into strong outputs, which would hold a released button low.
static uint8_t pcf8574_input_mask = 0;
static GpioExtiCallback pcf8574_int_cb = NULL;
static void* pcf8574_int_ctx = NULL;
static bool pcf8574_is_initialized = false;

// Resolve the bus used for all transactions: the bus explicitly set via
// furi_hal_pcf8574_set_i2c_bus() if any, otherwise the power bus (I2C1),
// which also hosts the OLED and INA219 on this board.
static const FuriHalI2cBusHandle* pcf8574_get_bus(void) {
    return pcf8574_bus ? pcf8574_bus : &furi_hal_i2c_handle_power;
}

// Build the byte that is actually written to the chip: output bits come from
// the given shadow state, input bits are always forced high. This is what
// makes the read-modify-write in write_pin/write_port safe against
// concurrently changing button pins.
static uint8_t pcf8574_compose_byte(uint8_t state) {
    return (uint8_t)((state & (uint8_t)~pcf8574_input_mask) | pcf8574_input_mask);
}

// Functional detection probe for a candidate 7-bit address.
//
// The PCF8574 is a transparent I/O port with no register space: a byte written
// to it is presented on the port pins and can be read straight back. Other
// devices sharing the bus ACK reads of their register space but do NOT echo
// arbitrary bytes, so a plain ACK probe is not enough to tell them apart —
// the INA219 sits at wire address 0x40 (PCF8574 candidate 0x20 << 1) and the
// OLED at 0x78 (PCF8574A candidate 0x3C << 1).
//
// Two complementary patterns (0x55/0xAA) are written and read back. On a real
// PCF8574 every flipped bit must flip on read-back, except pins externally held
// low (an active-low button pressed during boot). Register-based devices do not
// track the pattern and are rejected. Up to two externally held pins are
// tolerated so a boot-time button press cannot brick detection.
static bool pcf8574_probe_candidate(uint8_t addr) {
    bool ok = true;
    uint8_t wr = 0x55;
    uint8_t rb_a = 0x00;
    uint8_t rb_b = 0x00;
    const FuriHalI2cBusHandle* bus = pcf8574_get_bus();

    furi_hal_i2c_acquire(bus);
    ok = furi_hal_i2c_tx(bus, (uint8_t)(addr << 1), &wr, 1, 50);
    if(ok) {
        furi_delay_us(50); // let quasi-bidirectional outputs settle
        ok = furi_hal_i2c_rx(bus, (uint8_t)(addr << 1), &rb_a, 1, 50);
    }
    wr = 0xAA;
    if(ok) {
        ok = furi_hal_i2c_tx(bus, (uint8_t)(addr << 1), &wr, 1, 50);
    }
    if(ok) {
        furi_delay_us(50);
        ok = furi_hal_i2c_rx(bus, (uint8_t)(addr << 1), &rb_b, 1, 50);
    }
    furi_hal_i2c_release(bus);

    if(!ok) {
        return false;
    }

    // Count the bits that tracked the flipped 0x55 <-> 0xAA pattern.
    uint8_t flipped = (uint8_t)(rb_a ^ rb_b);
    uint8_t cnt = 0;
    for(uint8_t i = 0; i < 8; i++) {
        if(flipped & (uint8_t)(1u << i)) {
            cnt++;
        }
    }
    return cnt >= 6;
}

void furi_hal_pcf8574_set_i2c_bus(const FuriHalI2cBusHandle* bus_handle) {
    furi_check(bus_handle);
    pcf8574_bus = bus_handle;
    FURI_LOG_I(TAG, "PCF8574 I2C bus set");
}

bool furi_hal_pcf8574_init(void) {
    // Idempotent: init is called from several boot paths (vibro, input service).
    // Re-running a full probe/write cycle would reset the port to 0xFF and
    // clobber any output state configured by whoever initialized first.
    if(pcf8574_is_initialized) {
        FURI_LOG_I(TAG, "PCF8574 already initialized at 0x%02X", pcf8574_addr);
        return true;
    }

    const FuriHalI2cBusHandle* bus = pcf8574_get_bus();

    // PCF8574 addresses: 0x20..0x27
    // PCF8574A addresses: 0x38..0x3F
    uint8_t candidates[16];
    size_t cidx = 0;
    for(uint8_t a = 0x20; a <= 0x27; a++) {
        candidates[cidx++] = a;
    }
    for(uint8_t a = 0x38; a <= 0x3F; a++) {
        candidates[cidx++] = a;
    }

    bool detected = false;
    uint8_t detected_addr = PCF8574_DEFAULT_ADDR;
    // The first device that ACKed a write during the scan. In the normal
    // hardware layout the PCF8574 is the first candidate (0x20), so on a
    // detection failure this is very likely the real chip rather than the
    // OLED/INA219 that may ACK later addresses.
    uint8_t first_write_ack_addr = 0;

    for(size_t i = 0; i < cidx && !detected; i++) {
        // Cheap ACK pre-check before the full functional probe.
        furi_hal_i2c_acquire(bus);
        bool ready = furi_hal_i2c_is_device_ready(bus, (uint8_t)(candidates[i] << 1), 10);
        furi_hal_i2c_release(bus);
        if(!ready) {
            continue;
        }
        if(first_write_ack_addr == 0) {
            first_write_ack_addr = candidates[i];
        }

        if(pcf8574_probe_candidate(candidates[i])) {
            detected = true;
            detected_addr = candidates[i];
        }
    }

    if(!detected) {
        // Best-effort restore: the first ACKing device may be a real PCF8574
        // whose functional probe failed (e.g. >2 buttons held at boot). Put it
        // back to the idle all-high state so outputs are not left active.
        if(first_write_ack_addr != 0) {
            uint8_t idle = 0xFF;
            furi_hal_i2c_acquire(bus);
            furi_hal_i2c_tx(bus, (uint8_t)(first_write_ack_addr << 1), &idle, 1, 50);
            furi_hal_i2c_release(bus);
        }
        FURI_LOG_E(TAG, "PCF8574 not detected on I2C bus");
        return false;
    }

    pcf8574_addr = detected_addr;

    // Initialize all pins to 1 (inputs / high state). Reset the input-mask
    // bookkeeping too: if a previous init failed partway, the chip is in an
    // unknown state and must be re-declared from scratch.
    pcf8574_input_mask = 0;
    pcf8574_current_state = 0xFF;
    furi_hal_i2c_acquire(bus);
    bool res = furi_hal_i2c_tx(bus, (uint8_t)(pcf8574_addr << 1), &pcf8574_current_state, 1, 100);
    furi_hal_i2c_release(bus);

    if(!res) {
        FURI_LOG_E(TAG, "Failed to initialize PCF8574");
        return false;
    }

    pcf8574_is_initialized = true;
    FURI_LOG_I(TAG, "PCF8574 initialized at 0x%02X", pcf8574_addr);
    return true;
}

bool furi_hal_pcf8574_init_ex(uint8_t i2c_addr) {
    // Idempotent, same as furi_hal_pcf8574_init(). Note that if a previous
    // init already succeeded, the explicit address below is ignored in favor of
    // the already-bound one.
    if(pcf8574_is_initialized) {
        FURI_LOG_I(TAG, "PCF8574 already initialized at 0x%02X", pcf8574_addr);
        return true;
    }

    if(pcf8574_probe_candidate(i2c_addr)) {
        pcf8574_addr = i2c_addr;
        pcf8574_input_mask = 0;
        pcf8574_current_state = 0xFF;
        const FuriHalI2cBusHandle* bus = pcf8574_get_bus();
        furi_hal_i2c_acquire(bus);
        bool res =
            furi_hal_i2c_tx(bus, (uint8_t)(pcf8574_addr << 1), &pcf8574_current_state, 1, 100);
        furi_hal_i2c_release(bus);
        if(res) {
            pcf8574_is_initialized = true;
            FURI_LOG_I(TAG, "PCF8574 initialized at 0x%02X (explicit)", pcf8574_addr);
        }
        return res;
    }

    // Explicit address not found — fall back to auto-scan
    FURI_LOG_W(TAG, "PCF8574 not found at 0x%02X, falling back to auto-scan", i2c_addr);
    return furi_hal_pcf8574_init();
}

bool furi_hal_pcf8574_read_port(uint8_t* port_state) {
    if(!port_state) {
        return false;
    }
    const FuriHalI2cBusHandle* bus = pcf8574_get_bus();
    furi_hal_i2c_acquire(bus);
    bool res = furi_hal_i2c_rx(bus, (uint8_t)(pcf8574_addr << 1), port_state, 1, 100);
    furi_hal_i2c_release(bus);

    if(res) {
        // Keep the shadow state in sync with what the chip actually holds, but
        // ONLY for pins that are not declared as inputs. A read on an input pin
        // reflects external logic (e.g. a pressed active-low button reads 0);
        // adopting that into the shadow would make the next read-modify-write
        // drive the button pin low and latch it as "pressed" after release.
        // Output pins read back their driven level, so syncing them is safe.
        pcf8574_current_state = (pcf8574_current_state & pcf8574_input_mask) |
                                (*port_state & (uint8_t)~pcf8574_input_mask);
    }
    return res;
}

bool furi_hal_pcf8574_write_port(uint8_t port_state) {
    const FuriHalI2cBusHandle* bus = pcf8574_get_bus();
    furi_hal_i2c_acquire(bus);
    // Only non-input bits are writable; input pins are forced high regardless
    // of what the caller passes (writing 0 to an input pin would latch it low).
    // The RMW and compose run under the bus mutex so they stay atomic with the
    // input service's periodic read_port shadow sync; the shadow is committed
    // only if the wire write succeeds.
    uint8_t new_state = (pcf8574_current_state & pcf8574_input_mask) |
                        (port_state & (uint8_t)~pcf8574_input_mask);
    uint8_t byte = pcf8574_compose_byte(new_state);
    bool res = furi_hal_i2c_tx(bus, (uint8_t)(pcf8574_addr << 1), &byte, 1, 100);
    if(res) {
        pcf8574_current_state = new_state;
    }
    furi_hal_i2c_release(bus);
    return res;
}

bool furi_hal_pcf8574_write_pin(uint8_t pin, bool value) {
    if(pin > 7) {
        return false;
    }
    const FuriHalI2cBusHandle* bus = pcf8574_get_bus();
    furi_hal_i2c_acquire(bus);
    // Update only the requested bit in the output shadow, under the bus mutex
    // so this stays atomic with the input service's periodic read_port shadow
    // sync. Input pins are never touched: compose forces their bits high on the
    // wire, so a write_pin can never latch an input (button) pin low even if it
    // was read as pressed moments ago. The shadow is committed only if the wire
    // write succeeds.
    uint8_t new_state = pcf8574_current_state;
    if(value) {
        new_state |= (uint8_t)(1u << pin);
    } else {
        new_state &= (uint8_t) ~(1u << pin);
    }
    uint8_t byte = pcf8574_compose_byte(new_state);
    bool res = furi_hal_i2c_tx(bus, (uint8_t)(pcf8574_addr << 1), &byte, 1, 100);
    if(res) {
        pcf8574_current_state = new_state;
    }
    furi_hal_i2c_release(bus);
    return res;
}

void furi_hal_pcf8574_attach_int(GpioExtiCallback cb, void* ctx) {
    furi_check(cb);
    // Store callback and configure the PCF INT GPIO (active-LOW, falling edge)
    pcf8574_int_cb = cb;
    pcf8574_int_ctx = ctx;
    furi_hal_gpio_init(&gpio_pcf_int, GpioModeInterruptFall, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_add_int_callback(&gpio_pcf_int, cb, ctx);
    furi_hal_gpio_enable_int_callback(&gpio_pcf_int);

    // gpio_pcf_int is PB0 = EXTI line 0. furi_hal_gpio_* only sets the EXTI IT
    // bit; without NVIC enablement the EXTI0_IRQHandler never runs and the
    // interrupt-driven wakeup is silently dead (buttons would still work, but
    // only via the input service's 20 ms polling fallback). Note: the
    // resources init comment claiming EXTI15_10 covers PB0 is wrong — that
    // handler serves lines 10-15 only.
    NVIC_SetPriority(EXTI0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI0_IRQn);
}

bool furi_hal_pcf8574_configure_interrupts(uint8_t gpios_to_input_mask) {
    // For PCF8574, setting a pin's bit to 1 makes it act as an input (weak
    // pull-up) and participate in the INT behaviour. There is no hardware
    // interrupt-mask register; the mask is bookkeeping so the driver never
    // drives these pins low (see pcf8574_compose_byte).
    // The mask OR and the compose run under the bus mutex: this both makes the
    // bookkeeping atomic and re-asserts the input pins high while preserving
    // the current output shadow, so it cannot clobber e.g. a concurrent vibro
    // write with a stale shadow snapshot.
    const FuriHalI2cBusHandle* bus = pcf8574_get_bus();
    furi_hal_i2c_acquire(bus);
    pcf8574_input_mask |= gpios_to_input_mask;
    uint8_t byte = pcf8574_compose_byte(pcf8574_current_state);
    bool res = furi_hal_i2c_tx(bus, (uint8_t)(pcf8574_addr << 1), &byte, 1, 100);
    furi_hal_i2c_release(bus);
    return res;
}

bool furi_hal_pcf8574_check_and_restore(uint8_t expected_mask) {
    // PCF8574 has no config registers to read back. After a silent reset
    // the chip reverts to 0xFF (all quasi-bidirectional inputs). Restore
    // the full port state: re-assert the input mask AND re-write the
    // output shadow so output pins (vibro etc.) return to known state.
    const FuriHalI2cBusHandle* bus = pcf8574_get_bus();
    furi_hal_i2c_acquire(bus);
    pcf8574_input_mask = expected_mask; // force, not OR
    uint8_t byte = pcf8574_compose_byte(pcf8574_current_state);
    bool res = furi_hal_i2c_tx(bus, (uint8_t)(pcf8574_addr << 1), &byte, 1, 100);
    furi_hal_i2c_release(bus);
    return res;
}

// LED Stub functions - Since PCF8574 has only 8 pins, there's no room for RGB LED.
bool furi_hal_pcf8574_led_init(void) {
    return true;
}
bool furi_hal_pcf8574_led_set_red(bool on) {
    UNUSED(on);
    return true;
}
bool furi_hal_pcf8574_led_set_green(bool on) {
    UNUSED(on);
    return true;
}
bool furi_hal_pcf8574_led_set_blue(bool on) {
    UNUSED(on);
    return true;
}
bool furi_hal_pcf8574_led_set_color(bool red, bool green, bool blue) {
    UNUSED(red);
    UNUSED(green);
    UNUSED(blue);
    return true;
}
bool furi_hal_pcf8574_led_set(uint8_t red, uint8_t green, uint8_t blue) {
    UNUSED(red);
    UNUSED(green);
    UNUSED(blue);
    return true;
}
bool furi_hal_pcf8574_led_off(void) {
    return true;
}
