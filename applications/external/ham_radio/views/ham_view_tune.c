#include "ham_view_tune.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>

#define TAG "HamViewTune"

#define METER_LEFT_X  6
#define METER_RIGHT_X 121
#define METER_TOP_Y   30
#define METER_HEIGHT  10
/** Weakest level the meter represents, matching the sweep floor. */
#define METER_FLOOR_DBM (-100.0f)
#define METER_CEIL_DBM  (-20.0f)

typedef struct {
    uint32_t frequency;
    char label[HAM_BAND_LABEL_SIZE + 1];
    float rssi_dbm;
    uint8_t lqi;
    bool tx_armed;
    char tx_state[22];
} HamViewTuneModel;

struct HamRadioViewTune {
    View* view;
    HamViewTuneCallback callback;
    void* context;
};

/** Conventional S-meter mapping in dB: S0..S9 in 2 dB steps from -121 dBm, then
 * +20..+60 dB over the same steps. Index 0 is S0, index 9 is S9, and anything
 * past that renders as A, B, ... */
static const int ham_view_tune_s_scale[] = {
    -121, -119, -117, -115, -113, -111, -109, -107, -105, -103,
    -83,   -81,   -79,   -77,   -75,   -73,   -71,   -69,   -67,   -65,
    -45,   -43,   -41,   -39,   -37,   -35,   -33,   -31,   -29,   -27,
    -5,    -3,    -1,    1,     3,     5,     7,     9,     11,    13,
    35,    37,    39,    41,    43,    45,    47,    49,    51,    53,
};

/** Write the S-meter reading into a caller-owned buffer. Returns out. */
static const char* ham_view_tune_s_label(float rssi_dbm, char* out, size_t out_size) {
    const int last = (int)(sizeof(ham_view_tune_s_scale) / sizeof(ham_view_tune_s_scale[0])) - 1;

    char symbol = '.';
    for(int i = last; i >= 0; --i) {
        if(rssi_dbm >= (float)ham_view_tune_s_scale[i]) {
            // Indices 0..9 are S0..S9; past that read +20..+60 as A..Z.
            symbol = (i < 10) ? (char)('0' + i) : (char)('A' + (i - 10));
            break;
        }
    }

    snprintf(out, out_size, "%c", symbol);
    return out;
}

static uint8_t ham_view_tune_meter_width(float rssi_dbm) {
    if(rssi_dbm <= METER_FLOOR_DBM) return 0;
    if(rssi_dbm >= METER_CEIL_DBM) return (uint8_t)(METER_RIGHT_X - METER_LEFT_X);

    const float span = (float)(METER_CEIL_DBM - METER_FLOOR_DBM);
    const float scaled = (rssi_dbm - METER_FLOOR_DBM) / span * (float)(METER_RIGHT_X - METER_LEFT_X);
    return (uint8_t)CLAMP((int)scaled, 0, METER_RIGHT_X - METER_LEFT_X);
}

static void ham_view_tune_draw(Canvas* canvas, void* context) {
    furi_assert(context);
    HamViewTuneModel* model = context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    // Frequency is the headline number on this screen.
    char freq_str[20];
    snprintf(freq_str, sizeof(freq_str), "%.5f", (double)model->frequency / 1000000);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, freq_str);
    canvas_set_font(canvas, FontSecondary);

    if(model->label[0] != '\0') {
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, model->label);
    }

    // S-meter outline with scale ticks along the bottom edge.
    const uint8_t width = ham_view_tune_meter_width(model->rssi_dbm);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, METER_LEFT_X, METER_TOP_Y, METER_RIGHT_X - METER_LEFT_X, METER_HEIGHT);
    for(uint8_t x = METER_LEFT_X; x <= METER_RIGHT_X; x += 8) {
        canvas_draw_line(canvas, x, METER_TOP_Y + METER_HEIGHT, x, METER_TOP_Y + METER_HEIGHT + 2);
    }

    if(width > 0) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, METER_LEFT_X + 1, METER_TOP_Y + 1, width, METER_HEIGHT - 2);
    }

    char s_label[2];
    char level_str[28];
    snprintf(
        level_str,
        sizeof(level_str),
        "S%s  %.0f dBm  LQI %u",
        ham_view_tune_s_label(model->rssi_dbm, s_label, sizeof(s_label)),
        (double)model->rssi_dbm,
        model->lqi);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, level_str);

    // Transmit readiness, or the reason it is not.
    canvas_set_font(canvas, FontSecondary);
    if(model->tx_state[0] != '\0') {
        const bool armed = model->tx_armed;
        const int32_t str_width = canvas_string_width(canvas, model->tx_state);
        const int32_t x = 64 - str_width / 2;

        if(armed) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, x - 3, 55, str_width + 6, 9);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, model->tx_state);
        } else {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, model->tx_state);
        }
    }

    elements_button_left(canvas, "Back");
}

static bool ham_view_tune_input(InputEvent* event, void* context) {
    furi_assert(context);
    HamRadioViewTune* instance = context;

    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyOk:
        instance->callback(HamRadioCustomEventTuneOk, instance->context);
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

HamRadioViewTune* ham_view_tune_alloc(HamViewTuneCallback callback, void* context) {
    furi_assert(callback);

    HamRadioViewTune* instance = malloc(sizeof(HamRadioViewTune));
    instance->callback = callback;
    instance->context = context;

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(HamViewTuneModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, ham_view_tune_draw);
    view_set_input_callback(instance->view, ham_view_tune_input);

    with_view_model(
        instance->view,
        HamViewTuneModel * model,
        {
            model->frequency = 433920000;
            model->rssi_dbm = HAM_SCAN_FLOOR_DBM;
        },
        true);

    return instance;
}

void ham_view_tune_free(HamRadioViewTune* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* ham_view_tune_get_view(HamRadioViewTune* instance) {
    furi_assert(instance);
    return instance->view;
}

void ham_view_tune_set_frequency(HamRadioViewTune* instance, uint32_t frequency, const char* label) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        HamViewTuneModel * model,
        {
            model->frequency = frequency;
            memset(model->label, 0, sizeof(model->label));
            if(label != NULL) {
                strncpy(model->label, label, sizeof(model->label) - 1);
            }
            model->rssi_dbm = HAM_SCAN_FLOOR_DBM;
        },
        true);
}

void ham_view_tune_set_level(HamRadioViewTune* instance, float rssi_dbm, uint8_t lqi) {
    furi_assert(instance);

    // Polling the meter faster than the GUI redraws would only burn CPU, so
    // only commit a change large enough to matter.
    const float epsilon = 0.5f;
    with_view_model(
        instance->view,
        HamViewTuneModel * model,
        {
            const float delta = model->rssi_dbm - rssi_dbm;
            if((delta < 0.0f ? -delta : delta) >= epsilon || model->lqi != lqi) {
                model->rssi_dbm = rssi_dbm;
                model->lqi = lqi;
            }
        },
        false);
}

void ham_view_tune_set_tx_state(HamRadioViewTune* instance, bool armed, const char* tx_state) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        HamViewTuneModel * model,
        {
            memset(model->tx_state, 0, sizeof(model->tx_state));
            if(tx_state != NULL) {
                strncpy(model->tx_state, tx_state, sizeof(model->tx_state) - 1);
            }
            model->tx_armed = armed;
        },
        false);
}
