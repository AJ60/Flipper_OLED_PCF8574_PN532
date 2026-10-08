#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <notification/notification_messages.h>

#include <lib/subghz/subghz_radio_device_loader.h>
#include <lib/subghz/devices/devices.h>

#include "helpers/ham_band.h"
#include "helpers/ham_settings.h"
#include "helpers/ham_scan_worker.h"

typedef struct HamRadioApp HamRadioApp;
typedef struct HamRadioViewBand HamRadioViewBand;
typedef struct HamRadioViewScan HamRadioViewScan;
typedef struct HamRadioViewTune HamRadioViewTune;

/** Custom events start above the tick/idle range used by ViewDispatcher. */
#define HAM_RADIO_CUSTOM_EVENT_BASE 50

typedef enum {
    HamRadioCustomEventSweepDone = HAM_RADIO_CUSTOM_EVENT_BASE,
    HamRadioCustomEventAddEntry,
    HamRadioCustomEventTuneSelected,
    HamRadioCustomEventSweepToggle,
    HamRadioCustomEventTuneOk,
    HamRadioCustomEventRemoveEntry,
} HamRadioCustomEvent;

/** ViewDispatcher slots. Named with an Id infix so they do not collide with the
 * HamRadioView* view object typedefs. */
typedef enum {
    HamRadioViewIdSubmenu,
    HamRadioViewIdWidget,
    HamRadioViewIdVariableItemList,
    HamRadioViewIdNumberInput,
    HamRadioViewIdTextInput,
    HamRadioViewIdBand,
    HamRadioViewIdScan,
    HamRadioViewIdTune,
} HamRadioViewId;

/** Reusable submenu ids. Kept here because several scenes navigate on them. */
typedef enum {
    HamRadioSubmenuBack = 1000,
    HamRadioSubmenuItemScan,
    HamRadioSubmenuItemAdd,
    HamRadioSubmenuItemDelete,
    HamRadioSubmenuItemToggleTx,
    HamRadioSubmenuItemConsent,
    HamRadioSubmenuItemCallsign,
    HamRadioSubmenuItemPower,
    HamRadioSubmenuItemDuty,
    HamRadioSubmenuItemModule,
    HamRadioSubmenuSave,
} HamRadioSubmenu;

/** Which settings field the shared value-picker scene is editing. */
typedef enum {
    HamRadioSettingPower,
    HamRadioSettingDuty,
    HamRadioSettingTxArm,
    HamRadioSettingConsent,
} HamRadioSettingField;

struct HamRadioApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    Widget* widget;
    VariableItemList* variable_item_list;
    NumberInput* number_input;
    TextInput* text_input;

    HamRadioViewBand* view_band;
    HamRadioViewScan* view_scan;
    HamRadioViewTune* view_tune;

    HamBand band;
    HamSettings settings;
    HamScanWorker* scan_worker;

    const SubGhzDevice* radio_device;
    bool radio_external;

    /** Frequency currently pinned in the tune view. */
    uint32_t tune_frequency;
    char tune_label[HAM_BAND_LABEL_SIZE];

    /** Sweep running flag owned by the scan scene. */
    bool sweep_running;

    /** Main-menu highlight, restored when the menu is re-entered. */
    uint32_t start_selected;

    /** Settings field the shared value picker is editing. */
    HamRadioSettingField settings_field;

    /** Scratch state for the add-frequency / add-label input flow. */
    uint32_t draft_frequency;
    bool draft_in_use;

    /** TextInput destinations. These are app-owned buffers because TextInput
     * writes the entered string directly into them. */
    char callsign_buffer[HAM_SETTINGS_CALLSIGN_SIZE];
    char label_buffer[HAM_BAND_LABEL_SIZE + 1];
};

HamRadioApp* ham_radio_app_alloc(void);
void ham_radio_app_free(HamRadioApp* app);

/** Radio access shared by scenes. */
void ham_radio_radio_begin(HamRadioApp* app);
void ham_radio_radio_idle(HamRadioApp* app);
void ham_radio_radio_tune(HamRadioApp* app, uint32_t frequency);
float ham_radio_radio_rssi(HamRadioApp* app);
uint8_t ham_radio_radio_lqi(HamRadioApp* app);

/** Push the current receive entries into the sweep worker and the scan view. */
void ham_radio_push_rx_channels(HamRadioApp* app);

/** Human-readable reason transmit is currently refused, or NULL when armed.
 * Covers the software-side preconditions and the firmware region table; the
 * caller still has to respect the hardware power limit. */
const char* ham_radio_tx_block_reason(HamRadioApp* app, uint32_t frequency);

int32_t ham_radio_app(void* p);
