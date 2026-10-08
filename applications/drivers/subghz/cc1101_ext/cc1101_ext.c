#include "cc1101_ext.h"
#include <lib/subghz/devices/cc1101_configs.h>

#include <furi_hal_version.h>
#include <furi_hal_rtc.h>
#include <furi_hal_spi.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_resources.h>
#include <furi_hal_bus.h>

#include <stm32wbxx_ll_dma.h>
#include <furi_hal_cortex.h>

#include <furi.h>
#include <cc1101.h>
#include <stdio.h>

#include <momentum/settings.h>

#define TAG "SubGhzDeviceCc1101Ext"

#define SUBGHZ_DEVICE_CC1101_EXT_TX_GPIO       (&gpio_ext_pb2)
// Board wiring (user-confirmed): E07-433M20S TXEN -> board A5.
// On this DIY board A5 == MCU PA5 == header PC3 (gpio_ext_pc3).
// PA5 is shared with the LF-RFID 125kHz carrier, so TXEN must be
// driven low (module idle) whenever SubGHz EXT is not transmitting,
// and must never be left floating/analog.
#define SUBGHZ_DEVICE_CC1101_EXT_E07_AMP_GPIO  (&gpio_ext_pc3)
#define SUBGHZ_DEVICE_CC1101_EXT_E07_TXEN_GPIO SUBGHZ_DEVICE_CC1101_EXT_E07_AMP_GPIO

#define SUBGHZ_DEVICE_CC1101_CONFIG_VER 1

/* DMA Channels definition */
#define SUBGHZ_DEVICE_CC1101_EXT_DMA             (DMA2)
#define SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_CHANNEL (LL_DMA_CHANNEL_3)
#define SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_CHANNEL (LL_DMA_CHANNEL_4)
#define SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_CHANNEL (LL_DMA_CHANNEL_5)
#define SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_IRQ     (FuriHalInterruptIdDma2Ch3)
#define SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF \
    SUBGHZ_DEVICE_CC1101_EXT_DMA, SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_CHANNEL
#define SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_DEF \
    SUBGHZ_DEVICE_CC1101_EXT_DMA, SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_CHANNEL
#define SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_DEF \
    SUBGHZ_DEVICE_CC1101_EXT_DMA, SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_CHANNEL

/** Low level buffer dimensions and guard times */
#define SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_FULL (256u)
#define SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_HALF \
    (SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_FULL / 2)
#define SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_GUARD_TIME (999u >> 1)

/** SubGhz state */
typedef enum {
    SubGhzDeviceCC1101ExtStateInit, /**< Init pending */
    SubGhzDeviceCC1101ExtStateIdle, /**< Idle, energy save mode */
    SubGhzDeviceCC1101ExtStateAsyncRx, /**< Async RX started */
    SubGhzDeviceCC1101ExtStateAsyncTx, /**< Async TX started, DMA and timer is on */
} SubGhzDeviceCC1101ExtState;

/** SubGhz regulation, receive transmission on the current frequency for the
 * region */
typedef enum {
    SubGhzDeviceCC1101ExtRegulationOnlyRx, /**only Rx*/
    SubGhzDeviceCC1101ExtRegulationTxRx, /**TxRx*/
} SubGhzDeviceCC1101ExtRegulation;

typedef enum {
    SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateIdle,
    SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateReset,
    SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateRun,
} SubGhzDeviceCC1101ExtAsyncTxMiddlewareState;

typedef struct {
    SubGhzDeviceCC1101ExtAsyncTxMiddlewareState state;
    bool is_odd_level;
    uint32_t adder_duration;
} SubGhzDeviceCC1101ExtAsyncTxMiddleware;

typedef struct {
    uint32_t* buffer;
    SubGhzDeviceCC1101ExtCallback callback;
    void* callback_context;
    uint32_t gpio_tx_buff[2];
    uint32_t debug_gpio_buff[2];
    SubGhzDeviceCC1101ExtAsyncTxMiddleware middleware;
} SubGhzDeviceCC1101ExtAsyncTx;

typedef struct {
    uint32_t capture_delta_duration;
    SubGhzDeviceCC1101ExtCaptureCallback capture_callback;
    void* capture_callback_context;
} SubGhzDeviceCC1101ExtAsyncRx;

typedef struct {
    volatile SubGhzDeviceCC1101ExtState state;
    volatile SubGhzDeviceCC1101ExtRegulation regulation;
    const GpioPin* async_mirror_pin;
    const FuriHalSpiBusHandle* spi_bus_handle;
    const GpioPin* g0_pin;
    SubGhzDeviceCC1101ExtAsyncTx async_tx;
    SubGhzDeviceCC1101ExtAsyncRx async_rx;
    bool amp_and_leds;
    bool extended_range;
    bool bypass_region;
} SubGhzDeviceCC1101Ext;

static SubGhzDeviceCC1101Ext* subghz_device_cc1101_ext = NULL;

static void cc1101_ext_amp_init(void) {
    // Drive TXEN (board A5 == MCU PA5 == header PC3) low: module idle.
    furi_hal_gpio_init_simple(SUBGHZ_DEVICE_CC1101_EXT_E07_AMP_GPIO, GpioModeOutputPushPull);
    furi_hal_gpio_write(SUBGHZ_DEVICE_CC1101_EXT_E07_AMP_GPIO, 0);
}

static void cc1101_ext_txen_park_low(void) {
    // Unconditional park: TXEN must never float, even when amp_and_leds
    // is false (e.g. alloc(NULL) probe window) or RFID shares PA5.
    furi_hal_gpio_init_simple(SUBGHZ_DEVICE_CC1101_EXT_E07_TXEN_GPIO, GpioModeOutputPushPull);
    furi_hal_gpio_write(SUBGHZ_DEVICE_CC1101_EXT_E07_TXEN_GPIO, false);
}

static void cc1101_ext_amp_write(uint8_t state) {
    if(subghz_device_cc1101_ext && subghz_device_cc1101_ext->amp_and_leds) {
        furi_hal_gpio_write(SUBGHZ_DEVICE_CC1101_EXT_E07_AMP_GPIO, state);
    }
}

static bool subghz_device_cc1101_ext_check_init(void) {
    furi_assert(subghz_device_cc1101_ext->state == SubGhzDeviceCC1101ExtStateInit);
    subghz_device_cc1101_ext->state = SubGhzDeviceCC1101ExtStateIdle;

    bool ret = false;
    CC1101Status cc1101_status = {0};

    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    FURI_LOG_D(TAG, "check_init: SPI acquired");
    FuriHalCortexTimer timer = furi_hal_cortex_timer_get(100 * 1000);
    bool probed = false;

    for(int retry = 0; retry < 3; retry++) {
        // Reset
        furi_hal_gpio_init(
            subghz_device_cc1101_ext->g0_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        cc1101_status = cc1101_reset(subghz_device_cc1101_ext->spi_bus_handle);
        if(cc1101_status.CHIP_RDYn != 0) {
            if(retry == 2) break; // last retry: propagate failure
            furi_delay_ms(5);
            continue;
        }
        probed = true;
        break;
    }

    if(!probed) {
        furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
        FURI_LOG_D(TAG, "check_init: SPI released ret=%u (failed)", ret);
        furi_hal_gpio_init(
            subghz_device_cc1101_ext->g0_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        // Probe failed: restore Init state so the next alloc() re-probes
        // instead of assuming an initialized (sleeping) chip, and keep
        // TXEN (A5) parked low so the PA never floats.
        subghz_device_cc1101_ext->state = SubGhzDeviceCC1101ExtStateInit;
        cc1101_ext_txen_park_low();
        return false;
    }

    do {
        cc1101_status = cc1101_write_reg(
            subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG0, CC1101IocfgHighImpedance);
        if(cc1101_status.CHIP_RDYn != 0) {
            FURI_LOG_E(
                TAG,
                "check_init: IOCFG0 HighImpedance write failed CHIP_RDYn=%u",
                cc1101_status.CHIP_RDYn);
            break;
        }

        furi_hal_gpio_init(
            subghz_device_cc1101_ext->g0_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);

        // GD0 low
        cc1101_status = cc1101_write_reg(
            subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG0, CC1101IocfgHW);
        if(cc1101_status.CHIP_RDYn != 0) {
            FURI_LOG_E(
                TAG, "check_init: IOCFG0 HW write failed CHIP_RDYn=%u", cc1101_status.CHIP_RDYn);
            break;
        }
        while(furi_hal_gpio_read(subghz_device_cc1101_ext->g0_pin) != false) {
            if(furi_hal_cortex_timer_is_expired(timer)) {
                FURI_LOG_E(TAG, "check_init: GD0 low timeout");
                break;
            }
        }
        if(furi_hal_cortex_timer_is_expired(timer)) break;
        FURI_LOG_D(TAG, "check_init: GD0 low OK");

        // GD0 high
        furi_hal_gpio_init(
            subghz_device_cc1101_ext->g0_pin, GpioModeInput, GpioPullDown, GpioSpeedLow);
        cc1101_status = cc1101_write_reg(
            subghz_device_cc1101_ext->spi_bus_handle,
            CC1101_IOCFG0,
            CC1101IocfgHW | CC1101_IOCFG_INV);
        if(cc1101_status.CHIP_RDYn != 0) {
            FURI_LOG_E(
                TAG,
                "check_init: IOCFG0 HW+INV write failed CHIP_RDYn=%u",
                cc1101_status.CHIP_RDYn);
            break;
        }
        while(furi_hal_gpio_read(subghz_device_cc1101_ext->g0_pin) != true) {
            if(furi_hal_cortex_timer_is_expired(timer)) {
                FURI_LOG_E(TAG, "check_init: GD0 high timeout");
                break;
            }
        }
        if(furi_hal_cortex_timer_is_expired(timer)) break;
        FURI_LOG_D(TAG, "check_init: GD0 high OK");

        // Reset GD0 to floating state
        cc1101_status = cc1101_write_reg(
            subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG0, CC1101IocfgHighImpedance);
        if(cc1101_status.CHIP_RDYn != 0) {
            FURI_LOG_E(
                TAG,
                "check_init: IOCFG0 HighImpedance restore failed CHIP_RDYn=%u",
                cc1101_status.CHIP_RDYn);
            break;
        }

        // Drive GDO2 (!TX/RX) low (module idle). RXEN is wired to GDO2.
        // Keep RXEN driven low so the LNA/RF switch never floats.
        cc1101_status = cc1101_write_reg(
            subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG2, CC1101IocfgHW);
        if(cc1101_status.CHIP_RDYn != 0) {
            FURI_LOG_E(
                TAG, "check_init: IOCFG2 HW write failed CHIP_RDYn=%u", cc1101_status.CHIP_RDYn);
            break;
        }

        // Read chip version for diagnostics BEFORE releasing SPI
        uint8_t part = cc1101_get_partnumber(subghz_device_cc1101_ext->spi_bus_handle);
        uint8_t ver = cc1101_get_version(subghz_device_cc1101_ext->spi_bus_handle);
        FURI_LOG_I(TAG, "check_init: part=0x%02X ver=0x%02X", part, ver);

        // Leave chip in IDLE (not SPWD) so subsequent alloc/reset works without wake sequence
        cc1101_status = cc1101_switch_to_idle(subghz_device_cc1101_ext->spi_bus_handle);
        if(cc1101_status.CHIP_RDYn != 0) {
            FURI_LOG_E(
                TAG, "check_init: switch_to_idle failed CHIP_RDYn=%u", cc1101_status.CHIP_RDYn);
            break;
        }
        ret = true;

        FURI_LOG_I(TAG, "Init OK (part=0x%02X ver=0x%02X)", part, ver);
    } while(false);

    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
    FURI_LOG_D(TAG, "check_init: SPI released ret=%u", ret);

    if(!ret) {
        FURI_LOG_D(TAG, "Init failed (external CC1101 not detected)");
        furi_hal_gpio_init(
            subghz_device_cc1101_ext->g0_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        subghz_device_cc1101_ext->state = SubGhzDeviceCC1101ExtStateInit;
    }
    return ret;
}

bool subghz_device_cc1101_ext_alloc(SubGhzDeviceConf* conf) {
    if(subghz_device_cc1101_ext != NULL) {
        if(conf && conf->ver == SUBGHZ_DEVICE_CC1101_CONFIG_VER) {
            subghz_device_cc1101_ext->amp_and_leds = conf->amp_and_leds;
            subghz_device_cc1101_ext->extended_range = conf->extended_range;
            subghz_device_cc1101_ext->bypass_region = conf->bypass_region;
        }
        return true;
    }
    subghz_device_cc1101_ext = malloc(sizeof(SubGhzDeviceCC1101Ext));
    furi_check(subghz_device_cc1101_ext);
    subghz_device_cc1101_ext->state = SubGhzDeviceCC1101ExtStateInit;
    subghz_device_cc1101_ext->regulation = SubGhzDeviceCC1101ExtRegulationTxRx;
    subghz_device_cc1101_ext->async_mirror_pin = NULL;
    subghz_device_cc1101_ext->g0_pin = SUBGHZ_DEVICE_CC1101_EXT_TX_GPIO;
    subghz_device_cc1101_ext->amp_and_leds = false;
    subghz_device_cc1101_ext->extended_range = false;
    subghz_device_cc1101_ext->bypass_region = false;
    if(conf) {
        if(conf->ver == SUBGHZ_DEVICE_CC1101_CONFIG_VER) {
            subghz_device_cc1101_ext->amp_and_leds = conf->amp_and_leds;
            subghz_device_cc1101_ext->extended_range = conf->extended_range;
            subghz_device_cc1101_ext->bypass_region = conf->bypass_region;
        } else {
            FURI_LOG_E(TAG, "Config version mismatch");
        }
    }

    subghz_device_cc1101_ext->async_rx.capture_delta_duration = 0;

    // External SPI CS is PA4 (Header Pin 4)
    subghz_device_cc1101_ext->spi_bus_handle = &furi_hal_spi_bus_handle_external;

    // Initialize CS pin HIGH (inactive) before SPI init so CC1101 isn't selected during power-up
    furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pa4, true);

    // Park TXEN (board A5 == MCU PA5 == header PC3) low unconditionally:
    // it must never float, including during the probe window when
    // amp_and_leds is false (alloc(NULL) from is_connect).
    // Output-low is also what LF-RFID idle uses for the 125 kHz carrier pin, so
    // this does not fight the RFID stage.
    cc1101_ext_txen_park_low();

    furi_hal_spi_bus_handle_init(subghz_device_cc1101_ext->spi_bus_handle);

    // Power-up settle: E07 module needs a few ms after VCC before CC1101
    // will drive MISO. Without this, every SPI transaction times out with
    // "MISO stuck high".
    furi_delay_ms(5);

    // Enable E07 amp/LNA if configured (drives TXEN/A5 low for idle)
    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_init();
    }

    bool ok = subghz_device_cc1101_ext_check_init();
    if(!ok) {
        FURI_LOG_E(TAG, "alloc failed: check_init returned false");
        furi_hal_spi_bus_handle_deinit(subghz_device_cc1101_ext->spi_bus_handle);
        // Park TXEN low and CSN high (defined levels for a bare E07 header)
        cc1101_ext_txen_park_low();
        furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeOutputPushPull);
        furi_hal_gpio_write(&gpio_ext_pa4, true);
        free(subghz_device_cc1101_ext);
        subghz_device_cc1101_ext = NULL;
    }
    return ok;
}

void subghz_device_cc1101_ext_free(void) {
    if(subghz_device_cc1101_ext == NULL) {
        return;
    }

    if(subghz_device_cc1101_ext->state != SubGhzDeviceCC1101ExtStateInit) {
        subghz_device_cc1101_ext_sleep();
    }

    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(0);
    }

    furi_hal_spi_bus_handle_deinit(subghz_device_cc1101_ext->spi_bus_handle);

    // Park TXEN (A5) low and CSN high (defined levels for a bare E07 header)
    cc1101_ext_txen_park_low();
    furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pa4, true);

    free(subghz_device_cc1101_ext);
    subghz_device_cc1101_ext = NULL;
}

void subghz_device_cc1101_ext_set_async_mirror_pin(const GpioPin* pin) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "set_async_mirror_pin: no device");
        return;
    }
    subghz_device_cc1101_ext->async_mirror_pin = pin;
}

const GpioPin* subghz_device_cc1101_ext_get_data_gpio(void) {
    if(subghz_device_cc1101_ext == NULL) {
        return NULL;
    }
    return subghz_device_cc1101_ext->g0_pin;
}

bool subghz_device_cc1101_ext_is_connect(void) {
    bool ret = false;

    if(subghz_device_cc1101_ext == NULL) { // not initialized
        ret = subghz_device_cc1101_ext_alloc(NULL);
        if(ret) {
            // alloc ended in check_init with chip in IDLE (not SPWD).
            // Do NOT run sleep() here — the next alloc() will call check_init()
            // which expects IDLE. Leaving chip in SPWD causes probe retries.
        }
        subghz_device_cc1101_ext_free();
    } else { // initialized
        furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
        uint8_t partnumber = cc1101_get_partnumber(subghz_device_cc1101_ext->spi_bus_handle);
        uint8_t version = cc1101_get_version(subghz_device_cc1101_ext->spi_bus_handle);
        furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
        // Normal CC1101: partnumber=0, version=20 (0x14).
        // The old test (part != 0 && part != 0xFF) was inverted and always
        // reported the external module as absent. This is the correct test.
        ret = (partnumber == 0) && (version != 0 && version != 0xFF);
    }

    return ret;
}

void subghz_device_cc1101_ext_sleep(void) {
    if(subghz_device_cc1101_ext == NULL) {
        return;
    }

    if(subghz_device_cc1101_ext->state != SubGhzDeviceCC1101ExtStateIdle) {
        if(subghz_device_cc1101_ext->state == SubGhzDeviceCC1101ExtStateAsyncTx) {
            subghz_device_cc1101_ext_stop_async_tx();
        } else if(subghz_device_cc1101_ext->state == SubGhzDeviceCC1101ExtStateAsyncRx) {
            subghz_device_cc1101_ext_stop_async_rx();
        }
    }

    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(0);
    }

    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);

    cc1101_switch_to_idle(subghz_device_cc1101_ext->spi_bus_handle);

    cc1101_write_reg(
        subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG0, CC1101IocfgHighImpedance);
    // Drive GDO2 low (!TX/RX, RXEN disabled)
    cc1101_write_reg(subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG2, CC1101IocfgHW);
    furi_hal_gpio_init(subghz_device_cc1101_ext->g0_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    cc1101_shutdown(subghz_device_cc1101_ext->spi_bus_handle);

    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_dump_state(void) {
    if(subghz_device_cc1101_ext == NULL) {
        printf("[subghz_device_cc1101_ext] not initialized\r\n");
        return;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    printf(
        "[subghz_device_cc1101_ext] cc1101 chip %d, version %d\r\n",
        cc1101_get_partnumber(subghz_device_cc1101_ext->spi_bus_handle),
        cc1101_get_version(subghz_device_cc1101_ext->spi_bus_handle));
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_load_custom_preset(const uint8_t* preset_data) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "load_custom_preset: no device");
        return;
    }
    if(preset_data == NULL) {
        FURI_LOG_E(TAG, "load_custom_preset: NULL data");
        return;
    }
    //load config
    subghz_device_cc1101_ext_reset();
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    uint32_t i = 0;
    uint8_t pa[8] = {0};
    // i+1 bound: each step reads a reg/val pair; a missing 0-terminator
    // within the table is a corrupt preset, not a full one.
    while(i + 1 < 256 && preset_data[i]) {
        cc1101_write_reg(
            subghz_device_cc1101_ext->spi_bus_handle, preset_data[i], preset_data[i + 1]);
        i += 2;
    }
    if(i + 10 > 256 || preset_data[i] != 0) {
        // No room left for the 8-byte PA table after the terminator.
        FURI_LOG_E(TAG, "load_custom_preset: missing terminator/PA table");
        furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
        return;
    }
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);

    //load pa table
    memcpy(&pa[0], &preset_data[i + 2], 8);
    subghz_device_cc1101_ext_load_patable(pa);

    //show debug
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
        i = 0;
        FURI_LOG_D(TAG, "Loading custom preset");
        while(i + 1 < 256 && preset_data[i]) {
            FURI_LOG_D(TAG, "Reg[%lu]: %02X=%02X", i, preset_data[i], preset_data[i + 1]);
            i += 2;
        }
        for(uint8_t y = i; y < i + 10; y++) {
            FURI_LOG_D(TAG, "PA[%u]:  %02X", y, preset_data[y]);
        }
    }
}

void subghz_device_cc1101_ext_load_registers(const uint8_t* data) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "load_registers: no device");
        return;
    }
    if(data == NULL) {
        FURI_LOG_E(TAG, "load_registers: NULL data");
        return;
    }
    subghz_device_cc1101_ext_reset();
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    uint32_t i = 0;
    while(i + 1 < 256 && data[i]) {
        cc1101_write_reg(subghz_device_cc1101_ext->spi_bus_handle, data[i], data[i + 1]);
        i += 2;
    }
    if(i >= 256 || data[i] != 0) {
        FURI_LOG_E(TAG, "load_registers: missing terminator");
    }
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_load_patable(const uint8_t data[8]) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "load_patable: no device");
        return;
    }
    if(data == NULL) {
        FURI_LOG_E(TAG, "load_patable: NULL data");
        return;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_set_pa_table(subghz_device_cc1101_ext->spi_bus_handle, data);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_write_packet(const uint8_t* data, uint8_t size) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "write_packet: no device");
        return;
    }
    if(data == NULL || size == 0) {
        return;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_flush_tx(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_write_reg(subghz_device_cc1101_ext->spi_bus_handle, CC1101_FIFO, size);
    cc1101_write_fifo(subghz_device_cc1101_ext->spi_bus_handle, data, size);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_flush_rx(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "flush_rx: no device");
        return;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_flush_rx(subghz_device_cc1101_ext->spi_bus_handle);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_flush_tx(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "flush_tx: no device");
        return;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_flush_tx(subghz_device_cc1101_ext->spi_bus_handle);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

bool subghz_device_cc1101_ext_rx_pipe_not_empty(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "rx_pipe_not_empty: no device");
        return false;
    }
    CC1101RxBytes status[1];
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_read_reg(
        subghz_device_cc1101_ext->spi_bus_handle,
        (CC1101_STATUS_RXBYTES) | CC1101_BURST,
        (uint8_t*)status);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
    // TODO: Find reason why RXFIFO_OVERFLOW doesnt work correctly
    if(status->NUM_RXBYTES > 0) {
        return true;
    } else {
        return false;
    }
}

bool subghz_device_cc1101_ext_is_rx_data_crc_valid(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "is_rx_data_crc_valid: no device");
        return false;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    uint8_t data[1];
    cc1101_read_reg(
        subghz_device_cc1101_ext->spi_bus_handle, CC1101_STATUS_LQI | CC1101_BURST, data);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
    if((data[0] >> 7) & 0x01) {
        return true;
    } else {
        return false;
    }
}

void subghz_device_cc1101_ext_read_packet(uint8_t* data, uint8_t* size) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "read_packet: no device");
        return;
    }
    if(data == NULL || size == NULL) {
        FURI_LOG_E(TAG, "read_packet: NULL argument");
        return;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_read_fifo(subghz_device_cc1101_ext->spi_bus_handle, data, size);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_shutdown(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "shutdown: no device");
        return;
    }
    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(0);
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    // Reset and shutdown
    cc1101_shutdown(subghz_device_cc1101_ext->spi_bus_handle);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_reset(void) {
    // May be called before alloc/begin (e.g. misordered init): never deref NULL.
    if(subghz_device_cc1101_ext == NULL || subghz_device_cc1101_ext->spi_bus_handle == NULL) {
        FURI_LOG_E(TAG, "reset: device not allocated, ignoring");
        return;
    }
    // Break-before-make first: reset() can be called mid-TX (e.g. preset
    // reload), so park the PA before touching the chip, like idle/sleep/rx.
    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(0);
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    furi_hal_gpio_init(subghz_device_cc1101_ext->g0_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    // If chip is in SPWD (deep sleep), SIDLE/SRES won't work.
    // Proper wake: CS low -> wait for MISO low -> send SIDLE -> wait IDLE -> send SRES
    cc1101_switch_to_idle(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_wait_status_state(subghz_device_cc1101_ext->spi_bus_handle, CC1101StateIDLE, 10000);
    cc1101_reset(subghz_device_cc1101_ext->spi_bus_handle);

    // Warning: push pull cc1101 clock output on GD0
    cc1101_write_reg(
        subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG0, CC1101IocfgHighImpedance);
    // Drive GDO2 low (!TX/RX). RXEN is wired to GDO2, so it must never float
    cc1101_write_reg(subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG2, CC1101IocfgHW);

    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);

    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(0);
    }
}

void subghz_device_cc1101_ext_idle(void) {
    // May be called before alloc/begin (e.g. misordered init): never deref NULL.
    if(subghz_device_cc1101_ext == NULL || subghz_device_cc1101_ext->spi_bus_handle == NULL) {
        FURI_LOG_E(TAG, "idle: device not allocated, ignoring");
        return;
    }
    // Break-before-make: Disable TX amplifier immediately
    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(0);
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_switch_to_idle(subghz_device_cc1101_ext->spi_bus_handle);
    //waiting for the chip to switch to IDLE mode
    if(!cc1101_wait_status_state(
           subghz_device_cc1101_ext->spi_bus_handle, CC1101StateIDLE, 10000)) {
        FURI_LOG_E(TAG, "CC1101 ext IDLE failed");
    }
    // Drive GDO2 low (!TX/RX). RXEN is wired to GDO2, so it must never float
    cc1101_write_reg(subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG2, CC1101IocfgHW);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

void subghz_device_cc1101_ext_rx(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "rx: no device");
        return;
    }
    // Break-before-make: Disable TX amplifier before enabling RX
    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(0);
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_switch_to_rx(subghz_device_cc1101_ext->spi_bus_handle);
    //waiting for the chip to switch to Rx mode
    if(!cc1101_wait_status_state(subghz_device_cc1101_ext->spi_bus_handle, CC1101StateRX, 10000)) {
        FURI_LOG_E(TAG, "CC1101 ext RX failed");
    }
    // Go GDO2 (!TX/RX) to high (RX state -> LNA enabled)
    cc1101_write_reg(
        subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG2, CC1101IocfgHW | CC1101_IOCFG_INV);

    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
}

bool subghz_device_cc1101_ext_tx(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "tx: no device");
        return false;
    }
    if(subghz_device_cc1101_ext->regulation != SubGhzDeviceCC1101ExtRegulationTxRx) return false;
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_switch_to_tx(subghz_device_cc1101_ext->spi_bus_handle);
    //waiting for the chip to switch to Tx mode
    if(!cc1101_wait_status_state(subghz_device_cc1101_ext->spi_bus_handle, CC1101StateTX, 10000)) {
        FURI_LOG_E(TAG, "CC1101 ext TX failed");
        furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
        return false;
    }
    // Go GDO2 (!TX/RX) to low (TX state -> LNA disabled)
    cc1101_write_reg(subghz_device_cc1101_ext->spi_bus_handle, CC1101_IOCFG2, CC1101IocfgHW);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
    // Break-before-make: Enable TX power amplifier only after LNA is disabled
    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(1);
    }
    return true;
}

float subghz_device_cc1101_ext_get_rssi(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "get_rssi: no device");
        return -127.0f;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    int32_t rssi_dec = cc1101_get_rssi(subghz_device_cc1101_ext->spi_bus_handle);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);

    float rssi = rssi_dec;
    if(rssi_dec >= 128) {
        rssi = ((rssi - 256.0f) / 2.0f) - 74.0f;
    } else {
        rssi = (rssi / 2.0f) - 74.0f;
    }

    return rssi;
}

uint8_t subghz_device_cc1101_ext_get_lqi(void) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "get_lqi: no device");
        return 0;
    }
    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    uint8_t data[1];
    cc1101_read_reg(
        subghz_device_cc1101_ext->spi_bus_handle, CC1101_STATUS_LQI | CC1101_BURST, data);
    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
    return data[0] & 0x7F;
}

bool subghz_device_cc1101_ext_is_frequency_valid(uint32_t value) {
    if(!(value >= 281000000 && value <= 361000000) &&
       !(value >= 378000000 && value <= 481000000) &&
       !(value >= 749000000 && value <= 962000000)) {
        return false;
    }

    return true;
}

SubGhzTx subghz_device_cc1101_ext_check_tx(uint32_t value) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "check_tx: no device");
        return SubGhzTxUnsupported;
    }
    // Check against extended range of YARD Stick One, no configuration would allow this frequency
    if(!subghz_device_cc1101_ext_is_frequency_valid(value)) {
        FURI_LOG_I(TAG, "Frequency blocked - outside supported range");
        return SubGhzTxUnsupported;
    }

    // Check against default range, regardless of region restrictions
    if(!subghz_device_cc1101_ext->extended_range &&
       !(value >= 299999755 && value <= 350000335) && // was increased from 348 to 350
       !(value >= 386999938 && value <= 467750000) && // was increased from 464 to 467.75
       !(value >= 778999847 && value <= 928000000)) {
        FURI_LOG_I(TAG, "Frequency blocked - outside default range");
        return SubGhzTxBlockedDefault;
    }

    // Check against region restrictions, tighter than extended and default
    if(!subghz_device_cc1101_ext->bypass_region) {
        if(!furi_hal_region_is_provisioned()) {
            FURI_LOG_I(TAG, "Frequency blocked - region not provisioned");
            return SubGhzTxBlockedRegionNotProvisioned;
        }
        if(!furi_hal_region_is_frequency_allowed(value)) {
            FURI_LOG_I(TAG, "Frequency blocked - outside region range");
            return SubGhzTxBlockedRegion;
        }
    }

    // We already checked for extended range, default range, and region range
    return SubGhzTxAllowed;
}

bool subghz_device_cc1101_ext_is_tx_allowed(uint32_t value) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "is_tx_allowed: no device");
        return false;
    }
    return subghz_device_cc1101_ext_check_tx(value) == SubGhzTxAllowed;
}

uint32_t subghz_device_cc1101_ext_set_frequency(uint32_t value) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "set_frequency: no device");
        return 0;
    }
    if(subghz_device_cc1101_ext_is_tx_allowed(value)) {
        subghz_device_cc1101_ext->regulation = SubGhzDeviceCC1101ExtRegulationTxRx;
    } else {
        subghz_device_cc1101_ext->regulation = SubGhzDeviceCC1101ExtRegulationOnlyRx;
    }

    // Park the PA before retuning: a frequency change/hop during TX would
    // otherwise transmit out-of-band while the VCO/calibration settles.
    // (Return value intentionally stays the tuned frequency: 40+ callers
    // cache it, so a 0-on-cal-fail sentinel would poison them. Failures
    // remain visible via the LOG_E lines below.)
    if(subghz_device_cc1101_ext->amp_and_leds) {
        cc1101_ext_amp_write(0);
    }

    furi_hal_spi_acquire(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_switch_to_idle(subghz_device_cc1101_ext->spi_bus_handle);
    cc1101_wait_status_state(subghz_device_cc1101_ext->spi_bus_handle, CC1101StateIDLE, 10000);

    uint32_t real_frequency =
        cc1101_set_frequency(subghz_device_cc1101_ext->spi_bus_handle, value);

    // Wait for XO to fully stabilize before calibration.
    // DIY boards with cheap/drifty crystals need 2-5ms settling time;
    // calibrating too early locks the VCO to the wrong band.
    furi_delay_ms(5);

    // First calibration pass — may catch XO mid-settle on cold starts.
    cc1101_calibrate(subghz_device_cc1101_ext->spi_bus_handle);
    if(!cc1101_wait_status_state(
           subghz_device_cc1101_ext->spi_bus_handle, CC1101StateIDLE, 10000)) {
        FURI_LOG_E(TAG, "CC1101 ext calibration pass 1 IDLE failed");
    }

    // Second calibration pass — XO is now warm and stable; result is reliable.
    cc1101_calibrate(subghz_device_cc1101_ext->spi_bus_handle);
    if(!cc1101_wait_status_state(
           subghz_device_cc1101_ext->spi_bus_handle, CC1101StateIDLE, 10000)) {
        FURI_LOG_E(TAG, "CC1101 ext calibration pass 2 IDLE failed");
    }

    if(momentum_settings.subghz_autocal) {
        cc1101_write_reg(subghz_device_cc1101_ext->spi_bus_handle, CC1101_MCSM0, 0x18);
    } else {
        cc1101_write_reg(subghz_device_cc1101_ext->spi_bus_handle, CC1101_MCSM0, 0x08);
    }

    furi_hal_spi_release(subghz_device_cc1101_ext->spi_bus_handle);
    return real_frequency;
}

static bool subghz_device_cc1101_ext_start_debug(void) {
    bool ret = false;
    if(subghz_device_cc1101_ext == NULL) {
        return false;
    }
    if(subghz_device_cc1101_ext->async_mirror_pin != NULL) {
        furi_hal_gpio_init(
            subghz_device_cc1101_ext->async_mirror_pin,
            GpioModeOutputPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh);
        ret = true;
    }
    return ret;
}

static bool subghz_device_cc1101_ext_stop_debug(void) {
    bool ret = false;
    if(subghz_device_cc1101_ext == NULL) {
        return false;
    }
    if(subghz_device_cc1101_ext->async_mirror_pin != NULL) {
        furi_hal_gpio_init(
            subghz_device_cc1101_ext->async_mirror_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        ret = true;
    }
    return ret;
}

static void subghz_device_cc1101_ext_capture_ISR(void* context) {
    UNUSED(context);
    if(subghz_device_cc1101_ext == NULL) {
        return;
    }
    if(!furi_hal_gpio_read(subghz_device_cc1101_ext->g0_pin)) {
        if(subghz_device_cc1101_ext->async_rx.capture_callback) {
            if(subghz_device_cc1101_ext->async_mirror_pin != NULL)
                furi_hal_gpio_write(subghz_device_cc1101_ext->async_mirror_pin, false);

            subghz_device_cc1101_ext->async_rx.capture_callback(
                true,
                LL_TIM_GetCounter(TIM17) << 1,
                (void*)subghz_device_cc1101_ext->async_rx.capture_callback_context);
        }
    } else {
        if(subghz_device_cc1101_ext->async_rx.capture_callback) {
            if(subghz_device_cc1101_ext->async_mirror_pin != NULL)
                furi_hal_gpio_write(subghz_device_cc1101_ext->async_mirror_pin, true);

            subghz_device_cc1101_ext->async_rx.capture_callback(
                false,
                LL_TIM_GetCounter(TIM17) << 1,
                (void*)subghz_device_cc1101_ext->async_rx.capture_callback_context);
        }
    }
    LL_TIM_SetCounter(TIM17, 4); //8>>1
}

void subghz_device_cc1101_ext_start_async_rx(
    SubGhzDeviceCC1101ExtCaptureCallback callback,
    void* context) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "start_async_rx: no device");
        return;
    }
    if(subghz_device_cc1101_ext->state != SubGhzDeviceCC1101ExtStateIdle) {
        FURI_LOG_E(TAG, "start_async_rx: not idle");
        return;
    }

    subghz_device_cc1101_ext->async_rx.capture_callback = callback;
    subghz_device_cc1101_ext->async_rx.capture_callback_context = context;

    furi_hal_bus_enable(FuriHalBusTIM17);

    // Configure TIM
    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    //Set the timer resolution to 2 us
    TIM_InitStruct.Prescaler = (64 << 1) - 1;
    TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    TIM_InitStruct.Autoreload = 0xFFFF;
    TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    LL_TIM_Init(TIM17, &TIM_InitStruct);

    // Timer: advanced
    LL_TIM_SetClockSource(TIM17, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_DisableARRPreload(TIM17);
    LL_TIM_DisableDMAReq_TRIG(TIM17);
    LL_TIM_DisableIT_TRIG(TIM17);

    furi_hal_gpio_init(
        subghz_device_cc1101_ext->g0_pin, GpioModeInterruptRiseFall, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_remove_int_callback(subghz_device_cc1101_ext->g0_pin);
    furi_hal_gpio_add_int_callback(
        subghz_device_cc1101_ext->g0_pin,
        subghz_device_cc1101_ext_capture_ISR,
        subghz_device_cc1101_ext->async_rx.capture_callback);

    // Start timer
    LL_TIM_SetCounter(TIM17, 0);
    LL_TIM_EnableCounter(TIM17);

    // Start debug
    subghz_device_cc1101_ext_start_debug();

    // Switch to RX
    subghz_device_cc1101_ext_rx();

    //Clear the variable after the end of the session
    subghz_device_cc1101_ext->async_rx.capture_delta_duration = 0;

    // Publish the state last: a concurrent stop before this point must still
    // observe Idle and take the orderly early-return path.
    subghz_device_cc1101_ext->state = SubGhzDeviceCC1101ExtStateAsyncRx;
}

void subghz_device_cc1101_ext_stop_async_rx(void) {
    if(subghz_device_cc1101_ext == NULL) {
        return;
    }
    if(subghz_device_cc1101_ext->state != SubGhzDeviceCC1101ExtStateAsyncRx) {
        FURI_LOG_E(TAG, "stop_async_rx: not in RX");
        return;
    }
    subghz_device_cc1101_ext->state = SubGhzDeviceCC1101ExtStateIdle;

    // Remove GPIO interrupt callback immediately so capture_ISR cannot fire
    furi_hal_gpio_remove_int_callback(subghz_device_cc1101_ext->g0_pin);
    subghz_device_cc1101_ext->async_rx.capture_callback = NULL;

    // Shutdown radio
    subghz_device_cc1101_ext_idle();

    FURI_CRITICAL_ENTER();
    LL_TIM_DisableCounter(TIM17);
    furi_hal_bus_disable(FuriHalBusTIM17);

    // Stop debug
    subghz_device_cc1101_ext_stop_debug();

    FURI_CRITICAL_EXIT();
    furi_hal_gpio_init(subghz_device_cc1101_ext->g0_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void subghz_device_cc1101_ext_async_tx_middleware_idle(
    SubGhzDeviceCC1101ExtAsyncTxMiddleware* middleware) {
    middleware->state = SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateIdle;
    middleware->is_odd_level = false;
    middleware->adder_duration = SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_GUARD_TIME;
}

static inline uint32_t subghz_device_cc1101_ext_async_tx_middleware_get_duration(
    SubGhzDeviceCC1101ExtAsyncTxMiddleware* middleware,
    SubGhzDeviceCC1101ExtCallback callback) {
    uint32_t ret = 0;
    bool is_level = false;
    if(subghz_device_cc1101_ext == NULL) {
        return 0;
    }

    if(middleware->state == SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateReset) return 0;

    while(1) {
        LevelDuration ld = callback(subghz_device_cc1101_ext->async_tx.callback_context);
        if(level_duration_is_reset(ld)) {
            middleware->state = SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateReset;
            if(!middleware->is_odd_level) {
                return 0;
            } else {
                return middleware->adder_duration;
            }
        } else if(level_duration_is_wait(ld)) {
            middleware->is_odd_level = !middleware->is_odd_level;
            ret = middleware->adder_duration + SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_GUARD_TIME;
            middleware->adder_duration = 0;
            return ret;
        }

        is_level = level_duration_get_level(ld);

        if(middleware->state == SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateIdle) {
            if(is_level != middleware->is_odd_level) {
                middleware->state = SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateRun;
                middleware->is_odd_level = is_level;
                middleware->adder_duration = level_duration_get_duration(ld);
                return SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_GUARD_TIME;
            } else {
                continue;
            }
        }

        if(middleware->state == SubGhzDeviceCC1101ExtAsyncTxMiddlewareStateRun) {
            if(is_level == middleware->is_odd_level) {
                middleware->adder_duration += level_duration_get_duration(ld);
                continue;
            } else {
                middleware->is_odd_level = is_level;
                ret = middleware->adder_duration;
                middleware->adder_duration = level_duration_get_duration(ld);
                return ret;
            }
        }
    }
}

static void subghz_device_cc1101_ext_async_tx_refill(uint32_t* buffer, size_t samples) {
    if(subghz_device_cc1101_ext == NULL) {
        return;
    }
    furi_assert(subghz_device_cc1101_ext->state == SubGhzDeviceCC1101ExtStateAsyncTx);

    while(samples > 0) {
        volatile uint32_t duration = subghz_device_cc1101_ext_async_tx_middleware_get_duration(
            &subghz_device_cc1101_ext->async_tx.middleware,
            subghz_device_cc1101_ext->async_tx.callback);
        if(duration == 0) {
            *buffer = 0;
            buffer++;
            samples--;
            LL_DMA_DisableIT_HT(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF);
            LL_DMA_DisableIT_TC(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF);
            if(LL_DMA_IsActiveFlag_HT3(SUBGHZ_DEVICE_CC1101_EXT_DMA)) {
                LL_DMA_ClearFlag_HT3(SUBGHZ_DEVICE_CC1101_EXT_DMA);
            }
            if(LL_DMA_IsActiveFlag_TC3(SUBGHZ_DEVICE_CC1101_EXT_DMA)) {
                LL_DMA_ClearFlag_TC3(SUBGHZ_DEVICE_CC1101_EXT_DMA);
            }
            break;
        } else {
            // Lowest possible value is 4us
            if(duration < 4) duration = 4;
            // Divide by 2 since timer resolution is 2us
            // Subtract 1 since we counting from 0
            *buffer = (duration >> 1) - 1;
            buffer++;
            samples--;
        }
    }
}

static void subghz_device_cc1101_ext_async_tx_dma_isr(void* context) {
    UNUSED(context);
    if(subghz_device_cc1101_ext == NULL) {
        return;
    }
    furi_assert(subghz_device_cc1101_ext->state == SubGhzDeviceCC1101ExtStateAsyncTx);

#if SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_CHANNEL == LL_DMA_CHANNEL_3
    if(LL_DMA_IsActiveFlag_HT3(SUBGHZ_DEVICE_CC1101_EXT_DMA)) {
        LL_DMA_ClearFlag_HT3(SUBGHZ_DEVICE_CC1101_EXT_DMA);
        subghz_device_cc1101_ext_async_tx_refill(
            subghz_device_cc1101_ext->async_tx.buffer,
            SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_HALF);
    }
    if(LL_DMA_IsActiveFlag_TC3(SUBGHZ_DEVICE_CC1101_EXT_DMA)) {
        LL_DMA_ClearFlag_TC3(SUBGHZ_DEVICE_CC1101_EXT_DMA);
        subghz_device_cc1101_ext_async_tx_refill(
            subghz_device_cc1101_ext->async_tx.buffer +
                SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_HALF,
            SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_HALF);
    }
#else
#error Update this code. Would you kindly?
#endif
}

bool subghz_device_cc1101_ext_start_async_tx(SubGhzDeviceCC1101ExtCallback callback, void* context) {
    if(subghz_device_cc1101_ext == NULL) {
        FURI_LOG_E(TAG, "start_async_tx: no device");
        return false;
    }
    if(subghz_device_cc1101_ext->state != SubGhzDeviceCC1101ExtStateIdle) {
        FURI_LOG_E(TAG, "start_async_tx: not idle");
        return false;
    }
    if(callback == NULL) {
        FURI_LOG_E(TAG, "start_async_tx: NULL callback");
        return false;
    }

    //If transmission is prohibited by regional settings
    if(subghz_device_cc1101_ext->regulation != SubGhzDeviceCC1101ExtRegulationTxRx) return false;

    subghz_device_cc1101_ext->async_tx.callback = callback;
    subghz_device_cc1101_ext->async_tx.callback_context = context;

    subghz_device_cc1101_ext->async_tx.buffer =
        malloc(SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_FULL * sizeof(uint32_t));
    if(subghz_device_cc1101_ext->async_tx.buffer == NULL) {
        FURI_LOG_E(TAG, "start_async_tx: OOM for DMA buffer");
        return false;
    }

    // State is set only after the buffer exists: DMA ISR/refill dereference
    // it unconditionally, so an early state would fault on OOM.
    subghz_device_cc1101_ext->state = SubGhzDeviceCC1101ExtStateAsyncTx;

    //Signal generation with mem-to-mem DMA
    furi_hal_gpio_write(subghz_device_cc1101_ext->g0_pin, false);
    furi_hal_gpio_init(
        subghz_device_cc1101_ext->g0_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);

    // Configure DMA  update timer
    LL_DMA_SetMemoryAddress(
        SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF, (uint32_t)subghz_device_cc1101_ext->async_tx.buffer);
    LL_DMA_SetPeriphAddress(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF, (uint32_t) & (TIM17->ARR));
    LL_DMA_ConfigTransfer(
        SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MODE_CIRCULAR | LL_DMA_PERIPH_NOINCREMENT |
            LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_WORD | LL_DMA_MDATAALIGN_WORD |
            LL_DMA_PRIORITY_VERYHIGH);
    LL_DMA_SetDataLength(
        SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF, SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_FULL);
    LL_DMA_SetPeriphRequest(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF, LL_DMAMUX_REQ_TIM17_UP);

    LL_DMA_EnableIT_TC(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF);
    LL_DMA_EnableIT_HT(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF);
    LL_DMA_EnableChannel(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF);

    furi_hal_interrupt_set_isr(
        SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_IRQ, subghz_device_cc1101_ext_async_tx_dma_isr, NULL);

    furi_hal_bus_enable(FuriHalBusTIM17);

    // Configure TIM
    // Set the timer resolution to 2 us
    LL_TIM_SetCounterMode(TIM17, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetClockDivision(TIM17, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetAutoReload(TIM17, 500);
    LL_TIM_SetPrescaler(TIM17, (64 << 1) - 1);
    LL_TIM_SetClockSource(TIM17, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_DisableARRPreload(TIM17);

    subghz_device_cc1101_ext_async_tx_middleware_idle(
        &subghz_device_cc1101_ext->async_tx.middleware);
    subghz_device_cc1101_ext_async_tx_refill(
        subghz_device_cc1101_ext->async_tx.buffer, SUBGHZ_DEVICE_CC1101_EXT_ASYNC_TX_BUFFER_FULL);

    // Configure tx gpio dma
    const GpioPin* gpio = subghz_device_cc1101_ext->g0_pin;

    subghz_device_cc1101_ext->async_tx.gpio_tx_buff[0] = (uint32_t)gpio->pin << GPIO_NUMBER;
    subghz_device_cc1101_ext->async_tx.gpio_tx_buff[1] = gpio->pin;

    LL_DMA_SetMemoryAddress(
        SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_DEF,
        (uint32_t)subghz_device_cc1101_ext->async_tx.gpio_tx_buff);
    LL_DMA_SetPeriphAddress(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_DEF, (uint32_t) & (gpio->port->BSRR));
    LL_DMA_ConfigTransfer(
        SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_DEF,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MODE_CIRCULAR | LL_DMA_PERIPH_NOINCREMENT |
            LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_WORD | LL_DMA_MDATAALIGN_WORD |
            LL_DMA_PRIORITY_HIGH);
    LL_DMA_SetDataLength(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_DEF, 2);
    LL_DMA_SetPeriphRequest(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_DEF, LL_DMAMUX_REQ_TIM17_UP);
    LL_DMA_EnableChannel(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_DEF);

    // Start debug
    if(subghz_device_cc1101_ext_start_debug()) {
        gpio = subghz_device_cc1101_ext->async_mirror_pin;
        subghz_device_cc1101_ext->async_tx.debug_gpio_buff[0] = (uint32_t)gpio->pin << GPIO_NUMBER;
        subghz_device_cc1101_ext->async_tx.debug_gpio_buff[1] = gpio->pin;

        LL_DMA_SetMemoryAddress(
            SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_DEF,
            (uint32_t)subghz_device_cc1101_ext->async_tx.debug_gpio_buff);
        LL_DMA_SetPeriphAddress(
            SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_DEF, (uint32_t) & (gpio->port->BSRR));
        LL_DMA_ConfigTransfer(
            SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_DEF,
            LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MODE_CIRCULAR | LL_DMA_PERIPH_NOINCREMENT |
                LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_WORD | LL_DMA_MDATAALIGN_WORD |
                LL_DMA_PRIORITY_LOW);
        LL_DMA_SetDataLength(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_DEF, 2);
        LL_DMA_SetPeriphRequest(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_DEF, LL_DMAMUX_REQ_TIM17_UP);
        LL_DMA_EnableChannel(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_DEF);
    }

    // Start counter
    LL_TIM_EnableDMAReq_UPDATE(TIM17);

    subghz_device_cc1101_ext_tx();

    LL_TIM_SetCounter(TIM17, 0);
    LL_TIM_EnableCounter(TIM17);

    return true;
}

bool subghz_device_cc1101_ext_is_async_tx_complete(void) {
    if(subghz_device_cc1101_ext == NULL) {
        return true; // no device: treat as idle/done
    }
    return (subghz_device_cc1101_ext->state == SubGhzDeviceCC1101ExtStateAsyncTx) &&
           (LL_TIM_GetAutoReload(TIM17) == 0);
}

void subghz_device_cc1101_ext_stop_async_tx(void) {
    if(subghz_device_cc1101_ext == NULL) {
        return;
    }
    if(subghz_device_cc1101_ext->state != SubGhzDeviceCC1101ExtStateAsyncTx) {
        FURI_LOG_E(TAG, "stop_async_tx: not in TX");
        return;
    }

    // Disable DMA ISR and channels first so no DMA interrupts fire during shutdown
    furi_hal_interrupt_set_isr(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_IRQ, NULL, NULL);
    LL_DMA_DeInit(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH3_DEF);
    LL_DMA_DeInit(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH4_DEF);

    // Stop debug
    if(subghz_device_cc1101_ext_stop_debug()) {
        LL_DMA_DeInit(SUBGHZ_DEVICE_CC1101_EXT_DMA_CH5_DEF);
    }

    // Stop and deinitialize timer
    LL_TIM_DisableCounter(TIM17);
    furi_hal_bus_disable(FuriHalBusTIM17);

    // Shutdown radio
    subghz_device_cc1101_ext_idle();

    // Deinitialize GPIO
    furi_hal_gpio_write(subghz_device_cc1101_ext->g0_pin, false);
    furi_hal_gpio_init(subghz_device_cc1101_ext->g0_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    free(subghz_device_cc1101_ext->async_tx.buffer);
    subghz_device_cc1101_ext->async_tx.buffer = NULL;
    subghz_device_cc1101_ext->async_tx.callback = NULL;

    subghz_device_cc1101_ext->state = SubGhzDeviceCC1101ExtStateIdle;
}
