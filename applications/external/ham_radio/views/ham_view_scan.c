#include "ham_view_scan.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>

#include <math.h>

#define TAG "HamViewScan"

#define PLOT_LEFT_X   2
#define PLOT_RIGHT_X  125
#define PLOT_WIDTH    (PLOT_RIGHT_X - PLOT_LEFT_X + 1)
#define PLOT_TOP_Y    12
#define PLOT_BOTTOM_Y 42
#define PLOT_HEIGHT   (PLOT_BOTTOM_Y - PLOT_TOP_Y)

typedef struct {
    uint32_t frequencies[HAM_SCAN_MAX_CHANNELS];
    float rssi_dbm[HAM_SCAN_MAX_CHANNELS];
    uint8_t count;
    uint8_t peak_channel;
    bool peak_valid;
    bool running;
    bool ext_module;
} HamViewScanModel;

struct HamRadioViewScan {
    View* view;
    HamViewScanCallback callback;
    void* context;
};

/** Map RSSI dBm onto a 0..PLOT_HEIGHT bar height. */
static uint8_t ham_view_scan_height_for(float rssi_dbm) {
    if(rssi_dbm <= HAM_SCAN_FLOOR_DBM) return 0;
    if(rssi_dbm >= HAM_SCAN_PEAK_DBM) return PLOT_HEIGHT;

    const float span = (float)(HAM_SCAN_PEAK_DBM - HAM_SCAN_FLOOR_DBM);
    const float scaled = (rssi_dbm - HAM_SCAN_FLOOR_DBM) / span * (float)PLOT_HEIGHT;
    return (uint8_t)CLAMP((int)scaled, 0, PLOT_HEIGHT);
}

static uint8_t ham_view_scan_x_for(HamViewScanModel* model, uint8_t index) {
    if(model->count <= 1) return (uint8_t)(PLOT_LEFT_X + PLOT_WIDTH / 2);

    // Spread channels evenly across the plot regardless of how few there are.
    const int step = (int)(PLOT_WIDTH - 1) / (int)(model->count - 1);
    return (uint8_t)(PLOT_LEFT_X + index * step);
}

static void ham_view_scan_draw(Canvas* canvas, void* context) {
    furi_assert(context);
    HamViewScanModel* model = context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(model->count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Nothing to sweep");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Add a frequency");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "in the band list");
        elements_button_left(canvas, "Back");
        return;
    }

    // Axis baseline and a mid-height reference tick.
    canvas_draw_line(canvas, PLOT_LEFT_X, PLOT_BOTTOM_Y, PLOT_RIGHT_X, PLOT_BOTTOM_Y);
    for(uint8_t x = PLOT_LEFT_X; x <= PLOT_RIGHT_X; x += 6) {
        canvas_draw_line(canvas, x, PLOT_BOTTOM_Y, x, PLOT_BOTTOM_Y + 2);
    }

    for(uint8_t i = 0; i < model->count; ++i) {
        const uint8_t height = ham_view_scan_height_for(model->rssi_dbm[i]);
        const uint8_t x = ham_view_scan_x_for(model, i);
        const int y_bottom = PLOT_BOTTOM_Y;
        const int y_top = PLOT_BOTTOM_Y - height;

        // Monochrome panel: a strong channel is drawn as a solid double-width
        // bar rather than being given a distinct colour.
        const bool strong = model->rssi_dbm[i] >= HAM_SCAN_PEAK_DBM;
        canvas_set_color(canvas, ColorBlack);
        if(height > 0) {
            canvas_draw_line(canvas, x, y_bottom, x, y_top);
            if(strong) {
                canvas_draw_line(canvas, x + 1, y_bottom, x + 1, y_top);
            }
        } else {
            // Make a floor-level channel visible as a dot rather than nothing.
            canvas_draw_dot(canvas, x, y_bottom);
        }

        if(model->peak_valid && i == model->peak_channel) {
            // Marker tick indicating peak channel
            canvas_draw_dot(canvas, x, y_top > 2 ? y_top - 2 : y_top);
            canvas_draw_line(canvas, x - 1, y_top > 3 ? y_top - 3 : y_top, x + 1, y_top > 3 ? y_top - 3 : y_top);
        }
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    // Peak readout, right aligned so it does not collide with the axis ticks.
    char peak_str[20];
    if(model->peak_valid) {
        // Format the level as a string first: passing a float straight into
        // varargs promotes to double and trips the build's -Wdouble-promotion.
        char level_str[8];
        snprintf(level_str, sizeof(level_str), "%.0f", (double)model->rssi_dbm[model->peak_channel]);
        snprintf(
            peak_str,
            sizeof(peak_str),
            "%.3fM %sdBm",
            (double)model->frequencies[model->peak_channel] / 1000000,
            level_str);
    } else {
        snprintf(peak_str, sizeof(peak_str), "scanning...");
    }
    canvas_draw_str_aligned(canvas, 126, 0, AlignRight, AlignTop, peak_str);

    canvas_draw_str_aligned(canvas, 2, 0, AlignLeft, AlignTop, model->ext_module ? "E07" : "INT");

    // Lowest and highest swept frequency under the axis.
    if(model->count > 1) {
        uint32_t lowest = model->frequencies[0];
        uint32_t highest = model->frequencies[0];
        for(uint8_t i = 1; i < model->count; ++i) {
            if(model->frequencies[i] < lowest) lowest = model->frequencies[i];
            if(model->frequencies[i] > highest) highest = model->frequencies[i];
        }

        char edge_str[16];
        snprintf(edge_str, sizeof(edge_str), "%.2f", (double)lowest / 1000000);
        canvas_draw_str(canvas, PLOT_LEFT_X, 51, edge_str);
        snprintf(edge_str, sizeof(edge_str), "%.2f", (double)highest / 1000000);
        canvas_draw_str_aligned(canvas, PLOT_RIGHT_X, 51, AlignRight, AlignBottom, edge_str);

        char mid_str[16];
        snprintf(
            mid_str,
            sizeof(mid_str),
            "%.2f",
            (double)((uint32_t)lowest + (uint32_t)highest) / 2000000);
        canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, mid_str);
    }

    elements_button_left(canvas, "Back");
    elements_button_center(canvas, model->running ? "Pause" : "Run");
    elements_button_right(canvas, "Tune");
}

static bool ham_view_scan_input(InputEvent* event, void* context) {
    furi_assert(context);
    HamRadioViewScan* instance = context;
    HamViewScanCallback callback = instance->callback;
    void* callback_context = instance->context;

    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyOk:
        callback(HamRadioCustomEventSweepToggle, callback_context);
        return true;
    case InputKeyRight:
        callback(HamRadioCustomEventTuneSelected, callback_context);
        return true;
    default:
        return false;
    }
}

HamRadioViewScan* ham_view_scan_alloc(HamViewScanCallback callback, void* context) {
    furi_assert(callback);

    HamRadioViewScan* instance = malloc(sizeof(HamRadioViewScan));
    instance->callback = callback;
    instance->context = context;

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(HamViewScanModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, ham_view_scan_draw);
    view_set_input_callback(instance->view, ham_view_scan_input);

    with_view_model(
        instance->view,
        HamViewScanModel * model,
        {
            model->count = 0;
            model->peak_valid = false;
        },
        true);

    return instance;
}

void ham_view_scan_free(HamRadioViewScan* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* ham_view_scan_get_view(HamRadioViewScan* instance) {
    furi_assert(instance);
    return instance->view;
}

void ham_view_scan_set_channels(HamRadioViewScan* instance, const uint32_t* frequencies, uint8_t count) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        HamViewScanModel * model,
        {
            uint8_t copied = MIN(count, HAM_SCAN_MAX_CHANNELS);
            for(uint8_t i = 0; i < copied; ++i) {
                model->frequencies[i] = frequencies[i];
                // Reset the level so a stale reading is not attributed to a
                // different frequency after a plan edit.
                model->rssi_dbm[i] = HAM_SCAN_FLOOR_DBM;
            }
            model->count = copied;
            model->peak_valid = false;
        },
        true);
}

void ham_view_scan_set_result(HamRadioViewScan* instance, const HamScanResult* result) {
    furi_assert(instance);
    furi_assert(result);

    with_view_model(
        instance->view,
        HamViewScanModel * model,
        {
            for(uint8_t i = 0; i < result->channel_count && i < HAM_SCAN_MAX_CHANNELS; ++i) {
                model->frequencies[i] = result->frequencies[i];
                model->rssi_dbm[i] = result->rssi_dbm[i];
            }
            if(result->channel_count <= HAM_SCAN_MAX_CHANNELS) {
                model->count = result->channel_count;
            }
            model->peak_channel = result->peak_channel;
            model->peak_valid = result->peak_valid;
        },
        true);
}

uint8_t ham_view_scan_get_peak(HamRadioViewScan* instance) {
    furi_assert(instance);
    uint8_t peak = 0;
    with_view_model(instance->view, HamViewScanModel * model, { peak = model->peak_channel; }, false);
    return peak;
}

uint32_t ham_view_scan_get_frequency(HamRadioViewScan* instance, uint8_t index) {
    furi_assert(instance);
    uint32_t frequency = 0;
    with_view_model(
        instance->view,
        HamViewScanModel * model,
        {
            if(index < model->count) {
                frequency = model->frequencies[index];
            }
        },
        false);
    return frequency;
}

void ham_view_scan_set_module(HamRadioViewScan* instance, bool external) {
    furi_assert(instance);
    with_view_model(instance->view, HamViewScanModel * model, { model->ext_module = external; }, false);
}

void ham_view_scan_set_running(HamRadioViewScan* instance, bool running) {
    furi_assert(instance);
    with_view_model(instance->view, HamViewScanModel * model, { model->running = running; }, true);
}
