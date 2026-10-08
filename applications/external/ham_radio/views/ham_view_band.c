#include "ham_view_band.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>

#define TAG "HamViewBand"

#define FRAME_HEIGHT     12
#define MAX_VISIBLE_ROWS 4
#define LIST_TOP_Y       2
#define LIST_LEFT_X      2
#define LIST_RIGHT_X     118

typedef struct {
    HamBand band;
    uint8_t selected;
    uint8_t scroll;
    bool tx_armed;
    uint8_t duty_used;
    bool ext_module;
} HamViewBandModel;

struct HamRadioViewBand {
    View* view;
    HamViewBandCallback callback;
    void* context;
};

static void ham_view_band_scroll_into_view(HamViewBandModel* model) {
    if(model->selected < model->scroll) {
        model->scroll = model->selected;
    }
    if(model->selected >= model->scroll + MAX_VISIBLE_ROWS) {
        model->scroll = model->selected + 1 - MAX_VISIBLE_ROWS;
    }
}

static void ham_view_band_draw(Canvas* canvas, void* context) {
    furi_assert(context);
    HamViewBandModel* model = context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->band.count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "No frequencies");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Add the ones your");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "licence covers");
        elements_button_left(canvas, "Add");
        return;
    }

    const uint8_t rows = MIN(model->band.count, MAX_VISIBLE_ROWS);
    for(uint8_t row = 0; row < rows; ++row) {
        const uint8_t index = row + model->scroll;
        if(index >= model->band.count) break;

        const HamBandEntry* entry = &model->band.entries[index];
        const int32_t y = LIST_TOP_Y + row * FRAME_HEIGHT;
        const bool selected = (index == model->selected);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 3, LIST_RIGHT_X, FRAME_HEIGHT);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        char label[HAM_BAND_LABEL_SIZE + 1];
        memcpy(label, entry->label, sizeof(label) - 1);
        label[HAM_BAND_LABEL_SIZE] = '\0';

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, LIST_LEFT_X, y + 6, label);

        char freq_str[16];
        snprintf(freq_str, sizeof(freq_str), "%.3f", ((double)entry->frequency / 1000000));
        canvas_draw_str(canvas, 56, y + 6, freq_str);

        // A repeater entry transmits elsewhere, so mark it distinctly.
        canvas_draw_str(canvas, 100, y + 6, (entry->usage & HamBandUsageTransmit) ? "TX" : "RX");
        if(entry->tx_frequency != 0) {
            canvas_draw_str(canvas, 112, y + 6, "R");
        }
    }

    if(model->band.count > rows) {
        elements_scrollbar_pos(
            canvas, 128, LIST_TOP_Y - 3, rows * FRAME_HEIGHT, model->selected, model->band.count);
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char status_str[20];
    const uint8_t duty = model->duty_used;
    if(model->tx_armed) {
        snprintf(
            status_str,
            sizeof(status_str),
            "%u/%u TX %u%%%s",
            model->selected + 1,
            model->band.count,
            duty,
            model->ext_module ? "E" : "I");
    } else {
        snprintf(status_str, sizeof(status_str), "%u/%u TX OFF%s", model->selected + 1, model->band.count, model->ext_module ? " E" : "");
    }
    canvas_draw_str(canvas, 2, 63, status_str);

    elements_button_left(canvas, "Add");
    elements_button_center(canvas, "Tune");
    elements_button_right(canvas, "Del");
}

static bool ham_view_band_input(InputEvent* event, void* context) {
    furi_assert(context);
    HamRadioViewBand* instance = context;
    HamViewBandCallback callback = instance->callback;
    void* callback_context = instance->context;

    uint8_t count = 0;
    with_view_model(instance->view, HamViewBandModel * model, { count = model->band.count; }, false);

    if(count == 0) {
        // With an empty plan the only meaningful action is adding one.
        if((event->type == InputTypeShort && event->key == InputKeyLeft) ||
           (event->type == InputTypeLong && event->key == InputKeyOk)) {
            callback(HamRadioCustomEventAddEntry, callback_context);
        }
        return true;
    }

    if(event->type == InputTypeLong && event->key == InputKeyRight) {
        callback(HamRadioCustomEventRemoveEntry, callback_context);
        return true;
    }

    switch(event->type) {
    case InputTypeShort:
    case InputTypeRepeat:
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                instance->view,
                HamViewBandModel * model,
                {
                    if(model->selected > 0) model->selected--;
                    ham_view_band_scroll_into_view(model);
                },
                true);
            return true;
        case InputKeyDown:
            with_view_model(
                instance->view,
                HamViewBandModel * model,
                {
                    if(model->selected + 1 < model->band.count) model->selected++;
                    ham_view_band_scroll_into_view(model);
                },
                true);
            return true;
        case InputKeyOk:
            callback(HamRadioCustomEventTuneSelected, callback_context);
            return true;
        case InputKeyLeft:
            callback(HamRadioCustomEventAddEntry, callback_context);
            return true;
        case InputKeyRight:
            callback(HamRadioCustomEventRemoveEntry, callback_context);
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

HamRadioViewBand* ham_view_band_alloc(HamViewBandCallback callback, void* context) {
    furi_assert(callback);

    HamRadioViewBand* instance = malloc(sizeof(HamRadioViewBand));
    instance->callback = callback;
    instance->context = context;

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(HamViewBandModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, ham_view_band_draw);
    view_set_input_callback(instance->view, ham_view_band_input);

    with_view_model(
        instance->view,
        HamViewBandModel * model,
        {
            ham_band_init(&model->band);
            model->selected = 0;
            model->scroll = 0;
        },
        true);

    return instance;
}

void ham_view_band_free(HamRadioViewBand* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* ham_view_band_get_view(HamRadioViewBand* instance) {
    furi_assert(instance);
    return instance->view;
}

void ham_view_band_set_plan(HamRadioViewBand* instance, const HamBand* band) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        HamViewBandModel * model,
        {
            model->band = *band;
            if(model->band.count == 0) {
                model->selected = 0;
                model->scroll = 0;
            } else {
                if(model->selected >= model->band.count) {
                    model->selected = model->band.count - 1;
                }
                ham_view_band_scroll_into_view(model);
            }
        },
        true);
}

void ham_view_band_set_selected(HamRadioViewBand* instance, uint8_t index) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        HamViewBandModel * model,
        {
            model->selected = (index < model->band.count) ? index : 0;
            ham_view_band_scroll_into_view(model);
        },
        true);
}

uint8_t ham_view_band_get_selected(HamRadioViewBand* instance) {
    furi_assert(instance);
    uint8_t selected = 0;
    with_view_model(instance->view, HamViewBandModel * model, { selected = model->selected; }, false);
    return selected;
}

bool ham_view_band_get_selected_entry(HamRadioViewBand* instance, HamBandEntry* out_entry) {
    furi_assert(instance);
    furi_assert(out_entry);

    bool found = false;
    with_view_model(
        instance->view,
        HamViewBandModel * model,
        {
            if(model->selected < model->band.count) {
                *out_entry = model->band.entries[model->selected];
                found = true;
            }
        },
        false);
    return found;
}

void ham_view_band_set_tx_state(HamRadioViewBand* instance, bool armed, uint8_t duty_used) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        HamViewBandModel * model,
        {
            model->tx_armed = armed;
            model->duty_used = duty_used;
        },
        false);
}

void ham_view_band_set_module(HamRadioViewBand* instance, bool external) {
    furi_assert(instance);
    with_view_model(instance->view, HamViewBandModel * model, { model->ext_module = external; }, false);
}
