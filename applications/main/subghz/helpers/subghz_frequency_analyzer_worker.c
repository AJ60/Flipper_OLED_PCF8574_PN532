#include "subghz_frequency_analyzer_worker.h"
#include <lib/drivers/cc1101.h>

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <furi_hal_spi_config.h>
#include <momentum/settings.h>
#include <float_tools.h>

#define TAG "SubghzFrequencyAnalyzerWorker"

#define SUBGHZ_FREQUENCY_ANALYZER_THRESHOLD -97.0f

static const uint8_t subghz_preset_ook_58khz[][2] = {
    {CC1101_MDMCFG4, 0b11110111}, // Rx BW filter is 58.035714kHz
    {CC1101_FREND1, 0xB6},
    /* End  */
    {0, 0},
};

static const uint8_t subghz_preset_ook_650khz[][2] = {
    {CC1101_MDMCFG4, 0b00010111}, // Rx BW filter is 650.000kHz
    {CC1101_FREND1, 0xB6},
    /* End  */
    {0, 0},
};

struct SubGhzFrequencyAnalyzerWorker {
    FuriThread* thread;

    volatile bool worker_running;
    uint8_t sample_hold_counter;
    FrequencyRSSI frequency_rssi_buf;
    SubGhzSetting* setting;

    float filVal;
    float trigger_level;

    SubGhzFrequencyAnalyzerWorkerPairCallback pair_callback;
    void* context;
    bool is_ext_radio;
    const FuriHalSpiBusHandle* spi_handle;
    SubGhz* subghz; // Back-pointer for device-switch safety check
};

static void subghz_frequency_analyzer_worker_load_registers(
    const FuriHalSpiBusHandle* spi_handle,
    const uint8_t data[][2]) {
    furi_hal_spi_acquire(spi_handle);
    size_t i = 0;
    while(data[i][0]) {
        cc1101_write_reg(spi_handle, data[i][0], data[i][1]);
        i++;
    }
    furi_hal_spi_release(spi_handle);
}

static void subghz_frequency_analyzer_worker_idle(SubGhzFrequencyAnalyzerWorker* instance) {
    if(instance->is_ext_radio) {
        furi_hal_spi_acquire(instance->spi_handle);
        cc1101_switch_to_idle(instance->spi_handle);
        cc1101_wait_status_state(instance->spi_handle, CC1101StateIDLE, 10000);
        furi_hal_spi_release(instance->spi_handle);
    } else {
        furi_hal_subghz_idle();
    }
}

// Convert raw CC1101 RSSI register value to dBm.
// Shared between internal and external radio paths to avoid formula drift.
static float subghz_frequency_analyzer_worker_rssi_to_dbm(int32_t rssi_dec) {
    float rssi = rssi_dec;
    if(rssi_dec >= 128) {
        rssi = ((rssi - 256.0f) / 2.0f) - 74.0f;
    } else {
        rssi = (rssi / 2.0f) - 74.0f;
    }
    return rssi;
}

static float subghz_frequency_analyzer_worker_get_rssi(SubGhzFrequencyAnalyzerWorker* instance) {
    if(instance->is_ext_radio) {
        furi_hal_spi_acquire(instance->spi_handle);
        int32_t rssi_dec = cc1101_get_rssi(instance->spi_handle);
        furi_hal_spi_release(instance->spi_handle);
        return subghz_frequency_analyzer_worker_rssi_to_dbm(rssi_dec);
    } else {
        return furi_hal_subghz_get_rssi();
    }
}

// running average with adaptive coefficient
static uint32_t subghz_frequency_analyzer_worker_expRunningAverageAdaptive(
    SubGhzFrequencyAnalyzerWorker* instance,
    uint32_t newVal) {
    float k;
    float newValFloat = newVal;
    // the sharpness of the filter depends on the absolute value of the difference
    if(fabsf(newValFloat - instance->filVal) > 500000.f)
        k = 0.9;
    else
        k = 0.03;

    instance->filVal += (newValFloat - instance->filVal) * k;
    return (uint32_t)instance->filVal;
}

/** Drive the E07 module's TXEN (PA5 = gpio_ext_pc3) low.
 *
 * TXEN must never float: analog mode leaves the module's TX enable undefined,
 * and TXEN high with RXEN low would enable the PA. Driven low = module idle.
 */
static void subghz_frequency_analyzer_txen_off(void) {
    furi_hal_gpio_init_simple(&gpio_ext_pc3, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pc3, false);
}

/** Worker thread
 * 
 * @param context 
 * @return exit code 
 */
static int32_t subghz_frequency_analyzer_worker_thread(void* context) {
    SubGhzFrequencyAnalyzerWorker* instance = context;

    FrequencyRSSI frequency_rssi = {
        .frequency_coarse = 0, .rssi_coarse = 0, .frequency_fine = 0, .rssi_fine = 0};
    float rssi = 0;
    uint32_t frequency = 0;
    float rssi_temp = 0;
    uint32_t frequency_temp = 0;

    // NOTE: This worker uses direct SPI register access instead of the SubGhzDevice
    // abstraction for performance reasons. The frequency analyzer must rapidly scan
    // dozens of frequencies per second, and the abstraction overhead (mutex, state
    // tracking, function call indirection) would be too high. SPI bus contention is
    // still protected via furi_hal_spi_acquire/release.
    const FuriHalSpiBusHandle* spi_handle = instance->spi_handle;

    if(instance->is_ext_radio) {
        subghz_frequency_analyzer_txen_off();

        furi_hal_spi_bus_handle_init(spi_handle);

        // Put internal CC1101 to sleep to prevent cross-talk / power load
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();

        furi_hal_spi_acquire(spi_handle);
        CC1101Status status = cc1101_reset(spi_handle);
        if(status.CHIP_RDYn != 0) {
            furi_hal_spi_release(spi_handle);
            furi_hal_spi_bus_handle_deinit(spi_handle);
            subghz_frequency_analyzer_txen_off();
            instance->worker_running = false;
            FURI_LOG_W(TAG, "External CC1101 not detected or not responding");
            return -1;
        }
        cc1101_flush_rx(spi_handle);
        cc1101_flush_tx(spi_handle);
        cc1101_write_reg(spi_handle, CC1101_IOCFG0, CC1101IocfgHW);
        // GDO2 inverted: Assert HIGH in RX state to enable RX LNA on E07-433M20S
        cc1101_write_reg(spi_handle, CC1101_IOCFG2, CC1101IocfgHW | CC1101_IOCFG_INV);
        cc1101_write_reg(spi_handle, CC1101_MDMCFG3, 0b01111111); // symbol rate
        cc1101_write_reg(spi_handle, CC1101_FREND1, 0xB6);
        cc1101_write_reg(
            spi_handle,
            CC1101_AGCCTRL2,
            0b00000111); // 00 - DVGA all; 000 - MAX LNA+LNA2; 111 - MAGN_TARGET 42 dB
        cc1101_write_reg(
            spi_handle,
            CC1101_AGCCTRL1,
            0b00001000); // 0; 0 - LNA 2 gain is decreased to minimum before decreasing LNA gain; 00 - Relative carrier sense threshold disabled; 1000 - Absolute carrier sense threshold disabled
        cc1101_write_reg(
            spi_handle,
            CC1101_AGCCTRL0,
            0b00110000); // 00 - No hysteresis, medium asymmetric dead zone, medium gain ; 11 - 64 samples agc; 00 - Normal AGC, 00 - 4dB boundary
        furi_hal_spi_release(spi_handle);
    } else {
        subghz_frequency_analyzer_txen_off();

        // Start Internal CC1101
        furi_hal_subghz_reset();

        furi_hal_spi_acquire(spi_handle);
        cc1101_flush_rx(spi_handle);
        cc1101_flush_tx(spi_handle);
        cc1101_write_reg(spi_handle, CC1101_IOCFG0, CC1101IocfgHW);
        cc1101_write_reg(spi_handle, CC1101_MDMCFG3,
                         0b01111111); // symbol rate
        cc1101_write_reg(spi_handle, CC1101_FREND1, 0xB6);
        cc1101_write_reg(
            spi_handle,
            CC1101_AGCCTRL2,
            0b00000111); // 00 - DVGA all; 000 - MAX LNA+LNA2; 111 - MAGN_TARGET 42 dB
        cc1101_write_reg(
            spi_handle,
            CC1101_AGCCTRL1,
            0b00001000); // 0; 0 - LNA 2 gain is decreased to minimum before decreasing LNA gain; 00 - Relative carrier sense threshold disabled; 1000 - Absolute carrier sense threshold disabled
        cc1101_write_reg(
            spi_handle,
            CC1101_AGCCTRL0,
            0b00110000); // 00 - No hysteresis, medium asymmetric dead zone, medium gain ; 11 - 64 samples agc; 00 - Normal AGC, 00 - 4dB boundary

        furi_hal_spi_release(spi_handle);

        furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);
    }

    while(instance->worker_running) {
        // Safety check: if the SubGhz instance or its txrx is NULL (e.g. during a
        // device switch or app shutdown), stop the worker immediately.
        if(instance->subghz == NULL || instance->subghz->txrx == NULL) {
            FURI_LOG_W(TAG, "SubGhz instance invalid during analysis — stopping worker");
            break;
        }

        furi_delay_ms(10);

        float rssi_min = 26.0f;
        float rssi_avg = 0;
        size_t rssi_avg_samples = 0;

        frequency_rssi.rssi_coarse = -127.0f;
        frequency_rssi.rssi_fine = -127.0f;
        subghz_frequency_analyzer_worker_idle(instance);
        subghz_frequency_analyzer_worker_load_registers(spi_handle, subghz_preset_ook_650khz);

        // First stage: coarse scan
        for(size_t i = 0; i < subghz_setting_get_frequency_count(instance->setting); i++) {
            if(!instance->worker_running) break;
            uint32_t current_frequency = subghz_setting_get_frequency(instance->setting, i);
            if(furi_hal_subghz_is_frequency_valid(current_frequency) &&
               (((current_frequency != 462750000) && (current_frequency != 467750000) &&
                 (current_frequency != 464000000)) &&
                (current_frequency <= 920000000))) {
                furi_hal_spi_acquire(spi_handle);
                cc1101_switch_to_idle(spi_handle);
                frequency = cc1101_set_frequency(
                    spi_handle,
                    subghz_setting_get_frequency(instance->setting, i));

                cc1101_calibrate(spi_handle);

                cc1101_wait_status_state(
                    spi_handle, CC1101StateIDLE, 10000);

                cc1101_switch_to_rx(spi_handle);
                furi_hal_spi_release(spi_handle);

                furi_delay_ms(2);

                rssi = subghz_frequency_analyzer_worker_get_rssi(instance);

                rssi_avg += rssi;
                rssi_avg_samples++;

                if(rssi < rssi_min) rssi_min = rssi;

                if(frequency_rssi.rssi_coarse < rssi) {
                    frequency_rssi.rssi_coarse = rssi;
                    frequency_rssi.frequency_coarse = frequency;
                }
            }
        }

        FURI_LOG_T(
            TAG,
            "RSSI: avg %f, max %f at %lu, min %f",
            (double)(rssi_avg / rssi_avg_samples),
            (double)frequency_rssi.rssi_coarse,
            frequency_rssi.frequency_coarse,
            (double)rssi_min);

        // Second stage: fine scan
        if(frequency_rssi.rssi_coarse > instance->trigger_level && instance->worker_running) {
            subghz_frequency_analyzer_worker_idle(instance);
            subghz_frequency_analyzer_worker_load_registers(spi_handle, subghz_preset_ook_58khz);
            //for example -0.3 ... 433.92 ... +0.3 step 20KHz
            for(uint32_t i = frequency_rssi.frequency_coarse - 300000;
                i < frequency_rssi.frequency_coarse + 300000;
                i += 20000) {
                if(!instance->worker_running) break;
                if(furi_hal_subghz_is_frequency_valid(i)) {
                    furi_hal_spi_acquire(spi_handle);
                    cc1101_switch_to_idle(spi_handle);
                    frequency = cc1101_set_frequency(spi_handle, i);

                    cc1101_calibrate(spi_handle);

                    cc1101_wait_status_state(
                        spi_handle, CC1101StateIDLE, 10000);

                    cc1101_switch_to_rx(spi_handle);
                    furi_hal_spi_release(spi_handle);

                    furi_delay_ms(2);

                    rssi = subghz_frequency_analyzer_worker_get_rssi(instance);

                    FURI_LOG_T(TAG, "#:%lu:%f", frequency, (double)rssi);

                    if(frequency_rssi.rssi_fine < rssi) {
                        frequency_rssi.rssi_fine = rssi;
                        frequency_rssi.frequency_fine = frequency;
                    }
                }
            }
        }

        // Deliver results fine
        if(frequency_rssi.rssi_fine > instance->trigger_level) {
            FURI_LOG_D(
                TAG, "=:%lu:%f", frequency_rssi.frequency_fine, (double)frequency_rssi.rssi_fine);

            instance->sample_hold_counter = 20;
            rssi_temp = frequency_rssi.rssi_fine;
            frequency_temp = frequency_rssi.frequency_fine;

            if(!float_is_equal(instance->filVal, 0.f)) {
                frequency_rssi.frequency_fine =
                    subghz_frequency_analyzer_worker_expRunningAverageAdaptive(
                        instance, frequency_rssi.frequency_fine);
            }
            // Deliver callback
            if(instance->pair_callback) {
                instance->pair_callback(
                    instance->context,
                    frequency_rssi.frequency_fine,
                    frequency_rssi.rssi_fine,
                    true);
            }
        } else if( // Deliver results coarse
            (frequency_rssi.rssi_coarse > instance->trigger_level) &&
            (instance->sample_hold_counter < 10)) {
            FURI_LOG_D(
                TAG,
                "~:%lu:%f",
                frequency_rssi.frequency_coarse,
                (double)frequency_rssi.rssi_coarse);

            instance->sample_hold_counter = 20;
            rssi_temp = frequency_rssi.rssi_coarse;
            frequency_temp = frequency_rssi.frequency_coarse;
            if(!float_is_equal(instance->filVal, 0.f)) {
                frequency_rssi.frequency_coarse =
                    subghz_frequency_analyzer_worker_expRunningAverageAdaptive(
                        instance, frequency_rssi.frequency_coarse);
            }
            // Deliver callback
            if(instance->pair_callback) {
                instance->pair_callback(
                    instance->context,
                    frequency_rssi.frequency_coarse,
                    frequency_rssi.rssi_coarse,
                    true);
            }
        } else {
            if(instance->sample_hold_counter > 0) {
                instance->sample_hold_counter--;
                if(instance->sample_hold_counter == 18) {
                    if(instance->pair_callback) {
                        instance->pair_callback(
                            instance->context, frequency_temp, rssi_temp, false);
                    }
                }
            } else {
                instance->filVal = 0;
                if(instance->pair_callback)
                    instance->pair_callback(instance->context, 0, 0, false);
            }
        }
    }

    //Stop CC1101
    if(instance->is_ext_radio) {
        furi_hal_spi_acquire(spi_handle);
        cc1101_switch_to_idle(spi_handle);
        cc1101_write_reg(spi_handle, CC1101_IOCFG0, CC1101IocfgHighImpedance);
        // GDO2 drives the module's RXEN: keep it driven low, never tri-stated
        cc1101_write_reg(spi_handle, CC1101_IOCFG2, CC1101IocfgHW);
        cc1101_shutdown(spi_handle);
        furi_hal_spi_release(spi_handle);
        furi_hal_spi_bus_handle_deinit(spi_handle);
        subghz_frequency_analyzer_txen_off();
    } else {
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
    }

    instance->worker_running = false;
    return 0;
}

SubGhzFrequencyAnalyzerWorker* subghz_frequency_analyzer_worker_alloc(void* context) {
    furi_assert(context);
    SubGhzFrequencyAnalyzerWorker* instance = malloc(sizeof(SubGhzFrequencyAnalyzerWorker));
    if(instance == NULL) {
        return NULL;
    }
    SubGhz* subghz = context;
    if(subghz->txrx == NULL) {
        FURI_LOG_E(TAG, "analyzer alloc: no txrx");
        free(instance);
        return NULL;
    }

    instance->thread = furi_thread_alloc_ex(
        "SubGhzFAWorker", 2048, subghz_frequency_analyzer_worker_thread, instance);
    instance->subghz = subghz;
    instance->setting = subghz_txrx_get_setting(subghz->txrx);
    instance->trigger_level = subghz->last_settings->frequency_analyzer_trigger;
    //instance->trigger_level = SUBGHZ_FREQUENCY_ANALYZER_THRESHOLD;
    instance->is_ext_radio =
        (subghz_txrx_radio_device_get(subghz->txrx) != SubGhzRadioDeviceTypeInternal);
    if(instance->is_ext_radio) {
        // When using external radio, the analyzer drives the external SPI handle.
        // The mapping matches cc1101_ext.c: SpiDefault -> external_extra (PA4),
        // !SpiDefault -> external (PC3).
        instance->spi_handle = (momentum_settings.spi_cc1101_handle == SpiDefault ?
                                    &furi_hal_spi_bus_handle_external_extra :
                                    &furi_hal_spi_bus_handle_external);
    } else {
        // When using internal radio, the analyzer drives the internal SPI handle.
        instance->spi_handle = &furi_hal_spi_bus_handle_subghz;
    }
    return instance;
}

void subghz_frequency_analyzer_worker_free(SubGhzFrequencyAnalyzerWorker* instance) {
    if(instance == NULL) {
        return;
    }

    furi_thread_free(instance->thread);
    free(instance);
}

void subghz_frequency_analyzer_worker_set_pair_callback(
    SubGhzFrequencyAnalyzerWorker* instance,
    SubGhzFrequencyAnalyzerWorkerPairCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(context);
    instance->pair_callback = callback;
    instance->context = context;
}

void subghz_frequency_analyzer_worker_start(SubGhzFrequencyAnalyzerWorker* instance) {
    if(instance == NULL) {
        return;
    }
    if(!instance->worker_running) {
        instance->worker_running = true;
        furi_thread_start(instance->thread);
    }
}

void subghz_frequency_analyzer_worker_stop(SubGhzFrequencyAnalyzerWorker* instance) {
    if(instance == NULL) {
        return;
    }
    if(instance->worker_running) {
        instance->worker_running = false;
        furi_thread_join(instance->thread);
    }
}

bool subghz_frequency_analyzer_worker_is_running(SubGhzFrequencyAnalyzerWorker* instance) {
    if(instance == NULL) {
        return false;
    }
    return instance->worker_running;
}

void subghz_frequency_analyzer_worker_set_trigger_level(
    SubGhzFrequencyAnalyzerWorker* instance,
    float value) {
    if(instance == NULL) {
        return;
    }
    instance->trigger_level = value;
}

float subghz_frequency_analyzer_worker_get_trigger_level(SubGhzFrequencyAnalyzerWorker* instance) {
    if(instance == NULL) {
        return (float)SUBGHZ_FREQUENCY_ANALYZER_THRESHOLD;
    }
    return instance->trigger_level;
}

uint32_t subghz_frequency_analyzer_get_nearest_frequency(
    SubGhzFrequencyAnalyzerWorker* instance,
    uint32_t input) {
    if(instance == NULL) {
        return 0;
    }
    uint32_t prev_freq = 0;
    uint32_t result = 0;
    uint32_t current;

    for(size_t i = 0; i < subghz_setting_get_frequency_count(instance->setting); i++) {
        current = subghz_setting_get_frequency(instance->setting, i);
        if(current == 0) {
            continue;
        }
        if(current == input) {
            result = current;
            break;
        }
        if(current > input && prev_freq < input) {
            if(current - input < input - prev_freq) {
                result = current;
            } else {
                result = prev_freq;
            }
            break;
        }
        prev_freq = current;
    }

    return result;
}
