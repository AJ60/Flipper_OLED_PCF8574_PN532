#include "ham_scan_worker.h"

#include <furi.h>
#include <furi_hal.h>

#include <lib/subghz/subghz_radio_device_loader.h>
#include <lib/subghz/devices/devices.h>

#define TAG "HamScanWorker"

/** Channel set snapshot shared with the UI thread. */
typedef struct {
    FuriMutex* mutex;
    uint32_t frequencies[HAM_SCAN_MAX_CHANNELS];
    uint8_t count;
} HamScanChannels;

struct HamScanWorker {
    FuriThread* thread;
    volatile bool should_work;
    volatile bool enabled;

    HamScanWorkerCallback callback;
    void* callback_context;

    const SubGhzDevice* radio_device;
    HamScanChannels channels;
};

static void ham_scan_channels_init(HamScanChannels* channels) {
    channels->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    channels->count = 0;
}

static void ham_scan_channels_deinit(HamScanChannels* channels) {
    furi_mutex_free(channels->mutex);
    channels->mutex = NULL;
}

static int32_t ham_scan_worker_thread(void* context) {
    furi_assert(context);
    HamScanWorker* instance = context;

    FURI_LOG_I(TAG, "Sweep thread start");

    subghz_devices_reset(instance->radio_device);
    subghz_devices_load_preset(instance->radio_device, FuriHalSubGhzPresetOok650Async, NULL);

    uint32_t pending[HAM_SCAN_MAX_CHANNELS];
    uint8_t pending_count = 0;

    while(instance->should_work) {
        if(!instance->enabled) {
            furi_delay_ms(HAM_SCAN_PAUSE_MS);
            continue;
        }

        furi_check(furi_mutex_acquire(instance->channels.mutex, FuriWaitForever) == FuriStatusOk);
        memcpy(pending, instance->channels.frequencies, sizeof(pending));
        pending_count = instance->channels.count;
        furi_mutex_release(instance->channels.mutex);

        if(pending_count == 0) {
            furi_delay_ms(HAM_SCAN_PAUSE_MS);
            continue;
        }

        HamScanResult result;
        memset(&result, 0, sizeof(result));
        result.channel_count = pending_count;
        result.peak_valid = false;

        float best = 0.0f;
        for(uint8_t i = 0; i < pending_count; ++i) {
            if(!instance->should_work) break;

            const uint32_t frequency = pending[i];
            result.frequencies[i] = frequency;

            if(!subghz_devices_is_frequency_valid(instance->radio_device, frequency)) {
                FURI_LOG_W(TAG, "Skipping invalid frequency %lu Hz", (unsigned long)frequency);
                result.rssi_dbm[i] = HAM_SCAN_FLOOR_DBM;
                continue;
            }

            subghz_devices_idle(instance->radio_device);
            subghz_devices_set_frequency(instance->radio_device, frequency);
            subghz_devices_flush_rx(instance->radio_device);
            subghz_devices_set_rx(instance->radio_device);

            // Let the AGC and channel filter settle before sampling.
            furi_delay_ms(HAM_SCAN_DWELL_MS);

            const float rssi = subghz_devices_get_rssi(instance->radio_device);
            result.rssi_dbm[i] = rssi;

            if(!result.peak_valid || rssi > best) {
                best = rssi;
                result.peak_channel = i;
                result.peak_valid = true;
            }

            subghz_devices_idle(instance->radio_device);
        }

        furi_delay_ms(HAM_SCAN_PAUSE_MS);

        if(instance->callback) {
            instance->callback(&result, instance->callback_context);
        }
    }

    subghz_devices_idle(instance->radio_device);
    FURI_LOG_I(TAG, "Sweep thread exit");
    return 0;
}

HamScanWorker* ham_scan_worker_alloc(const SubGhzDevice* radio_device) {
    if(radio_device == NULL) {
        FURI_LOG_E(TAG, "alloc called without a radio device");
        return NULL;
    }

    HamScanWorker* instance = malloc(sizeof(HamScanWorker));
    instance->should_work = false;
    instance->enabled = false;
    instance->callback = NULL;
    instance->callback_context = NULL;
    instance->radio_device = radio_device;

    ham_scan_channels_init(&instance->channels);

    FURI_LOG_I(TAG, "Worker alloc, radio=%s", subghz_devices_get_name(instance->radio_device));

    instance->thread = furi_thread_alloc();
    furi_thread_set_name(instance->thread, "HamScanWorker");
    furi_thread_set_stack_size(instance->thread, 2048);
    furi_thread_set_context(instance->thread, instance);
    furi_thread_set_callback(instance->thread, ham_scan_worker_thread);

    return instance;
}

void ham_scan_worker_free(HamScanWorker* instance) {
    furi_assert(instance);

    if(instance->should_work) {
        ham_scan_worker_stop(instance);
    }

    furi_thread_free(instance->thread);
    ham_scan_channels_deinit(&instance->channels);

    free(instance);
}

void ham_scan_worker_set_callback(
    HamScanWorker* instance,
    HamScanWorkerCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

void ham_scan_worker_set_channels(
    HamScanWorker* instance,
    const uint32_t* frequencies,
    uint8_t count) {
    furi_assert(instance);
    furi_assert(count <= HAM_SCAN_MAX_CHANNELS);

    furi_check(furi_mutex_acquire(instance->channels.mutex, FuriWaitForever) == FuriStatusOk);

    if(frequencies == NULL) {
        instance->channels.count = 0;
    } else {
        uint8_t copied = MIN(count, HAM_SCAN_MAX_CHANNELS);
        for(uint8_t i = 0; i < copied; ++i) {
            instance->channels.frequencies[i] = frequencies[i];
        }
        instance->channels.count = copied;
    }

    furi_mutex_release(instance->channels.mutex);
    FURI_LOG_D(TAG, "Channel set updated, count=%u", instance->channels.count);
}

void ham_scan_worker_set_enabled(HamScanWorker* instance, bool enabled) {
    furi_assert(instance);
    instance->enabled = enabled;
}

void ham_scan_worker_start(HamScanWorker* instance) {
    furi_assert(instance);
    furi_assert(instance->should_work == false);

    instance->should_work = true;
    furi_thread_start(instance->thread);
}

void ham_scan_worker_stop(HamScanWorker* instance) {
    furi_assert(instance);
    furi_assert(instance->should_work == true);

    instance->should_work = false;
    furi_thread_join(instance->thread);
    instance->should_work = false;
}
