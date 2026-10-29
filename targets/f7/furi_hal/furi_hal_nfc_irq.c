#include "furi_hal_nfc_i.h"
#include <furi_hal_resources.h>

static void furi_hal_nfc_int_callback(void* context) {
    UNUSED(context);
    furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
}

uint32_t furi_hal_nfc_get_irq(const FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    // On PN532, IRQ pin is active low. When pin is LOW, response is ready.
    bool active = !furi_hal_gpio_read(&gpio_nfc_irq_rfid_pull);
    return active ? 1 : 0;
}

void furi_hal_nfc_init_gpio_isr(void) {
    // PN532 IRQ is active low, pull up and trigger on falling edge
    furi_hal_gpio_init(
        &gpio_nfc_irq_rfid_pull, GpioModeInterruptFall, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_add_int_callback(&gpio_nfc_irq_rfid_pull, furi_hal_nfc_int_callback, NULL);
    furi_hal_gpio_enable_int_callback(&gpio_nfc_irq_rfid_pull);
}

void furi_hal_nfc_deinit_gpio_isr(void) {
    furi_hal_gpio_remove_int_callback(&gpio_nfc_irq_rfid_pull);
    furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}
