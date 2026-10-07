#pragma once

#include "lib/hardware/subghz/SubGhzPayload.hpp"
#include <functional>

#include <furi.h>
#include <furi_hal.h>

#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/subghz_radio_device_global.h>

#include "SubGhzState.hpp"
#include "data/SubGhzReceivedDataImpl.hpp"

#include "lib/hardware/notification/Notification.hpp"

using namespace std;

#undef LOG_TAG
#define LOG_TAG "SUB_GHZ"

class SubGhzModule {
private:
    SubGhzEnvironment* environment = nullptr;
    const SubGhzDevice* device = nullptr;
    SubGhzReceiver* receiver = nullptr;
    SubGhzWorker* worker = nullptr;
    SubGhzTransmitter* transmitter = nullptr;
    function<void(SubGhzReceivedData*)> receiveHandler = nullptr;
    FuriTimer* txCompleteCheckTimer = nullptr;
    function<void()> txCompleteHandler = nullptr;
    int repeatsLeft = 0;
    SubGhzPayload* currentPayload = nullptr;
    uint32_t receiveFrequency = 0;

    bool isExternal = false;
    SubGhzState state = IDLE;
    bool receiveAfterTransmission = false;
    // True once subghz_devices_begin() succeeded. All RX/TX/idle paths
    // require it; without it the device pointer must be treated as unusable.
    bool begun = false;
    // True only if this instance enabled OTG itself: destructor disables OTG
    // only in that case, never killing a pre-existing OTG session.
    bool otgWeEnabled = false;

    static void captureCallback(SubGhzReceiver* receiver, SubGhzProtocolDecoderBase* decoderBase, void* context) {
        UNUSED(receiver);

        if(context == NULL) {
            return;
        }

        SubGhzModule* subghz = (SubGhzModule*)context;
        if(subghz->receiveHandler != NULL) {
            subghz->receiveHandler(new SubGhzReceivedDataImpl(decoderBase, subghz->receiveFrequency));
        }
    }

    static void txCompleteCheckCallback(void* context) {
        SubGhzModule* subghz = (SubGhzModule*)context;
        if(subghz == nullptr || subghz->device == nullptr || !subghz->begun) {
            return;
        }
        if(subghz_devices_is_async_complete_tx(subghz->device)) {
            if(subghz->repeatsLeft-- > 0 && subghz->currentPayload != NULL) {
                subghz->startTransmission(0);
                return;
            }

            if(subghz->txCompleteCheckTimer != NULL) {
                furi_timer_stop(subghz->txCompleteCheckTimer);
            }

            if(subghz->txCompleteHandler != NULL) {
                subghz->txCompleteHandler();
            } else {
                subghz->DefaultAfterTransmissionHandler();
            }
        }
    }

    // Largest single heap block the RX path needs: worker stream
    // (~8KB after the 2048-entry halving) + decoder burst headroom. Total
    // free is irrelevant; max_free_block is what matters (cf. preload check).
    static const size_t RX_MIN_FREE_BLOCK = 24U * 1024U;

    bool prepareReceiver() {
        if(memmgr_heap_get_max_free_block() < RX_MIN_FREE_BLOCK) {
            FURI_LOG_E(LOG_TAG, "prepareReceiver: low memory, need 24KB block");
            return false;
        }
        receiver = subghz_receiver_alloc_init(environment);
        if(receiver == nullptr) {
            FURI_LOG_E(LOG_TAG, "prepareReceiver: receiver alloc failed");
            return false;
        }
        subghz_receiver_set_filter(receiver, SubGhzProtocolFlag_Decodable);
        subghz_receiver_set_rx_callback(receiver, captureCallback, this);

        worker = subghz_worker_alloc();
        if(worker == nullptr) {
            FURI_LOG_E(LOG_TAG, "prepareReceiver: worker alloc failed");
            subghz_receiver_free(receiver);
            receiver = nullptr;
            return false;
        }
        subghz_worker_set_overrun_callback(worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
        subghz_worker_set_pair_callback(worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
        subghz_worker_set_context(worker, receiver);
        return true;
    }

    void setFrequencyIgnoringStateChecks(uint32_t frequency) {
        if(device != nullptr && subghz_devices_is_frequency_valid(device, frequency)) {
            subghz_devices_idle(device);
            subghz_devices_set_frequency(device, frequency);
        }
    }

public:
    SubGhzModule(uint32_t frequency) {
        environment = subghz_environment_alloc();
        furi_check(environment);
        subghz_environment_set_protocol_registry(environment, &subghz_protocol_registry);

        subghz_devices_init();

        // Honor the global module selection persisted by main SubGHz Radio
        // Settings (last_subghz.settings RadioDevice: 1=Internal, 2=External).
        // External selected => use EXT or fail loudly (device stays nullptr,
        // IsReady() false); never silently fall back to internal.
        uint32_t global = subghz_global_radio_device_load();
        bool wantExternal = (global == SUBGHZ_GLOBAL_RADIO_DEVICE_EXTERNAL);

        if(wantExternal) {
            if(!furi_hal_power_is_otg_enabled()) {
                furi_hal_power_enable_otg();
                otgWeEnabled = true;
                // E07 needs a few ms after VCC before CC1101 drives MISO.
                furi_delay_ms(10);
            }
            device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
            if(device && subghz_devices_is_connect(device)) {
                isExternal = true;
            } else {
                FURI_LOG_E(LOG_TAG, "External selected but CC1101 not detected");
                if(otgWeEnabled) {
                    furi_hal_power_disable_otg();
                    otgWeEnabled = false;
                }
                device = nullptr;
                isExternal = false;
            }
        } else {
            device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
            isExternal = false;
        }

        if(device != nullptr) {
            // begin() allocates/initializes the driver and must come BEFORE
            // reset/idle: calling reset/idle first dereferences an
            // uninitialized driver and hard-faults.
            if(subghz_devices_begin(device)) {
                begun = true;
                subghz_devices_reset(device);
                subghz_devices_idle(device);
                subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);
            } else {
                FURI_LOG_E(LOG_TAG, "SubGhzModule: device begin failed");
                device = nullptr;
            }
        }

        FURI_LOG_I(LOG_TAG, "SubGhzModule: calling SetReceiveFrequency(%lu)", frequency);
        SetReceiveFrequency(frequency);
        FURI_LOG_I(LOG_TAG, "SubGhzModule: SetReceiveFrequency done");

        txCompleteCheckTimer = furi_timer_alloc(txCompleteCheckCallback, FuriTimerTypePeriodic, this);
        furi_check(txCompleteCheckTimer);
        FURI_LOG_I(LOG_TAG, "SubGhzModule: constructor completed");
    }

    bool IsReady() {
        return begun && device != nullptr;
    }

    void SetReceiveFrequency(uint32_t frequency) {
        if(device == nullptr || !begun) {
            // No usable radio (e.g. External selected but absent): remember
            // the frequency so a later retry can apply it, but touch nothing.
            receiveFrequency = frequency;
            return;
        }
        if(receiveFrequency == frequency) {
            return;
        } else {
            receiveFrequency = frequency;
        }

        bool restoreReceive = state == RECEIVING;
        PutToIdle();

        setFrequencyIgnoringStateChecks(frequency);

        if(restoreReceive) {
            ReceiveAsync();
        }
    }

    void SetReceiveAfterTransmission(bool value) {
        this->receiveAfterTransmission = value;
    }

    void DefaultAfterTransmissionHandler() {
        if(receiveAfterTransmission) {
            ReceiveAsync();
        } else {
            PutToIdle();
        }
    }

    void SetReceiveHandler(function<void(SubGhzReceivedData*)> handler) {
        receiveHandler = handler;
    }

    bool ReceiveAsync() {
        FURI_LOG_I(LOG_TAG, "ReceiveAsync: entering, state=%d", state);
        if(device == nullptr || !begun) {
            FURI_LOG_E(LOG_TAG, "ReceiveAsync: no radio device, ignoring");
            return false;
        }
        if(receiver == NULL) {
            if(!prepareReceiver()) {
                state = IDLE;
                return false;
            }
        }

        PutToIdle();

        setFrequencyIgnoringStateChecks(receiveFrequency);

        if(device != nullptr && worker != nullptr) {
            FURI_LOG_I(LOG_TAG, "ReceiveAsync: starting RX worker");
            subghz_devices_flush_rx(device);
            subghz_devices_start_async_rx(device, (void*)subghz_worker_rx_callback, worker);
            subghz_worker_start(worker);
            FURI_LOG_I(LOG_TAG, "ReceiveAsync: RX worker started");
            state = RECEIVING;
        } else {
            state = IDLE;
            return false;
        }
        FURI_LOG_I(LOG_TAG, "ReceiveAsync: completed");
        return true;
    }

    void SetTransmitCompleteHandler(function<void()> txCompleteHandler) {
        this->txCompleteHandler = txCompleteHandler;
    }

    void Transmit(SubGhzPayload* payload, uint32_t frequency) {
        if(device == nullptr || !begun) {
            FURI_LOG_E(LOG_TAG, "Transmit: no radio device, dropping payload");
            delete payload;
            return;
        }
        if(state != TRANSMITTING) {
            PutToIdle();
            state = TRANSMITTING;
        } else {
            if(txCompleteCheckTimer != nullptr) {
                furi_timer_stop(txCompleteCheckTimer);
            }
            if(currentPayload != nullptr) {
                delete currentPayload;
                currentPayload = nullptr;
            }
        }

        Notification::Play(&sequence_blink_start_magenta);

        currentPayload = payload;
        repeatsLeft = payload->GetRequiredSofwareRepeats() - 1;

        startTransmission(frequency);

        uint32_t interval = furi_kernel_get_tick_frequency() / 100; // every 10 ms
        if(txCompleteCheckTimer != nullptr) {
            furi_timer_start(txCompleteCheckTimer, interval);
        }
    }

private:
    void startTransmission(uint32_t frequency) {
        stopTransmission();

        if(frequency > 0) {
            setFrequencyIgnoringStateChecks(frequency);
        }

        if(currentPayload != nullptr && environment != nullptr && device != nullptr) {
            transmitter = subghz_transmitter_alloc_init(environment, currentPayload->GetProtocol());
            subghz_transmitter_deserialize(transmitter, currentPayload->GetFlipperFormat());
            subghz_devices_flush_tx(device);
            subghz_devices_start_async_tx(device, (void*)subghz_transmitter_yield, transmitter);
        }
    }

    void stopTransmission() {
        if(transmitter != NULL) {
            if(device != nullptr) {
                subghz_devices_stop_async_tx(device);
            }
            subghz_transmitter_free(transmitter);
            transmitter = NULL;
        }
    }

public:
    void StopReceive() {
        if(worker != nullptr && subghz_worker_is_running(worker)) {
            subghz_worker_stop(worker);
        }
        if(device != nullptr) {
            subghz_devices_stop_async_rx(device);
            subghz_devices_idle(device);
        }
        state = IDLE;
    }

    void StopTranmit() {
        Notification::Play(&sequence_blink_stop);

        repeatsLeft = 0;
        if(currentPayload != nullptr) {
            delete currentPayload;
            currentPayload = nullptr;
        }

        if(txCompleteCheckTimer != nullptr) {
            furi_timer_stop(txCompleteCheckTimer);
        }
        stopTransmission();
        if(device != nullptr) {
            subghz_devices_idle(device);
        }

        state = IDLE;
    }

    void PutToIdle() {
        switch(state) {
        case RECEIVING:
            StopReceive();
            break;

        case TRANSMITTING:
            StopTranmit();
            break;

        default:
        case IDLE:
            break;
        }

        if(device != nullptr && begun) {
            subghz_devices_idle(device);
        }
    }

    bool IsExternal() {
        return isExternal;
    }

    ~SubGhzModule() {
        FURI_LOG_I(LOG_TAG, "SubGhzModule: destructor enter");
        // Unhook first so the RX worker callback cannot fire during teardown.
        receiveHandler = nullptr;
        PutToIdle();

        // Ensure worker is fully stopped before freeing — PutToIdle() calls
        // subghz_worker_stop() which waits, but be explicit about the invariant.
        furi_check(!(worker != nullptr && subghz_worker_is_running(worker)));

        if(txCompleteCheckTimer != NULL) {
            furi_timer_free(txCompleteCheckTimer);
            txCompleteCheckTimer = NULL;
        }

        // Only disable OTG if this instance enabled it: never kill a
        // pre-existing OTG session owned by another app/driver.
        if(otgWeEnabled && furi_hal_power_is_otg_enabled()) {
            furi_hal_power_disable_otg();
            otgWeEnabled = false;
        }

        if(worker != NULL) {
            subghz_worker_free(worker);
            worker = NULL;
        }

        if(receiver != NULL) {
            subghz_receiver_free(receiver);
            receiver = NULL;
        }

        if(environment != NULL) {
            subghz_environment_free(environment);
            environment = NULL;
        }

        if(device != nullptr && begun) {
            FURI_LOG_I(LOG_TAG, "SubGhzModule: calling subghz_devices_sleep");
            subghz_devices_sleep(device);
            FURI_LOG_I(LOG_TAG, "SubGhzModule: calling subghz_devices_end");
            subghz_devices_end(device);
        }
        // Balances the subghz_devices_init() in the constructor exactly once,
        // whether or not begin() succeeded.
        FURI_LOG_I(LOG_TAG, "SubGhzModule: calling subghz_devices_deinit");
        subghz_devices_deinit();
        device = nullptr;
        begun = false;
        FURI_LOG_I(LOG_TAG, "SubGhzModule: destructor exit");
    }
};
