#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/number_input.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <notification/notification.h>
#include <input/input.h>
#include <furi/core/thread.h>

#include "jammer_signal.h"
#include "jammer_radio.h"
#include "jammer_settings.h"
#include "jammer_thermal.h"

#define TAG "SubGhzJammer"

/** Custom events exchanged between views and the scene manager. */
typedef enum {
    SubGhzJammerEventStart,
    SubGhzJammerEventStop,
    SubGhzJammerEventBack,
    SubGhzJammerEventFreqEdit,
    SubGhzJammerEventMenuEnter,
    SubGhzJammerEventTxError,
} SubGhzJammerEvent;

/** View identifiers for the view dispatcher. */
typedef enum {
    SubGhzJammerViewMenu,
    SubGhzJammerViewStart,
    SubGhzJammerViewStop,
    SubGhzJammerViewFreqEdit,
} SubGhzJammerView;

/** Application instance. */
typedef struct Jammer Jammer;

/** Worker thread */
int32_t jammer_worker_thread(void* context);
void jammer_start_worker(Jammer* jammer);
void jammer_stop_worker(Jammer* jammer);

/** App struct. */
struct Jammer {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    VariableItemList* var_item_list;
    Widget* widget_start;
    Widget* widget_stop;
    NumberInput* number_input;

    JammerSettings settings;
    JammerThermalState thermal;
    FuriThread* worker;
    volatile bool abort_requested;
    volatile bool tx_active;
    volatile bool tx_error;

    // Runtime-only thermal throttle for pulse_ratio.
    // Starts equal to settings.pulse_ratio at worker launch; thermal logic may
    // reduce it during a TX session. Never written back to settings so the
    // user's configured duty-cycle is preserved across sessions.
    uint32_t runtime_pulse_ratio;
};

/** Entry point registered in application.fam. */
int32_t subghz_jammer_app(void* context);
