#include "ham_radio_app_i.h"

#include "scenes/ham_radio_scene.h"
#include "views/ham_view_band.h"
#include "views/ham_view_scan.h"
#include "views/ham_view_tune.h"

#include <furi_hal.h>
#include <notification/notification_messages.h>

#define TAG "HamRadio"

#define HAM_RADIO_DEFAULT_FREQUENCY 433920000

static bool ham_radio_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    HamRadioApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ham_radio_app_back_event_callback(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void ham_radio_app_tick_event_callback(void* context) {
    furi_assert(context);
    HamRadioApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static void ham_radio_view_band_callback(HamRadioCustomEvent event, void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void ham_radio_view_scan_callback(HamRadioCustomEvent event, void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void ham_radio_view_tune_callback(HamRadioCustomEvent event, void* context) {
    furi_assert(context);
    HamRadioApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

/* Radio access ----------------------------------------------------------- */

void ham_radio_radio_begin(HamRadioApp* app) {
    furi_assert(app);
    if(app->radio_device == NULL) return;

    subghz_devices_begin(app->radio_device);
    subghz_devices_reset(app->radio_device);
    subghz_devices_load_preset(app->radio_device, FuriHalSubGhzPresetOok650Async, NULL);
}

void ham_radio_radio_idle(HamRadioApp* app) {
    furi_assert(app);
    if(app->radio_device == NULL) return;

    subghz_devices_flush_rx(app->radio_device);
    subghz_devices_stop_async_rx(app->radio_device);
    subghz_devices_idle(app->radio_device);
}

void ham_radio_radio_tune(HamRadioApp* app, uint32_t frequency) {
    furi_assert(app);
    if(app->radio_device == NULL) return;
    if(!subghz_devices_is_frequency_valid(app->radio_device, frequency)) {
        FURI_LOG_W(TAG, "Refusing invalid frequency %lu Hz", (unsigned long)frequency);
        return;
    }

    subghz_devices_idle(app->radio_device);
    subghz_devices_set_frequency(app->radio_device, frequency);
    subghz_devices_flush_rx(app->radio_device);
    subghz_devices_set_rx(app->radio_device);
}

float ham_radio_radio_rssi(HamRadioApp* app) {
    furi_assert(app);
    if(app->radio_device == NULL) return HAM_SCAN_FLOOR_DBM;
    return subghz_devices_get_rssi(app->radio_device);
}

uint8_t ham_radio_radio_lqi(HamRadioApp* app) {
    furi_assert(app);
    if(app->radio_device == NULL) return 0;
    return subghz_devices_get_lqi(app->radio_device);
}

void ham_radio_push_rx_channels(HamRadioApp* app) {
    furi_assert(app);

    uint32_t frequencies[HAM_SCAN_MAX_CHANNELS];
    const uint8_t count = ham_band_collect_rx(&app->band, frequencies, HAM_SCAN_MAX_CHANNELS);

    ham_scan_worker_set_channels(app->scan_worker, count ? frequencies : NULL, count);
    ham_view_scan_set_channels(app->view_scan, frequencies, count);

    FURI_LOG_D(TAG, "Pushed %u receive channels", count);
}

const char* ham_radio_tx_block_reason(HamRadioApp* app, uint32_t frequency) {
    furi_assert(app);

    if(app->settings.callsign[0] == '\0') return "TX: no callsign";
    if(!app->settings.consent_accepted) return "TX: licence ack needed";
    if(!app->settings.tx_enabled) return "TX: disarmed";

    if(!ham_band_frequency_is_in_range(frequency)) return "TX: freq out of range";

    if(!ham_settings_duty_cycle_allows(&app->settings, furi_get_tick())) {
        return "TX: duty cycle";
    }

    if(app->radio_device != NULL) {
        // The firmware region table only covers the ISM/LPMR allocations, so a
        // licensed amateur frequency usually needs the operator to opt in via
        // extend_range.txt. Report that rather than silently transmitting.
        switch(subghz_devices_check_tx(app->radio_device, frequency)) {
        case SubGhzTxAllowed:
            return NULL;
        case SubGhzTxBlockedRegionNotProvisioned:
        case SubGhzTxBlockedRegion:
            return "TX: region blocked";
        case SubGhzTxBlockedDefault:
        case SubGhzTxBlockedExtended:
            return "TX: needs ext range";
        case SubGhzTxUnsupported:
            return "TX: unsupported";
        default:
            return "TX: blocked";
        }
    }

    return "TX: no radio";
}

/* Lifecycle -------------------------------------------------------------- */

HamRadioApp* ham_radio_app_alloc(void) {
    HamRadioApp* app = malloc(sizeof(HamRadioApp));

    // NULL-init so ham_radio_app_free() can run from any early failure path.
    app->gui = NULL;
    app->view_dispatcher = NULL;
    app->scene_manager = NULL;
    app->notifications = NULL;
    app->submenu = NULL;
    app->widget = NULL;
    app->variable_item_list = NULL;
    app->number_input = NULL;
    app->text_input = NULL;
    app->view_band = NULL;
    app->view_scan = NULL;
    app->view_tune = NULL;
    app->scan_worker = NULL;
    app->radio_device = NULL;
    app->radio_external = false;
    app->sweep_running = false;
    app->settings_field = HamRadioSettingPower;

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&ham_radio_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, ham_radio_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, ham_radio_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, ham_radio_app_tick_event_callback, 200);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HamRadioViewIdSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, HamRadioViewIdWidget, widget_get_view(app->widget));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HamRadioViewIdVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HamRadioViewIdNumberInput, number_input_get_view(app->number_input));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HamRadioViewIdTextInput, text_input_get_view(app->text_input));

    app->view_band = ham_view_band_alloc(ham_radio_view_band_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, HamRadioViewIdBand, ham_view_band_get_view(app->view_band));

    app->view_scan = ham_view_scan_alloc(ham_radio_view_scan_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, HamRadioViewIdScan, ham_view_scan_get_view(app->view_scan));

    app->view_tune = ham_view_tune_alloc(ham_radio_view_tune_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, HamRadioViewIdTune, ham_view_tune_get_view(app->view_tune));

    ham_band_load(&app->band);
    ham_settings_load(&app->settings);

    app->tune_frequency = HAM_RADIO_DEFAULT_FREQUENCY;
    app->tune_label[0] = '\0';
    app->draft_in_use = false;

    furi_hal_power_suppress_charge_enter();

    // The app owns the radio for its lifetime; the sweep worker borrows the
    // same handle so sweeping and tuning never fight over two devices.
    subghz_devices_init();
    app->radio_device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeAuto);
    if(app->radio_device == NULL) {
        FURI_LOG_E(TAG, "No Sub-GHz radio detected (check global setting)");
        ham_radio_app_free(app);
        return NULL;
    }
    app->radio_external = radio_device_loader_is_external(app->radio_device);
    ham_radio_radio_begin(app);

    app->scan_worker = ham_scan_worker_alloc(app->radio_device);
    if(app->scan_worker == NULL) {
        ham_radio_app_free(app);
        return NULL;
    }

    ham_radio_push_rx_channels(app);
    ham_scan_worker_start(app->scan_worker);

    scene_manager_next_scene(app->scene_manager, HamRadioSceneStart);
    return app;
}

void ham_radio_app_free(HamRadioApp* app) {
    furi_assert(app);

    ham_band_save(&app->band);

    // Every member is NULL-checked: alloc calls this on its early failure
    // paths, which run before the later members exist.
    if(app->scan_worker) {
        ham_scan_worker_free(app->scan_worker);
        app->scan_worker = NULL;
    }

    if(app->radio_device) {
        ham_radio_radio_idle(app);
        subghz_devices_sleep(app->radio_device);
        radio_device_loader_end(app->radio_device);
        app->radio_device = NULL;
        subghz_devices_deinit();
    }

    if(app->view_tune) {
        view_dispatcher_remove_view(app->view_dispatcher, HamRadioViewIdTune);
        ham_view_tune_free(app->view_tune);
    }

    if(app->view_scan) {
        view_dispatcher_remove_view(app->view_dispatcher, HamRadioViewIdScan);
        ham_view_scan_free(app->view_scan);
    }

    if(app->view_band) {
        view_dispatcher_remove_view(app->view_dispatcher, HamRadioViewIdBand);
        ham_view_band_free(app->view_band);
    }

    if(app->text_input) {
        view_dispatcher_remove_view(app->view_dispatcher, HamRadioViewIdTextInput);
        text_input_free(app->text_input);
    }

    if(app->number_input) {
        view_dispatcher_remove_view(app->view_dispatcher, HamRadioViewIdNumberInput);
        number_input_free(app->number_input);
    }

    if(app->variable_item_list) {
        view_dispatcher_remove_view(app->view_dispatcher, HamRadioViewIdVariableItemList);
        variable_item_list_free(app->variable_item_list);
    }

    if(app->widget) {
        view_dispatcher_remove_view(app->view_dispatcher, HamRadioViewIdWidget);
        widget_free(app->widget);
    }

    if(app->submenu) {
        view_dispatcher_remove_view(app->view_dispatcher, HamRadioViewIdSubmenu);
        submenu_free(app->submenu);
    }

    if(app->scene_manager) scene_manager_free(app->scene_manager);
    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);

    if(app->notifications) furi_record_close(RECORD_NOTIFICATION);
    if(app->gui) furi_record_close(RECORD_GUI);

    furi_hal_power_suppress_charge_exit();

    free(app);
}

int32_t ham_radio_app(void* p) {
    UNUSED(p);

    HamRadioApp* app = ham_radio_app_alloc();
    if(app == NULL) return 255;

    view_dispatcher_run(app->view_dispatcher);

    ham_radio_app_free(app);
    return 0;
}
