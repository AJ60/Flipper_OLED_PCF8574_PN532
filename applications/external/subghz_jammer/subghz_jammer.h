#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/widget.h>
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

#define TAG "SubGhzJammer"

/** Custom events exchanged between views and the scene manager. */
typedef enum {
    SubGhzJammerEventStart,
    SubGhzJammerEventStop,
    SubGhzJammerEventAbort,
    SubGhzJammerEventHop,
    SubGhzJammerEventBack,
} SubGhzJammerEvent;

/** View identifiers for the view dispatcher. */
typedef enum {
    SubGhzJammerViewMenu,
    SubGhzJammerViewStart,
    SubGhzJammerViewStop,
} SubGhzJammerView;

/** Application instance. */
typedef struct Jammer Jammer;

/**
 * Worker thread owns the entire transmission lifecycle: start the async-TX
 * loop, (for hop mode) reconfigure the frequency on each dwell, and wait for
 * the abort signal before stopping. The async-TX itself runs in the DMA
 * interrupt; this thread only coordinates.
 */
int32_t jammer_worker_thread(void* context);

/** Request a clean abort of the current transmission. */
void jammer_worker_request_abort(void);

/** @return true if a transmission is currently active. */
bool jammer_worker_is_active(void);

/** App struct. */
struct Jammer {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    JammerSignal* signal;
    JammerSettings settings;
    Widget* widget;
    FuriThread* worker;
    FuriPubSub* input_pubsub;
    FuriPubSubSubscription* input_subscription;
    bool abort_requested;
    bool tx_active;
};

/** Entry point registered in application.fam. */
int32_t subghz_jammer_app(void* context);
