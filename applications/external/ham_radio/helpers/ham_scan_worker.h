#pragma once

#include "ham_band.h"

#include <lib/subghz/devices/devices.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAM_SCAN_MAX_CHANNELS (32)
#define HAM_SCAN_DWELL_MS     (6)
#define HAM_SCAN_PAUSE_MS     (60)

/** Signal strength below which a channel is drawn as noise floor. */
#define HAM_SCAN_FLOOR_DBM    (-100.0f)
/** Strength above which the channel is highlighted as occupied. */
#define HAM_SCAN_PEAK_DBM     (-60.0f)

/** One completed sweep of every receive frequency in the plan. */
typedef struct {
    uint32_t frequencies[HAM_SCAN_MAX_CHANNELS];
    float rssi_dbm[HAM_SCAN_MAX_CHANNELS];
    uint8_t channel_count;
    uint8_t peak_channel;
    bool peak_valid;
} HamScanResult;

typedef struct HamScanWorker HamScanWorker;

/** Invoked from the worker thread after each full sweep. */
typedef void (*HamScanWorkerCallback)(const HamScanResult* result, void* context);

/** alloc with a borrowed radio handle: the caller owns the device and its
 * subghz_devices_init()/deinit() pairing, so sweeping and tuning share one
 * transceiver instead of fighting over two handles. */
HamScanWorker* ham_scan_worker_alloc(const SubGhzDevice* radio_device);
void ham_scan_worker_free(HamScanWorker* instance);

void ham_scan_worker_set_callback(
    HamScanWorker* instance,
    HamScanWorkerCallback callback,
    void* context);

/** Replace the channel set. Safe to call while the worker is running: the new
 * frequencies are picked up on the next sweep. */
void ham_scan_worker_set_channels(HamScanWorker* instance, const uint32_t* frequencies, uint8_t count);

void ham_scan_worker_set_enabled(HamScanWorker* instance, bool enabled);

void ham_scan_worker_start(HamScanWorker* instance);
void ham_scan_worker_stop(HamScanWorker* instance);

#ifdef __cplusplus
}
#endif
