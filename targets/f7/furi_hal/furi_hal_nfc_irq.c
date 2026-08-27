#include "furi_hal_nfc_i.h"

#include <lib/drivers/st25r3916.h>
#include <furi_hal_resources.h>

static void furi_hal_nfc_int_callback(void* context) {
    UNUSED(context);
    furi_hal_nfc_event_set(FuriHalNfcEventInternalTypeIrq);
}

uint32_t furi_hal_nfc_get_irq(const FuriHalSpiBusHandle* handle) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    UNUSED(handle);
    // On PN532, IRQ pin is active low. When pin is LOW, response is ready.
    bool active = !furi_hal_gpio_read(&gpio_nfc_irq_rfid_pull);
    return active ? 1 : 0;
#else
    uint32_t irq = 0;
    uint32_t safety_counter = 100;

    // furi_hal_nfc_get_irq is called only from furi_hal_nfc_wait_event_common,
    // always from thread context and never while this thread already holds the
    // NFC mutex. A single blocking acquire is sufficient and avoids the
    // previous busy-spin that could stall IRQ processing by up to 1001 ms.
    FuriHalNfcError acq_err = furi_hal_nfc_acquire();
    if(acq_err != FuriHalNfcErrorNone) {
        FURI_LOG_E("FuriHalNfcIrq", "Failed to acquire NFC lock for IRQ read (err=%d)", acq_err);
        return 0;
    }

    while(furi_hal_gpio_read_port_pin(gpio_nfc_irq_rfid_pull.port, gpio_nfc_irq_rfid_pull.pin) &&
          safety_counter > 0) {
        irq |= st25r3916_get_irq(handle);
        safety_counter--;
    }

    furi_hal_nfc_release();

    if(safety_counter == 0) {
        FURI_LOG_E(
            "FuriHalNfcIrq",
            "NFC IRQ pin stuck HIGH! Possible hardware or SPI communication failure.");
    }
    return irq;
#endif
}

void furi_hal_nfc_init_gpio_isr(void) {
#if defined(FURI_HAL_NFC_CHIP_PN532)
    // PN532 IRQ is active low, pull up and trigger on falling edge
    furi_hal_gpio_init(
        &gpio_nfc_irq_rfid_pull, GpioModeInterruptFall, GpioPullUp, GpioSpeedVeryHigh);
#else
    furi_hal_gpio_init(
        &gpio_nfc_irq_rfid_pull, GpioModeInterruptRise, GpioPullDown, GpioSpeedVeryHigh);
#endif
    furi_hal_gpio_add_int_callback(&gpio_nfc_irq_rfid_pull, furi_hal_nfc_int_callback, NULL);
    furi_hal_gpio_enable_int_callback(&gpio_nfc_irq_rfid_pull);
}

void furi_hal_nfc_deinit_gpio_isr(void) {
    furi_hal_gpio_remove_int_callback(&gpio_nfc_irq_rfid_pull);
    furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}
