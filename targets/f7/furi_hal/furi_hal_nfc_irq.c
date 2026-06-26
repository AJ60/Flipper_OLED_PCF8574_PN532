#include "furi_hal_nfc_i.h"

#include <lib/drivers/st25r3916.h>
#include <furi_hal_resources.h>

static void furi_hal_nfc_int_callback(void* context) {
    UNUSED(context);
    furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
}

uint32_t furi_hal_nfc_get_irq(const FuriHalSpiBusHandle* handle) {
    uint32_t irq = 0;
    uint32_t safety_counter = 100;

    // Use NFC acquire with recursive lock support.
    // When called from wait_event_common (no lock held), this does a fresh acquire.
    // When called from wait_for_specific_irq during listener_tx (lock already held),
    // the recursive mechanism just increments lock_count without re-acquiring SPI.
    // We MUST NOT fail here: if IRQ registers aren't read, the IRQ pin stays HIGH,
    // no new rising-edge interrupt fires, and the event loop hangs forever.
    // Retry in a loop until we succeed.
    while(furi_hal_nfc_acquire() != FuriHalNfcErrorNone) {
        furi_delay_ms(1);
    }

    while(furi_hal_gpio_read_port_pin(gpio_nfc_irq_rfid_pull.port, gpio_nfc_irq_rfid_pull.pin) && safety_counter > 0) {
        irq |= st25r3916_get_irq(handle);
        safety_counter--;
    }

    furi_hal_nfc_release();

    if(safety_counter == 0) {
        FURI_LOG_E("FuriHalNfcIrq", "NFC IRQ pin stuck HIGH! Possible hardware or SPI communication failure.");
    }
    return irq;
}

void furi_hal_nfc_init_gpio_isr(void) {
    furi_hal_gpio_init(
        &gpio_nfc_irq_rfid_pull, GpioModeInterruptRise, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_add_int_callback(&gpio_nfc_irq_rfid_pull, furi_hal_nfc_int_callback, NULL);
    furi_hal_gpio_enable_int_callback(&gpio_nfc_irq_rfid_pull);
}

void furi_hal_nfc_deinit_gpio_isr(void) {
    furi_hal_gpio_remove_int_callback(&gpio_nfc_irq_rfid_pull);
    furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}
