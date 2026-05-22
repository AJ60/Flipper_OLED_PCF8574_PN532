#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

// I2C конфигурация
#define I2C_BUS &furi_hal_i2c_handle_external
#define I2C_TIMEOUT 100

// Конфигурация регионов
typedef struct {
    const char* name;
    float min_freq;
    float max_freq;
    float step;
} RegionConfig;

static const RegionConfig regions[] = {
    {"Europe", 87.5f, 108.0f, 0.1f},
    {"USA", 88.0f, 108.0f, 0.2f},
    {"Japan", 76.0f, 95.0f, 0.1f}
};
static const size_t REGION_COUNT = COUNT_OF(regions);

// Типы экранов приложения
typedef enum {
    SCREEN_MAIN,
    SCREEN_REGION,
    SCREEN_FREQUENCY,
    SCREEN_INIT,
} AppScreen;

// Состояние приложения
typedef struct {
    AppScreen current_screen;
    size_t region_index;
    float frequency;
    bool init_in_progress;
    char init_status[64];
} FMTXState;

// Основная структура приложения
typedef struct {
    FMTXState state;
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriMutex* state_mutex;
    NotificationApp* notifications;
} FMTXApp;

// Поиск I2C устройства
static uint8_t find_i2c_device() {
    furi_hal_i2c_acquire(I2C_BUS);
    
    for(uint8_t addr = 0x08; addr <= 0x77; addr++) {
        // Проверяем готовность устройства с 7-битным адресом
        if(furi_hal_i2c_is_device_ready(I2C_BUS, addr << 1, I2C_TIMEOUT)) {
            furi_hal_i2c_release(I2C_BUS);
            return addr;
        }
    }
    
    furi_hal_i2c_release(I2C_BUS);
    return 0xFF; // Специальное значение, означающее, что устройство не найдено
}

// Отправка команды инициализации через I2C
static bool send_fmtx_init(uint8_t addr, uint16_t freq_khz) {
    furi_hal_i2c_acquire(I2C_BUS);
    
    // Подготовка команды инициализации
    uint8_t cmd[4] = {
        0x01, // Регистр настройки частоты
        (freq_khz >> 8) & 0xFF,
        freq_khz & 0xFF,
        0x00  // Регистр настройки региона
    };
    
    // Отправка команды
    bool success = furi_hal_i2c_tx(
        I2C_BUS,
        addr << 1,  // Преобразуем 7-битный адрес в 8-битный
        cmd,
        sizeof(cmd),
        I2C_TIMEOUT
    );
    
    furi_hal_i2c_release(I2C_BUS);
    return success;
}

// Функция отрисовки
void fmtx_draw_callback(Canvas* canvas, void* ctx) {
    FMTXApp* app = (FMTXApp*)ctx;
    
    if(furi_mutex_acquire(app->state_mutex, 200) != FuriStatusOk) {
        return;
    }
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Рамка
    canvas_draw_rframe(canvas, 2, 2, 124, 60, 3);
    
    switch(app->state.current_screen) {
        case SCREEN_MAIN:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "KT0803 Init");
            
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Set frequency on module to");
            canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, "transmit AUX signal");
            
            // Кнопка "Configure"
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "[ Configure ]");
            break;
            
        case SCREEN_REGION:
            canvas_set_font(canvas, FontPrimary);
            // Заголовок сдвинут вверх на 8 пикселей
            canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Select Region");
            
            canvas_set_font(canvas, FontSecondary);
            for(size_t i = 0; i < REGION_COUNT; i++) {
                if(i == app->state.region_index) {
                    // Указатель остается на месте
                    canvas_draw_str(canvas, 10, 28 + i * 14, ">");
                }
                // Текст регионов поднят на 8 пикселей вверх
                canvas_draw_str_aligned(canvas, 20, 20 + i * 14, AlignLeft, AlignTop, regions[i].name);
            }
            break;
            
        case SCREEN_FREQUENCY:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Set Frequency");
            
            canvas_set_font(canvas, FontSecondary);
            const RegionConfig* region = &regions[app->state.region_index];
            char freq_text[32];
            snprintf(freq_text, sizeof(freq_text), "Freq: %.1f MHz", (double)app->state.frequency);
            // Информация о частоте сдвинута вверх
            canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, freq_text);
            
            snprintf(freq_text, sizeof(freq_text), "(%s) %.1f-%.1f MHz", 
                    region->name, (double)region->min_freq, (double)region->max_freq);
            // Информация о регионе сдвинута вверх
            canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, freq_text);
            
            // Кнопки управления
            canvas_draw_str(canvas, 5, 50, "L:Back");
            canvas_draw_str(canvas, 50, 50, "U/D:Set");
            canvas_draw_str(canvas, 95, 50, "OK:Next");
            break;
            
        case SCREEN_INIT:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Init Status");
            
            canvas_set_font(canvas, FontSecondary);
            if(app->state.init_in_progress) {
                canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Initializing...");
            } else {
                // Разбиваем статус на строки, если он слишком длинный
                char status_line[32];
                strncpy(status_line, app->state.init_status, sizeof(status_line) - 1);
                status_line[sizeof(status_line) - 1] = '\0';
                // Статус сдвинут вверх
                canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, status_line);
                
                if(strlen(app->state.init_status) > sizeof(status_line) - 1) {
                    strncpy(status_line, app->state.init_status + sizeof(status_line) - 1, sizeof(status_line) - 1);
                    status_line[sizeof(status_line) - 1] = '\0';
                    // Вторая строка статуса сдвинута вверх
                    canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, status_line);
                }
            }
            
            // Кнопка "Start Init"
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "[ Start Init ]");
            break;
    }
    
    furi_mutex_release(app->state_mutex);
}

// Обработчик ввода
void fmtx_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FMTXApp* app = ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

// Основной цикл приложения
int32_t fmtx_i2c_init_app(void* p) {
    UNUSED(p);
    
    // Создание очереди событий
    FMTXApp* app = malloc(sizeof(FMTXApp));
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    
    // Создание мьютекса для состояния
    app->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    
    // Инициализация состояния
    if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
        app->state.current_screen = SCREEN_MAIN;
        app->state.region_index = 0; // Europe по умолчанию
        app->state.frequency = 100.0f;
        app->state.init_in_progress = false;
        strcpy(app->state.init_status, "Ready to set frequency");
        furi_mutex_release(app->state_mutex);
    }
    
    // Создание ViewPort
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, fmtx_draw_callback, app);
    view_port_input_callback_set(app->view_port, fmtx_input_callback, app);
    
    // Регистрация ViewPort в GUI
    app->gui = furi_record_open("gui");
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    // Инициализация уведомлений
    app->notifications = furi_record_open("notification");
    
    // Основной цикл обработки событий
    InputEvent event;
    while(1) {
        FuriStatus event_status = furi_message_queue_get(app->event_queue, &event, 100);
        
        // Обработка события таймаута
        if(event_status != FuriStatusOk) {
            view_port_update(app->view_port);
            continue;
        }
        
        // Обработка кнопки Back
        if(event.key == InputKeyBack) {
            if(event.type == InputTypeShort) {
                if(app->state.current_screen == SCREEN_MAIN) {
                    break; // Выход из приложения
                } else {
                    if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
                        app->state.current_screen = SCREEN_MAIN;
                        furi_mutex_release(app->state_mutex);
                    }
                }
            }
            continue;
        }
        
        // Обработка ввода только при отпускании кнопки
        if(event.type != InputTypeRelease) {
            continue;
        }
        
        // Обработка ввода в зависимости от текущего экрана
        if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) != FuriStatusOk) {
            continue;
        }
        
        switch(app->state.current_screen) {
            case SCREEN_MAIN:
                if(event.key == InputKeyOk) {
                    app->state.current_screen = SCREEN_REGION;
                }
                break;
                
            case SCREEN_REGION:
                if(event.key == InputKeyUp) {
                    if(app->state.region_index > 0) {
                        app->state.region_index--;
                    }
                } else if(event.key == InputKeyDown) {
                    if(app->state.region_index < REGION_COUNT - 1) {
                        app->state.region_index++;
                    }
                } else if(event.key == InputKeyOk) {
                    app->state.current_screen = SCREEN_FREQUENCY;
                }
                break;
                
            case SCREEN_FREQUENCY:
                if(event.key == InputKeyUp) {
                    const RegionConfig* region = &regions[app->state.region_index];
                    app->state.frequency += region->step;
                    if(app->state.frequency > region->max_freq) {
                        app->state.frequency = region->max_freq;
                    }
                } else if(event.key == InputKeyDown) {
                    const RegionConfig* region = &regions[app->state.region_index];
                    app->state.frequency -= region->step;
                    if(app->state.frequency < region->min_freq) {
                        app->state.frequency = region->min_freq;
                    }
                } else if(event.key == InputKeyLeft) {
                    app->state.current_screen = SCREEN_REGION;
                } else if(event.key == InputKeyOk) {
                    app->state.current_screen = SCREEN_INIT;
                }
                break;
                
            case SCREEN_INIT:
                if(event.key == InputKeyOk) {
                    app->state.init_in_progress = true;
                    strcpy(app->state.init_status, "Initializing...");
                    furi_mutex_release(app->state_mutex);
                    view_port_update(app->view_port);
                    
                    uint8_t i2c_addr = find_i2c_device();
                    
                    if(i2c_addr == 0xFF) {
                        if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
                            strcpy(app->state.init_status, "No KT0803 found!\nConnect module first");
                            notification_message(app->notifications, &sequence_error);
                            app->state.init_in_progress = false;
                            furi_mutex_release(app->state_mutex);
                        }
                    } else {
                        uint16_t freq_khz = (uint16_t)(app->state.frequency * 1000.0f);
                        
                        if(send_fmtx_init(i2c_addr, freq_khz)) {
                            char success_msg[64];
                            snprintf(success_msg, sizeof(success_msg), "Success!\nDevice: 0x%02X", i2c_addr);
                            if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
                                strcpy(app->state.init_status, success_msg);
                                notification_message(app->notifications, &sequence_blink_green_100);
                                app->state.init_in_progress = false;
                                furi_mutex_release(app->state_mutex);
                            }
                        } else {
                            if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
                                strcpy(app->state.init_status, "I2C write failed");
                                notification_message(app->notifications, &sequence_error);
                                app->state.init_in_progress = false;
                                furi_mutex_release(app->state_mutex);
                            }
                        }
                    }
                }
                break;
        }
        
        furi_mutex_release(app->state_mutex);
        view_port_update(app->view_port);
    }
    
    // Очистка ресурсов
    furi_record_close("notification");
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->state_mutex);
    free(app);
    
    return 0;
}
