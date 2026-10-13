#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/elements.h>
#include <furi_hal_speaker.h>
#include <subghz/devices/devices.h>
#include <lib/subghz/subghz_radio_device_loader.h>
#include <string.h>

#define TAG "WalkieTalkieApp"

#define SUBGHZ_FREQUENCY_MIN  300000000
#define SUBGHZ_FREQUENCY_MAX  928000000
#define SUBGHZ_FREQUENCY_STEP 10000

#define FRS_NUM_CHANNELS    22
#define FRS_NUM_SUBCHANNELS 38

// Ticks (at ~10ms each) to hold on an active channel before auto-resuming scan
#define SCAN_HOLD_TICKS       50
#define SCAN_SETTLE_MS         75
#define SCAN_CONFIRM_SAMPLES    2
#define SQUELCH_LEVEL_DEFAULT -68.0f

static const uint32_t frs_frequencies[FRS_NUM_CHANNELS] = {
    462562500, 462587500, 462612500, 462637500, 462662500,
    462687500, 462712500, 467562500, 467587500, 467612500,
    467637500, 467662500, 467687500, 467712500, 462550000,
    462575000, 462600000, 462625000, 462650000, 462675000,
    462700000, 462725000};

typedef enum {
    ScanDirectionUp,
    ScanDirectionDown,
} ScanDirection;

typedef enum {
    WalkieTalkieScanModeAuto,
    WalkieTalkieScanModeManual,
} WalkieTalkieScanMode;

typedef enum {
    WalkieTalkiePageMenu,
    WalkieTalkiePageListenNow,
    WalkieTalkiePageActiveList,
    WalkieTalkiePageSettings,
    WalkieTalkiePageFrsList,
    WalkieTalkiePageAbout,
} WalkieTalkiePage;

typedef struct {
    uint8_t channel_idx;
    float rssi;
    uint32_t last_seen_tick;
} ActiveChannelItem;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriMutex* mutex;
    bool running;
    uint32_t current_channel;
    uint32_t current_subchannel;
    bool mute;
    uint32_t frequency;
    float rssi;
    WalkieTalkieScanMode scan_mode;
    bool scanning;
    ScanDirection scan_direction;
    WalkieTalkiePage page;
    uint32_t frs_list_index;
    uint32_t menu_index;
    uint32_t settings_cursor;
    ActiveChannelItem active_channels[FRS_NUM_CHANNELS];
    uint8_t active_channels_count;
    uint8_t active_list_index;
    const SubGhzDevice* radio_device;
    bool speaker_acquired;
    bool auto_squelch;
    bool scan_paused;
    float squelch_level;
    uint32_t scan_hold_ticks; // countdown ticks before auto-resuming scan
    uint32_t frequency_set_tick;
    uint8_t signal_samples;
} WalkieTalkieApp;

WalkieTalkieApp* walkie_talkie_app_alloc();
void walkie_talkie_app_free(WalkieTalkieApp* app);
int32_t walkie_talkie_app(void* p);

// Map RSSI to 0-5 signal bars
static int walkie_talkie_rssi_bars(float rssi) {
    if(rssi > -50) return 5;
    if(rssi > -60) return 4;
    if(rssi > -70) return 3;
    if(rssi > -85) return 2;
    if(rssi > -100) return 1;
    return 0;
}

static void walkie_talkie_draw_signal_bars(Canvas* canvas, int bars, int x, int y) {
    for(int b = 0; b < 5; b++) {
        int bar_h = 2 + b * 2;
        int bar_x = x + b * 6;
        int bar_y = y - bar_h;
        if(b < bars) {
            canvas_draw_box(canvas, bar_x, bar_y, 4, bar_h);
        } else {
            canvas_draw_frame(canvas, bar_x, bar_y, 4, bar_h);
        }
    }
}

static void walkie_talkie_add_active_channel(WalkieTalkieApp* app, uint8_t channel_idx, float rssi) {
    if(!app || channel_idx >= FRS_NUM_CHANNELS) return;

    // Check if channel already exists in the active list
    for(uint8_t i = 0; i < app->active_channels_count; i++) {
        if(app->active_channels[i].channel_idx == channel_idx) {
            app->active_channels[i].rssi = rssi;
            app->active_channels[i].last_seen_tick = furi_get_tick();
            return;
        }
    }

    if(app->active_channels_count >= FRS_NUM_CHANNELS) return;

    // Insert sorted by channel index for a clean and stable list
    uint8_t insert_pos = 0;
    while(insert_pos < app->active_channels_count &&
          app->active_channels[insert_pos].channel_idx < channel_idx) {
        insert_pos++;
    }

    for(uint8_t i = app->active_channels_count; i > insert_pos; i--) {
        app->active_channels[i] = app->active_channels[i - 1];
    }

    app->active_channels[insert_pos].channel_idx = channel_idx;
    app->active_channels[insert_pos].rssi = rssi;
    app->active_channels[insert_pos].last_seen_tick = furi_get_tick();
    app->active_channels_count++;
}

#define ACTIVE_CHANNEL_EXPIRE_MS (300000) // 5 minutes

static void walkie_talkie_prune_active_channels(WalkieTalkieApp* app) {
    if(!app || !app->mutex) return;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->active_channels_count > 0) {
        uint32_t now = furi_get_tick();
        for(int i = (int)app->active_channels_count - 1; i >= 0; i--) {
            if((now - app->active_channels[i].last_seen_tick) > ACTIVE_CHANNEL_EXPIRE_MS) {
                for(uint8_t j = i; j < app->active_channels_count - 1; j++) {
                    app->active_channels[j] = app->active_channels[j + 1];
                }
                app->active_channels_count--;
                if(app->active_list_index >= app->active_channels_count && app->active_list_index > 0) {
                    app->active_list_index--;
                }
            }
        }
    }
    furi_mutex_release(app->mutex);
}

static void walkie_talkie_draw_callback(Canvas* canvas, void* context) {
    WalkieTalkieApp* app = context;
    if(!app || !app->mutex) return;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    bool is_ext = app->radio_device && radio_device_loader_is_external(app->radio_device);

    if(app->page == WalkieTalkiePageActiveList) {
        char act_title[32];
        snprintf(
            act_title,
            sizeof(act_title),
            "Active (%u)%s",
            app->active_channels_count,
            is_ext ? " [Ext]" : "");
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, act_title);
    } else if(app->page == WalkieTalkiePageFrsList) {
        canvas_draw_str_aligned(
            canvas,
            64,
            0,
            AlignCenter,
            AlignTop,
            is_ext ? "FRS Channels [Ext]" : "FRS Channels");
    } else if(is_ext) {
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Walkie-Talkie [Ext]");
    } else {
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Walkie-Talkie");
    }

    canvas_set_font(canvas, FontSecondary);
    if(app->page == WalkieTalkiePageMenu) {
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Menu");

        char active_item_text[24];
        snprintf(
            active_item_text,
            sizeof(active_item_text),
            "Active List (%u)",
            app->active_channels_count);

        const char* items[] = {
            "Listen Now",
            active_item_text,
            "Settings",
            "FRS List",
            "About"};

        uint32_t total = 5;
        uint32_t sel = app->menu_index;
        if(sel >= total) sel = total - 1;
        uint32_t start = (sel >= 3) ? sel - 3 : 0;
        if(start + 4 > total) start = (total >= 4) ? total - 4 : 0;

        for(uint32_t i = 0; i < 4; i++) {
            uint32_t idx = start + i;
            if(idx >= total) break;
            char line[32];
            snprintf(line, sizeof(line), "%c %s", (idx == sel) ? '>' : ' ', items[idx]);
            canvas_draw_str_aligned(canvas, 14, 22 + (i * 10), AlignLeft, AlignTop, line);
        }
        elements_scrollbar(canvas, sel, total);

    } else if(app->page == WalkieTalkiePageListenNow) {
        canvas_set_font(canvas, FontPrimary);
        char ch_big[16];
        snprintf(ch_big, sizeof(ch_big), "CH %02lu", (unsigned long)(app->current_channel + 1));
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignTop, ch_big);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            4,
            12,
            AlignLeft,
            AlignTop,
            (app->scan_mode == WalkieTalkieScanModeAuto) ? "[AUTO]" : "[MAN]");

        if(app->active_channels_count > 0) {
            char act_cnt_str[12];
            snprintf(act_cnt_str, sizeof(act_cnt_str), "Act:%u", app->active_channels_count);
            canvas_draw_str_aligned(canvas, 124, 12, AlignRight, AlignTop, act_cnt_str);
        }

        char sub_str[16];
        if(app->current_subchannel == 0) {
            snprintf(sub_str, sizeof(sub_str), "SUB --");
        } else {
            snprintf(sub_str, sizeof(sub_str), "SUB %02lu", (unsigned long)app->current_subchannel);
        }
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, sub_str);

        char freq_str[32];
        uint32_t f_mhz = app->frequency / 1000000u;
        uint32_t f_khz = (app->frequency % 1000000u) / 1000u;
        snprintf(freq_str, sizeof(freq_str), "%lu.%03lu MHz", (unsigned long)f_mhz, (unsigned long)f_khz);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, freq_str);

        char rssi_str[20];
        snprintf(rssi_str, sizeof(rssi_str), "RSSI: %.0f dBm", (double)app->rssi);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, rssi_str);

        const char* status_str;
        if(app->scanning && !app->scan_paused) {
            status_str = app->scan_direction == ScanDirectionUp ? "SCAN UP" : "SCAN DN";
        } else if(app->scanning && app->scan_paused) {
            status_str = "ACTIVE";
        } else {
            status_str = app->mute ? "MUTED" : "LISTENING";
        }
        canvas_draw_str_aligned(canvas, 4, 55, AlignLeft, AlignTop, status_str);

        // Signal strength bars at bottom-right
        int bars = walkie_talkie_rssi_bars(app->rssi);
        walkie_talkie_draw_signal_bars(canvas, bars, 94, 63);

    } else if(app->page == WalkieTalkiePageActiveList) {
        canvas_draw_line(canvas, 0, 9, 127, 9);
        canvas_set_font(canvas, FontSecondary);

        if(app->active_channels_count == 0) {
            canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "No Active Channels");
            if(app->scan_mode == WalkieTalkieScanModeAuto) {
                canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Scanning FRS band...");
            } else {
                canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Mode: MANUAL");
                canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "[OK] Start Auto Scan");
            }
            canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignTop, "[Back] Menu");
        } else {
            uint32_t total = app->active_channels_count;
            uint32_t sel = app->active_list_index;
            if(sel >= total) sel = total - 1;
            uint32_t start = (sel >= 2) ? sel - 2 : 0;
            if(start + 5 > total) start = (total >= 5) ? total - 5 : 0;

            for(uint32_t i = 0; i < 5; i++) {
                uint32_t idx = start + i;
                if(idx >= total) break;

                ActiveChannelItem* item = &app->active_channels[idx];
                uint32_t freq_hz = frs_frequencies[item->channel_idx];
                uint32_t mhz = freq_hz / 1000000;
                uint32_t khz = (freq_hz % 1000000) / 1000;
                int row_y = 11 + (int)(i * 10);
                bool is_selected = (idx == sel);
                bool is_current = (item->channel_idx == app->current_channel);

                if(is_selected) {
                    canvas_draw_box(canvas, 0, row_y - 1, 120, 10);
                    canvas_set_color(canvas, ColorWhite);
                }

                uint32_t age_sec = (furi_get_tick() - item->last_seen_tick) / 1000;
                char age_str[8];
                if(age_sec < 3) {
                    snprintf(age_str, sizeof(age_str), "ACT");
                } else if(age_sec < 60) {
                    snprintf(age_str, sizeof(age_str), "%lus", (unsigned long)age_sec);
                } else {
                    snprintf(age_str, sizeof(age_str), "%lum", (unsigned long)(age_sec / 60));
                }

                char line[32];
                snprintf(
                    line,
                    sizeof(line),
                    "%s CH%02u %lu.%03lu %3s",
                    is_current ? "*" : " ",
                    (unsigned int)(item->channel_idx + 1),
                    (unsigned long)mhz,
                    (unsigned long)khz,
                    age_str);
                canvas_draw_str_aligned(canvas, 2, row_y, AlignLeft, AlignTop, line);

                if(is_selected) {
                    canvas_set_color(canvas, ColorBlack);
                }
            }

            elements_scrollbar(canvas, sel, total);
        }

    } else if(app->page == WalkieTalkiePageSettings) {
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Settings");

        char mode_toggle_str[32];
        snprintf(
            mode_toggle_str,
            sizeof(mode_toggle_str),
            "%c Scan Mode: %s",
            (app->settings_cursor == 0) ? '>' : ' ',
            (app->scan_mode == WalkieTalkieScanModeAuto) ? "AUTO" : "MANUAL");
        canvas_draw_str_aligned(canvas, 6, 22, AlignLeft, AlignTop, mode_toggle_str);

        char squelch_toggle_str[32];
        snprintf(
            squelch_toggle_str,
            sizeof(squelch_toggle_str),
            "%c Auto-Squelch: %s",
            (app->settings_cursor == 1) ? '>' : ' ',
            app->auto_squelch ? "ON" : "OFF");
        canvas_draw_str_aligned(canvas, 6, 33, AlignLeft, AlignTop, squelch_toggle_str);

        if(app->auto_squelch) {
            char sens_str[32];
            snprintf(
                sens_str,
                sizeof(sens_str),
                "%c Sensitivity: %.0f dB",
                (app->settings_cursor == 2) ? '>' : ' ',
                (double)app->squelch_level);
            canvas_draw_str_aligned(canvas, 6, 44, AlignLeft, AlignTop, sens_str);
            canvas_draw_str_aligned(canvas, 6, 55, AlignLeft, AlignTop, "  [< >] adjust  [OK] select");
        } else {
            canvas_draw_str_aligned(canvas, 6, 55, AlignLeft, AlignTop, "  [OK] toggle setting");
        }

    } else if(app->page == WalkieTalkiePageAbout) {
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "About");
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "FRS Channel Monitor");
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Subchannels are labels");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "No CTCSS/DCS decode");
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "Auto/Manual Scanner");

    } else if(app->page == WalkieTalkiePageFrsList) {
        canvas_draw_line(canvas, 0, 9, 127, 9);

        // Scroll window: show 5 rows fitting below the header line
        uint32_t total = FRS_NUM_CHANNELS;
        uint32_t sel = app->frs_list_index;
        uint32_t start = (sel >= 2) ? sel - 2 : 0;
        if(start + 5 > total) start = (total >= 5) ? total - 5 : 0;

        for(uint32_t i = 0; i < 5; i++) {
            uint32_t idx = start + i;
            if(idx >= total) break;

            uint32_t freq_hz = frs_frequencies[idx];
            uint32_t mhz = freq_hz / 1000000;
            uint32_t khz = (freq_hz % 1000000) / 1000;
            int row_y = 11 + (int)(i * 10);
            bool is_selected = (idx == sel);
            bool is_current = (idx == app->current_channel);

            if(is_selected) {
                canvas_draw_box(canvas, 0, row_y - 1, 120, 10);
                canvas_set_color(canvas, ColorWhite);
            }

            char line[32];
            snprintf(
                line,
                sizeof(line),
                "%s CH%02lu  %lu.%03lu",
                is_current ? "*" : " ",
                (unsigned long)(idx + 1),
                (unsigned long)mhz,
                (unsigned long)khz);
            canvas_draw_str_aligned(canvas, 2, row_y, AlignLeft, AlignTop, line);

            if(is_selected) {
                canvas_set_color(canvas, ColorBlack);
            }
        }

        elements_scrollbar(canvas, sel, total);
    }
    furi_mutex_release(app->mutex);
}

static void walkie_talkie_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    FuriMessageQueue* event_queue = context;
    furi_message_queue_put(event_queue, input_event, 0);
}

static void walkie_talkie_rx_callback(const void* data, size_t size, void* context) {
    (void)data;
    (void)size;
    (void)context;
}

static void walkie_talkie_apply_audio(WalkieTalkieApp* app) {
    if(!app->radio_device || !app->speaker_acquired) return;

    bool muted = app->mute;

    if(!muted && app->scanning && !app->scan_paused) {
        // While actively hopping channels, suppress speaker noise
        muted = true;
    } else if(!muted && app->auto_squelch) {
        muted = (app->rssi < app->squelch_level);
    }

    subghz_devices_set_async_mirror_pin(app->radio_device, muted ? NULL : &gpio_speaker);
}

static void walkie_talkie_set_frequency(WalkieTalkieApp* app, uint32_t frequency) {
    if(!app->radio_device) return;

    subghz_devices_stop_async_rx(app->radio_device);
    subghz_devices_idle(app->radio_device);

    app->frequency = frequency;
    subghz_devices_set_frequency(app->radio_device, app->frequency);

    subghz_devices_start_async_rx(app->radio_device, walkie_talkie_rx_callback, app);
    app->frequency_set_tick = furi_get_tick();
    app->signal_samples = 0;
}

static void walkie_talkie_update_rssi(WalkieTalkieApp* app) {
    if(app->radio_device) {
        app->rssi = subghz_devices_get_rssi(app->radio_device);
    } else {
        app->rssi = -127.0f;
    }
}

static bool walkie_talkie_init_subghz(WalkieTalkieApp* app) {
    subghz_devices_init();

    const SubGhzDevice* device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeAuto);
    if(!device) {
        FURI_LOG_E(TAG, "Failed to get SubGhzDevice");
        subghz_devices_deinit();
        return false;
    }

    app->radio_device = device;

    subghz_devices_begin(device);

    if(!subghz_devices_is_frequency_valid(device, app->frequency)) {
        FURI_LOG_E(TAG, "Invalid frequency: %lu", app->frequency);
        subghz_devices_sleep(device);
        radio_device_loader_end(device);
        app->radio_device = NULL;
        return false;
    }

    subghz_devices_load_preset(device, FuriHalSubGhzPreset2FSKDev238Async, NULL);
    subghz_devices_set_frequency(device, app->frequency);
    subghz_devices_start_async_rx(device, walkie_talkie_rx_callback, app);

    if(furi_hal_speaker_acquire(30)) {
        app->speaker_acquired = true;
        walkie_talkie_apply_audio(app);
    } else {
        app->speaker_acquired = false;
        FURI_LOG_E(TAG, "Failed to acquire speaker");
    }

    return true;
}

static void walkie_talkie_process_scanning(WalkieTalkieApp* app) {
    if((furi_get_tick() - app->frequency_set_tick) < SCAN_SETTLE_MS) return;
    walkie_talkie_update_rssi(app);

    bool signal_detected = (app->rssi > app->squelch_level);

    if(app->scan_paused) {
        if(signal_detected) {
            // Keep hold timer fresh while signal is active
            app->scan_hold_ticks = SCAN_HOLD_TICKS;
            walkie_talkie_add_active_channel(app, (uint8_t)app->current_channel, app->rssi);
        } else if(app->scan_hold_ticks > 0) {
            app->scan_hold_ticks--;
        } else {
            // Signal gone long enough — resume scanning
            app->scan_paused = false;
            walkie_talkie_apply_audio(app);
            FURI_LOG_D(TAG, "Scan auto-resumed");
        }
        return;
    }

    if(signal_detected) {
        if(++app->signal_samples < SCAN_CONFIRM_SAMPLES) return;
        app->scan_paused = true;
        app->scan_hold_ticks = SCAN_HOLD_TICKS;
        walkie_talkie_add_active_channel(app, (uint8_t)app->current_channel, app->rssi);
        walkie_talkie_apply_audio(app); // unmute so user hears the signal
        FURI_LOG_D(TAG, "Scan paused on CH %lu (Active recorded)", app->current_channel + 1);
        return;
    }
    app->signal_samples = 0;

    // Advance to next channel in the selected direction
    if(app->scan_direction == ScanDirectionUp) {
        app->current_channel = (app->current_channel + 1) % FRS_NUM_CHANNELS;
    } else {
        app->current_channel =
            (app->current_channel > 0) ? app->current_channel - 1 : FRS_NUM_CHANNELS - 1;
    }
    walkie_talkie_set_frequency(app, frs_frequencies[app->current_channel]);
}

WalkieTalkieApp* walkie_talkie_app_alloc() {
    WalkieTalkieApp* app = malloc(sizeof(WalkieTalkieApp));
    if(!app) return NULL;

    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->running = true;
    app->current_channel = 0;
    app->current_subchannel = 0;
    app->mute = false;
    app->frequency = frs_frequencies[0];
    app->rssi = -100.0f;
    app->scan_mode = WalkieTalkieScanModeAuto;
    app->scanning = true;
    app->scan_direction = ScanDirectionUp;
    app->scan_paused = false;
    app->scan_hold_ticks = 0;
    app->frequency_set_tick = 0;
    app->signal_samples = 0;
    app->speaker_acquired = false;
    app->auto_squelch = true;
    app->squelch_level = SQUELCH_LEVEL_DEFAULT;
    app->page = WalkieTalkiePageListenNow;
    app->frs_list_index = 0;
    app->active_list_index = 0;
    app->active_channels_count = 0;
    memset(app->active_channels, 0, sizeof(app->active_channels));
    app->menu_index = 0;
    app->settings_cursor = 0;
    app->radio_device = NULL;

    view_port_draw_callback_set(app->view_port, walkie_talkie_draw_callback, app);
    view_port_input_callback_set(app->view_port, walkie_talkie_input_callback, app->event_queue);

    return app;
}

void walkie_talkie_app_free(WalkieTalkieApp* app) {
    if(app->speaker_acquired && furi_hal_speaker_is_mine()) {
        subghz_devices_set_async_mirror_pin(app->radio_device, NULL);
        furi_hal_speaker_release();
        app->speaker_acquired = false;
    }

    if(app->radio_device) {
        subghz_devices_stop_async_rx(app->radio_device);
        subghz_devices_idle(app->radio_device);
        subghz_devices_sleep(app->radio_device);
        radio_device_loader_end(app->radio_device);
        app->radio_device = NULL;
    }

    subghz_devices_deinit();
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    if(app->mutex) {
        furi_mutex_free(app->mutex);
        app->mutex = NULL;
    }
    free(app);
}

int32_t walkie_talkie_app(void* p) {
    UNUSED(p);

    WalkieTalkieApp* app = walkie_talkie_app_alloc();
    if(!app) return -1;

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);

    if(!walkie_talkie_init_subghz(app)) {
        FURI_LOG_E(TAG, "Failed to initialize SubGHz");
        gui_remove_view_port(gui, app->view_port);
        furi_record_close(RECORD_GUI);
        walkie_talkie_app_free(app);
        return 255;
    }

    InputEvent event;
    while(app->running) {
        walkie_talkie_prune_active_channels(app);

        if(app->scanning) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            walkie_talkie_process_scanning(app);
            furi_mutex_release(app->mutex);
        } else {
            if((furi_get_tick() - app->frequency_set_tick) >= SCAN_SETTLE_MS) {
                walkie_talkie_update_rssi(app);
                if(app->auto_squelch) {
                    walkie_talkie_apply_audio(app);
                }
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                if(app->rssi > app->squelch_level) {
                    walkie_talkie_add_active_channel(app, (uint8_t)app->current_channel, app->rssi);
                }
                furi_mutex_release(app->mutex);
            }
        }

        if(furi_message_queue_get(app->event_queue, &event, 10) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(event.type == InputTypeShort) {
                if(event.key == InputKeyOk) {
                    if(app->page == WalkieTalkiePageMenu) {
                        if(app->menu_index == 0) app->page = WalkieTalkiePageListenNow;
                        else if(app->menu_index == 1) app->page = WalkieTalkiePageActiveList;
                        else if(app->menu_index == 2) app->page = WalkieTalkiePageSettings;
                        else if(app->menu_index == 3) app->page = WalkieTalkiePageFrsList;
                        else if(app->menu_index == 4) app->page = WalkieTalkiePageAbout;
                    } else if(app->page == WalkieTalkiePageActiveList) {
                        if(app->active_channels_count > 0) {
                            if(app->active_list_index >= app->active_channels_count) {
                                app->active_list_index = app->active_channels_count - 1;
                            }
                            app->current_channel = app->active_channels[app->active_list_index].channel_idx;
                            walkie_talkie_set_frequency(app, frs_frequencies[app->current_channel]);
                            if(app->scan_mode == WalkieTalkieScanModeAuto) {
                                app->scan_paused = true;
                                app->scan_hold_ticks = SCAN_HOLD_TICKS;
                            } else {
                                app->scanning = false;
                                app->scan_paused = false;
                            }
                            walkie_talkie_apply_audio(app);
                            app->page = WalkieTalkiePageListenNow;
                        } else {
                            if(app->scan_mode == WalkieTalkieScanModeManual) {
                                app->scan_mode = WalkieTalkieScanModeAuto;
                                app->scanning = true;
                                app->scan_paused = false;
                                walkie_talkie_apply_audio(app);
                            }
                        }
                    } else if(app->page == WalkieTalkiePageFrsList) {
                        // Tune to the highlighted channel
                        app->current_channel = app->frs_list_index;
                        if(app->scan_mode == WalkieTalkieScanModeManual) {
                            app->scanning = false;
                            app->scan_paused = false;
                        } else {
                            app->scan_paused = true;
                            app->scan_hold_ticks = SCAN_HOLD_TICKS;
                        }
                        walkie_talkie_set_frequency(app, frs_frequencies[app->current_channel]);
                        walkie_talkie_apply_audio(app);
                        app->page = WalkieTalkiePageListenNow;
                    } else if(app->page == WalkieTalkiePageSettings) {
                        if(app->settings_cursor == 0) {
                            // Toggle scan mode between Auto and Manual
                            if(app->scan_mode == WalkieTalkieScanModeAuto) {
                                app->scan_mode = WalkieTalkieScanModeManual;
                                app->scanning = false;
                                app->scan_paused = false;
                            } else {
                                app->scan_mode = WalkieTalkieScanModeAuto;
                                app->scanning = true;
                                app->scan_paused = false;
                            }
                            walkie_talkie_apply_audio(app);
                        } else if(app->settings_cursor == 1) {
                            app->auto_squelch = !app->auto_squelch;
                            if(!app->auto_squelch && app->settings_cursor > 1) {
                                app->settings_cursor = 1;
                            }
                            walkie_talkie_apply_audio(app);
                        } else if(app->settings_cursor == 2 && app->auto_squelch) {
                            // Reset squelch to default
                            app->squelch_level = SQUELCH_LEVEL_DEFAULT;
                        }
                    } else {
                        app->mute = !app->mute;
                        walkie_talkie_apply_audio(app);
                    }
                } else if(event.key == InputKeyUp) {
                    if(app->page == WalkieTalkiePageMenu) {
                        if(app->menu_index > 0) app->menu_index--;
                    } else if(app->page == WalkieTalkiePageActiveList) {
                        if(app->active_list_index > 0) app->active_list_index--;
                    } else if(app->page == WalkieTalkiePageSettings) {
                        if(app->settings_cursor > 0) app->settings_cursor--;
                    } else if(app->page == WalkieTalkiePageFrsList) {
                        if(app->frs_list_index > 0) app->frs_list_index--;
                    } else {
                        // Channel up
                        app->current_channel =
                            (app->current_channel < FRS_NUM_CHANNELS - 1) ?
                                app->current_channel + 1 : 0;
                        if(app->scan_mode == WalkieTalkieScanModeAuto) {
                            app->scan_direction = ScanDirectionUp;
                            app->scan_paused = false;
                            app->scanning = true;
                        } else {
                            app->scanning = false;
                            app->scan_paused = false;
                        }
                        walkie_talkie_set_frequency(app, frs_frequencies[app->current_channel]);
                        walkie_talkie_apply_audio(app);
                    }
                } else if(event.key == InputKeyDown) {
                    if(app->page == WalkieTalkiePageMenu) {
                        if(app->menu_index < 4) app->menu_index++;
                    } else if(app->page == WalkieTalkiePageActiveList) {
                        if(app->active_channels_count > 0 &&
                           app->active_list_index < app->active_channels_count - 1) {
                            app->active_list_index++;
                        }
                    } else if(app->page == WalkieTalkiePageSettings) {
                        uint32_t max_cursor = app->auto_squelch ? 2 : 1;
                        if(app->settings_cursor < max_cursor) app->settings_cursor++;
                    } else if(app->page == WalkieTalkiePageFrsList) {
                        if(app->frs_list_index < FRS_NUM_CHANNELS - 1) app->frs_list_index++;
                    } else {
                        // Channel down
                        app->current_channel =
                            (app->current_channel > 0) ?
                                app->current_channel - 1 : FRS_NUM_CHANNELS - 1;
                        if(app->scan_mode == WalkieTalkieScanModeAuto) {
                            app->scan_direction = ScanDirectionDown;
                            app->scan_paused = false;
                            app->scanning = true;
                        } else {
                            app->scanning = false;
                            app->scan_paused = false;
                        }
                        walkie_talkie_set_frequency(app, frs_frequencies[app->current_channel]);
                        walkie_talkie_apply_audio(app);
                    }
                } else if(event.key == InputKeyLeft) {
                    if(app->page == WalkieTalkiePageSettings) {
                        if(app->settings_cursor == 0) {
                            if(app->scan_mode == WalkieTalkieScanModeAuto) {
                                app->scan_mode = WalkieTalkieScanModeManual;
                                app->scanning = false;
                                app->scan_paused = false;
                            } else {
                                app->scan_mode = WalkieTalkieScanModeAuto;
                                app->scanning = true;
                                app->scan_paused = false;
                            }
                            walkie_talkie_apply_audio(app);
                        } else if(app->settings_cursor == 1) {
                            app->auto_squelch = !app->auto_squelch;
                            if(!app->auto_squelch && app->settings_cursor > 1) {
                                app->settings_cursor = 1;
                            }
                            walkie_talkie_apply_audio(app);
                        } else if(app->settings_cursor == 2 && app->auto_squelch) {
                            if(app->squelch_level > -120.0f) app->squelch_level -= 1.0f;
                        }
                    } else if(
                        app->page != WalkieTalkiePageFrsList &&
                        app->page != WalkieTalkiePageActiveList &&
                        app->page != WalkieTalkiePageMenu) {
                        if(app->current_subchannel > 0) app->current_subchannel--;
                    }
                } else if(event.key == InputKeyRight) {
                    if(app->page == WalkieTalkiePageSettings) {
                        if(app->settings_cursor == 0) {
                            if(app->scan_mode == WalkieTalkieScanModeAuto) {
                                app->scan_mode = WalkieTalkieScanModeManual;
                                app->scanning = false;
                                app->scan_paused = false;
                            } else {
                                app->scan_mode = WalkieTalkieScanModeAuto;
                                app->scanning = true;
                                app->scan_paused = false;
                            }
                            walkie_talkie_apply_audio(app);
                        } else if(app->settings_cursor == 1) {
                            app->auto_squelch = !app->auto_squelch;
                            if(!app->auto_squelch && app->settings_cursor > 1) {
                                app->settings_cursor = 1;
                            }
                            walkie_talkie_apply_audio(app);
                        } else if(app->settings_cursor == 2 && app->auto_squelch) {
                            if(app->squelch_level < -30.0f) app->squelch_level += 1.0f;
                        }
                    } else if(
                        app->page != WalkieTalkiePageFrsList &&
                        app->page != WalkieTalkiePageActiveList &&
                        app->page != WalkieTalkiePageMenu) {
                        if(app->current_subchannel < FRS_NUM_SUBCHANNELS)
                            app->current_subchannel++;
                    }
                } else if(event.key == InputKeyBack) {
                    if(app->page == WalkieTalkiePageMenu) {
                        app->page = WalkieTalkiePageListenNow;
                    } else {
                        if(app->page == WalkieTalkiePageSettings) app->settings_cursor = 0;
                        app->page = WalkieTalkiePageMenu;
                    }
                }
            } else if(event.type == InputTypeLong) {
                if(event.key == InputKeyOk) {
                    if(app->page == WalkieTalkiePageListenNow) {
                        if(app->scan_mode == WalkieTalkieScanModeAuto) {
                            if(app->scan_paused) {
                                // Skip current transmission and resume auto scan immediately
                                app->scan_paused = false;
                                app->scan_hold_ticks = 0;
                                walkie_talkie_apply_audio(app);
                            } else {
                                // Pause scan on current channel
                                app->scan_paused = true;
                                app->scan_hold_ticks = SCAN_HOLD_TICKS;
                                walkie_talkie_apply_audio(app);
                            }
                        } else {
                            // Toggle manual scan mode
                            if(!app->scanning) {
                                app->scanning = true;
                                app->scan_paused = false;
                            } else {
                                app->scanning = false;
                                app->scan_paused = false;
                            }
                            walkie_talkie_apply_audio(app);
                        }
                    } else if(app->page == WalkieTalkiePageActiveList) {
                        // Clear active channels list on hold OK
                        app->active_channels_count = 0;
                        app->active_list_index = 0;
                    }
                } else if(event.key == InputKeyBack) {
                    app->running = false;
                } else if(event.key == InputKeyLeft) {
                    app->scan_direction = ScanDirectionDown;
                } else if(event.key == InputKeyRight) {
                    app->scan_direction = ScanDirectionUp;
                } else if(event.key == InputKeyUp) {
                    // Quick squelch raise from Listen screen
                    if(app->page == WalkieTalkiePageListenNow && app->squelch_level < -30.0f) {
                        app->squelch_level += 1.0f;
                    }
                } else if(event.key == InputKeyDown) {
                    // Quick squelch lower from Listen screen
                    if(app->page == WalkieTalkiePageListenNow && app->squelch_level > -120.0f) {
                        app->squelch_level -= 1.0f;
                    }
                }
            }
            furi_mutex_release(app->mutex);
        }

        view_port_update(app->view_port);
        furi_delay_ms(10);
    }

    gui_remove_view_port(gui, app->view_port);
    furi_record_close(RECORD_GUI);
    walkie_talkie_app_free(app);
    return 0;
}
