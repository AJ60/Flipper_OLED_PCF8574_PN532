#include "nrf24.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <assert.h>
#include <string.h>

const FuriHalSpiBusHandle* nrf24_active_handle = NULL;
static bool nrf24_test_handle(const FuriHalSpiBusHandle* handle);

const FuriHalSpiBusHandle* nrf24_get_handle(void) {
    if(!nrf24_active_handle) {
        nrf24_init();
    }
    return nrf24_active_handle;
}

void nrf24_init() {
    // Initialize both potential CS pins high so neither floats or interferes
    furi_hal_gpio_init_simple(&gpio_ext_pc3, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pc3, true);
    furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pa4, true);

    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external_extra);

    furi_hal_gpio_init(nrf24_CE_PIN, GpioModeOutputPushPull, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_write(nrf24_CE_PIN, false);

    furi_delay_ms(5);

    const FuriHalSpiBusHandle* pref = (momentum_settings.spi_nrf24_handle == SpiDefault ?
                                       &furi_hal_spi_bus_handle_external_extra :
                                       &furi_hal_spi_bus_handle_external);
    const FuriHalSpiBusHandle* alt = (pref == &furi_hal_spi_bus_handle_external_extra ?
                                      &furi_hal_spi_bus_handle_external :
                                      &furi_hal_spi_bus_handle_external_extra);

    if(nrf24_test_handle(pref)) {
        nrf24_active_handle = pref;
        FURI_LOG_I("nrf24", "NRF24 detected on %s", (pref == &furi_hal_spi_bus_handle_external_extra) ? "Pin 4 (PA4)" : "Pin 7 (PC3)");
    } else if(nrf24_test_handle(alt)) {
        nrf24_active_handle = alt;
        FURI_LOG_I("nrf24", "NRF24 detected on %s", (alt == &furi_hal_spi_bus_handle_external_extra) ? "Pin 4 (PA4)" : "Pin 7 (PC3)");
    } else {
        nrf24_active_handle = pref;
        FURI_LOG_W("nrf24", "NRF24 not responding on Pin 4 or Pin 7, defaulting to %s", (pref == &furi_hal_spi_bus_handle_external_extra) ? "Pin 4" : "Pin 7");
    }
}

void nrf24_deinit() {
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external_extra);
    furi_hal_gpio_write(nrf24_CE_PIN, false);
    furi_hal_gpio_init(nrf24_CE_PIN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    // resetting both CS pins to floating analog
    furi_hal_gpio_init_simple(&gpio_ext_pc3, GpioModeAnalog);
    furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeAnalog);

    nrf24_active_handle = NULL;
}

void nrf24_spi_trx(
    const FuriHalSpiBusHandle* handle,
    uint8_t* tx,
    uint8_t* rx,
    uint8_t size,
    uint32_t timeout) {
    UNUSED(timeout);
    furi_hal_spi_acquire(handle);
    furi_hal_gpio_write(handle->cs, false);
    furi_hal_spi_bus_trx(handle, tx, rx, size, nrf24_TIMEOUT);
    furi_hal_gpio_write(handle->cs, true);
    furi_hal_spi_release(handle);
}

uint8_t nrf24_write_reg(const FuriHalSpiBusHandle* handle, uint8_t reg, uint8_t data) {
    uint8_t tx[2] = {W_REGISTER | (REGISTER_MASK & reg), data};
    uint8_t rx[2] = {0};
    nrf24_spi_trx(handle, tx, rx, 2, nrf24_TIMEOUT);
    return rx[0];
}

uint8_t nrf24_read_reg(const FuriHalSpiBusHandle* handle, uint8_t reg, uint8_t* data, uint8_t size) {
    uint8_t tx[size + 1];
    uint8_t rx[size + 1];
    memset(rx, 0, size + 1);
    tx[0] = R_REGISTER | (REGISTER_MASK & reg);
    memset(&tx[1], 0, size);
    nrf24_spi_trx(handle, tx, rx, size + 1, nrf24_TIMEOUT);
    memcpy(data, &rx[1], size);
    return rx[0];
}

uint8_t nrf24_flush_rx(const FuriHalSpiBusHandle* handle) {
    uint8_t tx[] = {FLUSH_RX};
    uint8_t rx[] = {0};
    nrf24_spi_trx(handle, tx, rx, 1, nrf24_TIMEOUT);
    return rx[0];
}

uint8_t nrf24_get_rdp(const FuriHalSpiBusHandle* handle) {
    uint8_t rdp;
    nrf24_read_reg(handle, REG_RDP, &rdp, 1);
    return rdp;
}

uint8_t nrf24_status(const FuriHalSpiBusHandle* handle) {
    uint8_t status;
    uint8_t tx[] = {R_REGISTER | (REGISTER_MASK & REG_STATUS)};
    nrf24_spi_trx(handle, tx, &status, 1, nrf24_TIMEOUT);
    return status;
}

uint8_t nrf24_set_idle(const FuriHalSpiBusHandle* handle) {
    uint8_t status = 0;
    uint8_t cfg = 0;
    nrf24_read_reg(handle, REG_CONFIG, &cfg, 1);
    cfg &= 0xfc; // clear bottom two bits to power down the radio
    status = nrf24_write_reg(handle, REG_CONFIG, cfg);
    furi_hal_gpio_write(nrf24_CE_PIN, false);
    return status;
}

uint8_t nrf24_set_rx_mode(const FuriHalSpiBusHandle* handle, bool nodelay) {
    uint8_t status = 0;
    uint8_t cfg = 0;
    nrf24_read_reg(handle, REG_CONFIG, &cfg, 1);
    cfg |= 0x03; // PWR_UP, and PRIM_RX
    status = nrf24_write_reg(handle, REG_CONFIG, cfg);
    furi_hal_gpio_write(nrf24_CE_PIN, true);
    if(!nodelay) furi_delay_ms(2000);
    return status;
}

static bool nrf24_test_handle(const FuriHalSpiBusHandle* handle) {
    uint8_t status = nrf24_status(handle);
    if(status == 0x00 || status == 0xFF) return false;
    uint8_t orig = 0;
    nrf24_read_reg(handle, REG_RF_CH, &orig, 1);
    nrf24_write_reg(handle, REG_RF_CH, 0x42);
    uint8_t test = 0;
    nrf24_read_reg(handle, REG_RF_CH, &test, 1);
    nrf24_write_reg(handle, REG_RF_CH, orig);
    if(test == 0x42) return true;
    return ((status & 0x80) == 0 && (status != 0x00));
}

bool nrf24_check_connected(const FuriHalSpiBusHandle* handle) {
    if(!handle) handle = nrf24_get_handle();
    if(nrf24_test_handle(handle)) {
        return true;
    }
    const FuriHalSpiBusHandle* alt = (handle == &furi_hal_spi_bus_handle_external ?
                                      &furi_hal_spi_bus_handle_external_extra :
                                      &furi_hal_spi_bus_handle_external);
    if(nrf24_test_handle(alt)) {
        nrf24_active_handle = alt;
        FURI_LOG_I("nrf24", "NRF24 re-detected on %s", (alt == &furi_hal_spi_bus_handle_external) ? "Pin 7 (PC3)" : "Pin 4 (PA4)");
        return true;
    }
    return false;
}
