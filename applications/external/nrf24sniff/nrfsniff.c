#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <stdlib.h>

#include <nrf24.h>
#include <toolbox/stream/file_stream.h>

#include "stdstring.h"

#define LOGITECH_MAX_CHANNEL 85
#define COUNT_THRESHOLD      2
#define DEFAULT_SAMPLE_TIME  4000
#define MAX_ADDRS            100
#define MAX_CONFIRMED        32

#define NRFSNIFF_APP_PATH_FOLDER STORAGE_APP_DATA_PATH_PREFIX
#define NRFSNIFF_APP_FILENAME    "addresses.txt"
#define TAG                      "nrfsniff"

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    FuriMutex* mutex;
    bool is_nrf24_connected;
} PluginState;

char rate_text_fmt[] = "Transfer rate: %dMbps";
char sample_text_fmt[] = "Sample Time: %d ms";
char channel_text_fmt[] = "Channel: %d    Sniffing: %s";
char preamble_text_fmt[] = "Preamble: %02X";
char sniff_text_fmt[] = "Found: %d       Unique: %u";
char addresses_header_text[] = "Address,rate";
char sniffed_address_fmt[] = "%s,%d";
char rate_text[46];
char channel_text[38];
char sample_text[32];
char preamble_text[14];
char sniff_text[38];
char sniffed_address[14];

uint8_t target_channel = 0;
uint32_t found_count = 0;
uint32_t unique_saved_count = 0;
uint32_t sample_time = DEFAULT_SAMPLE_TIME;
uint8_t target_rate = 8; // rate can be either 8 (2Mbps) or 0 (1Mbps)
uint8_t target_preamble[] = {0xAA, 0x00};
uint8_t sniffing_state = false;
char top_address[12];

uint8_t candidates[MAX_ADDRS][5] = {0}; // last 100 sniffed addresses
uint32_t counts[MAX_ADDRS];
uint8_t confirmed[MAX_CONFIRMED][5] = {0}; // first 32 confirmed addresses
uint8_t confirmed_idx = 0;
uint32_t total_candidates = 0;
uint32_t candidate_idx = 0;

static int get_addr_index(uint8_t* addr, uint8_t addr_size) {
    for(uint32_t i = 0; i < total_candidates; i++) {
        uint8_t* arr_item = candidates[i];
        if(!memcmp(arr_item, addr, addr_size)) return i;
    }

    return -1;
}

static int get_highest_idx() {
    uint32_t highest = 0;
    int highest_idx = 0;
    for(uint32_t i = 0; i < total_candidates; i++) {
        if(counts[i] > highest) {
            highest = counts[i];
            highest_idx = i;
        }
    }

    return highest_idx;
}

// if array is full, start over from beginning
static void insert_addr(uint8_t* addr, uint8_t addr_size) {
    if(candidate_idx >= MAX_ADDRS) candidate_idx = 0;

    memcpy(candidates[candidate_idx], addr, addr_size);
    counts[candidate_idx] = 1;
    if(total_candidates < MAX_ADDRS) total_candidates++;
    candidate_idx++;
}

static void render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    const PluginState* plugin_state = ctx;
    if(furi_mutex_acquire(plugin_state->mutex, 100) != FuriStatusOk) {
        return;
    }

    uint8_t rate = 2;
    char sniffing[] = "Yes";

    // border around the edge of the screen
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontSecondary);

    if(target_rate == 0) rate = 1;

    if(!sniffing_state) strcpy(sniffing, "No");

    snprintf(rate_text, sizeof(rate_text), rate_text_fmt, (int)rate);
    snprintf(channel_text, sizeof(channel_text), channel_text_fmt, (int)target_channel, sniffing);
    snprintf(sample_text, sizeof(sample_text), sample_text_fmt, (int)sample_time);
    //snprintf(preamble_text, sizeof(preamble_text), preamble_text_fmt, target_preamble[0]);
    snprintf(sniff_text, sizeof(sniff_text), sniff_text_fmt, found_count, unique_saved_count);
    snprintf(
        sniffed_address, sizeof(sniffed_address), sniffed_address_fmt, top_address, (int)rate);
    canvas_draw_str_aligned(canvas, 10, 10, AlignLeft, AlignBottom, rate_text);
    canvas_draw_str_aligned(canvas, 10, 20, AlignLeft, AlignBottom, sample_text);
    canvas_draw_str_aligned(canvas, 10, 30, AlignLeft, AlignBottom, channel_text);
    //canvas_draw_str_aligned(canvas, 10, 30, AlignLeft, AlignBottom, preamble_text);
    canvas_draw_str_aligned(canvas, 10, 40, AlignLeft, AlignBottom, sniff_text);

    if(!plugin_state->is_nrf24_connected) {
        canvas_draw_str_aligned(canvas, 10, 50, AlignLeft, AlignBottom, "NRF24 NOT CONNECTED!");
        canvas_draw_str_aligned(canvas, 10, 60, AlignLeft, AlignBottom, "CSN: Pin 7 / 4  CE: Pin 6");
    } else {
        canvas_draw_str_aligned(canvas, 30, 50, AlignLeft, AlignBottom, addresses_header_text);
        canvas_draw_str_aligned(canvas, 30, 60, AlignLeft, AlignBottom, sniffed_address);
    }

    furi_mutex_release(plugin_state->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, 0);
}

static void hexlify(uint8_t* in, uint8_t size, char* out) {
    // Bug #12 fix: sizeof(ptr+offset) is always pointer size (4 bytes on ARM), NOT the
    // remaining buffer length. Use index-based snprintf with a known 2-char step.
    memset(out, 0, size * 2 + 1);
    for(int i = 0; i < size; i++)
        snprintf(out + i * 2, 3, "%02X", in[i]);
}

static bool save_addr_to_file(
    Storage* storage,
    uint8_t* data,
    uint8_t size,
    NotificationApp* notification) {
    size_t file_size = 0;
    uint8_t linesize = 0;
    char filepath[42] = {0};
    char addrline[14] = {0};
    char ending[4];
    uint8_t* file_contents;
    uint8_t rate = 1;
    Stream* stream = file_stream_alloc(storage);

    if(target_rate == 8) rate = 2;
    snprintf(ending, sizeof(ending), ",%d\n", rate);
    hexlify(data, size, addrline);
    nrf_strcat(addrline, ending);
    linesize = strlen(addrline);
    strcpy(filepath, NRFSNIFF_APP_PATH_FOLDER);
    nrf_strcat(filepath, "/");
    nrf_strcat(filepath, NRFSNIFF_APP_FILENAME);
    // check if address already exists in file
    if(file_stream_open(stream, filepath, FSAM_READ_WRITE, FSOM_OPEN_APPEND)) {
        // Bug #13 fix: seek after open (seeking a never-opened stream is undefined).
        stream_seek(stream, 0, StreamOffsetFromStart);
        bool found = false;
        file_size = stream_size(stream);
        stream_seek(stream, 0, StreamOffsetFromStart);
        if(file_size > 0) {
            file_contents = malloc(file_size + 1);
            memset(file_contents, 0, file_size + 1);
            if(stream_read(stream, file_contents, file_size) > 0) {
                char* line = nrf_strtok((char*)file_contents, "\n");

                while(line != NULL) {
                    if(!memcmp(line, addrline, 12)) {
                        found = true;
                        break;
                    }
                    line = nrf_strtok(NULL, "\n");
                }
            }
            free(file_contents);
        }

        if(found) {
            FURI_LOG_I(TAG, "Address exists in file. Ending save process.");
            stream_free(stream);
            return false;
        } else {
            if(stream_write(stream, (uint8_t*)addrline, linesize) != linesize) {
                FURI_LOG_I(TAG, "Failed to write bytes to file stream.");
                stream_free(stream);
                return false;
            } else {
                FURI_LOG_I(TAG, "Found a new address: %s", addrline);
                FURI_LOG_I(TAG, "Save successful!");

                notification_message(notification, &sequence_success);

                stream_free(stream);
                unique_saved_count++;
                return true;
            }
        }
    } else {
        FURI_LOG_I(TAG, "Cannot open file \"%s\"", filepath);
        stream_free(stream);
        return false;
    }
}

void alt_address(uint8_t* addr, uint8_t* altaddr) {
    uint8_t macmess_hi_b[4];
    uint32_t macmess_hi;
    uint8_t macmess_lo;
    uint8_t preserved;
    uint8_t tmpaddr[5];

    // swap bytes
    for(int i = 0; i < 5; i++)
        tmpaddr[i] = addr[4 - i];

    // get address into 32-bit and 8-bit variables
    memcpy(macmess_hi_b, tmpaddr, 4);
    macmess_lo = tmpaddr[4];

    macmess_hi = bytes_to_int32(macmess_hi_b, true);

    //preserve lowest bit from hi to shift to low
    preserved = macmess_hi & 1;
    macmess_hi >>= 1;
    macmess_lo >>= 1;
    macmess_lo = (preserved << 7) | macmess_lo;
    int32_to_bytes(macmess_hi, macmess_hi_b, true);
    memcpy(tmpaddr, macmess_hi_b, 4);
    tmpaddr[4] = macmess_lo;

    // swap bytes back
    for(int i = 0; i < 5; i++)
        altaddr[i] = tmpaddr[4 - i];
}

static bool previously_confirmed(uint8_t* addr) {
    bool found = false;
    for(int i = 0; i < MAX_CONFIRMED; i++) {
        if(!memcmp(confirmed[i], addr, 5)) {
            found = true;
            break;
        }
    }

    return found;
}

static void wrap_up(const FuriHalSpiBusHandle* handle, Storage* storage, NotificationApp* notification) {
    uint8_t ch;
    uint8_t addr[5];
    uint8_t altaddr[5];
    char trying[12];
    int idx;
    uint8_t rate = 0;
    if(target_rate == 8) rate = 2;

    nrf24_set_idle(handle);

    int max_candidates = 2; // Test at most the top 2 candidates to prevent UI lockup
    while(max_candidates-- > 0) {
        idx = get_highest_idx();
        if(counts[idx] < COUNT_THRESHOLD) break;

        counts[idx] = 0;
        memcpy(addr, candidates[idx], 5);
        hexlify(addr, 5, trying);
        FURI_LOG_I(TAG, "trying address %s", trying);
        ch = nrf24_find_channel(handle, addr, addr, 5, rate, 2, LOGITECH_MAX_CHANNEL, false);
        FURI_LOG_I(TAG, "find_channel returned %d", (int)ch);
        if(ch > LOGITECH_MAX_CHANNEL) {
            alt_address(addr, altaddr);
            hexlify(altaddr, 5, trying);
            FURI_LOG_I(TAG, "trying alternate address %s", trying);
            ch = nrf24_find_channel(
                handle, altaddr, altaddr, 5, rate, 2, LOGITECH_MAX_CHANNEL, false);
            FURI_LOG_I(TAG, "find_channel returned %d", (int)ch);
            memcpy(addr, altaddr, 5);
        }

        if(ch <= LOGITECH_MAX_CHANNEL) {
            hexlify(addr, 5, top_address);
            found_count++;
            save_addr_to_file(storage, addr, 5, notification);
            if(confirmed_idx < MAX_CONFIRMED) memcpy(confirmed[confirmed_idx++], addr, 5);
            break;
        }
        furi_delay_ms(1);
    }
    // Clear low count candidates so noise does not accumulate and cause long freezes
    memset(counts, 0, sizeof(counts));
}

static void clear_cache() {
    found_count = 0;
    unique_saved_count = 0;
    confirmed_idx = 0;
    candidate_idx = 0;
    target_channel = 2;
    total_candidates = 0;
    memset(candidates, 0, sizeof(candidates));
    memset(counts, 0, sizeof(counts));
    memset(confirmed, 0, sizeof(confirmed));
}

static void start_sniffing(const FuriHalSpiBusHandle* handle) {
    nrf24_init_promisc_mode(handle, target_channel, target_rate);
}


int32_t nrfsniff_app(void* p) {
    UNUSED(p);
    uint8_t address[5] = {0};
    uint32_t start = 0;
    hexlify(address, 5, top_address);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(16, sizeof(PluginEvent));
    PluginState* plugin_state = malloc(sizeof(PluginState));
    plugin_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    plugin_state->is_nrf24_connected = false;
    if(!plugin_state->mutex) {
        furi_message_queue_free(event_queue);
        FURI_LOG_E(TAG, "cannot create mutex\r\n");
        free(plugin_state);
        return 255;
    }

    uint8_t attempts = 0;
    bool otg_was_enabled = furi_hal_power_is_otg_enabled();
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }

    furi_delay_ms(100);

    nrf24_init();

    const FuriHalSpiBusHandle* active_handle = nrf24_get_handle();
    plugin_state->is_nrf24_connected = nrf24_check_connected(active_handle);
    if(plugin_state->is_nrf24_connected) {
        FURI_LOG_I(TAG, "NRF24 detected on handle %p", active_handle);
    } else {
        FURI_LOG_W(TAG, "NRF24 not connected");
    }

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, plugin_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_migrate(storage, EXT_PATH("nrfsniff"), NRFSNIFF_APP_PATH_FOLDER);
    storage_common_mkdir(storage, NRFSNIFF_APP_PATH_FOLDER);

    PluginEvent event;
    uint32_t last_redraw = 0;
    for(bool processing = true; processing;) {
        FuriStatus event_status =
            furi_message_queue_get(event_queue, &event, sniffing_state ? 0 : 100);

        bool needs_redraw = false;

        if(event_status == FuriStatusOk) {
            // key events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypeShort ||
                   (event.input.type == InputTypeRepeat &&
                    (event.input.key == InputKeyRight || event.input.key == InputKeyLeft))) {
                    needs_redraw = true;
                    switch(event.input.key) {
                    case InputKeyUp:
                        // toggle rate  1/2Mbps
                        if(!sniffing_state) {
                            target_rate = (target_rate == 0) ? 8 : 0;
                        }
                        break;
                    case InputKeyDown:
                        // toggle preamble
                        if(!sniffing_state) {
                            target_preamble[0] = (target_preamble[0] == 0x55) ? 0xAA : 0x55;
                            if(active_handle) {
                                nrf24_set_src_mac(active_handle, target_preamble, 2);
                            }
                        }
                        break;
                    case InputKeyRight:
                        sample_time += 500;
                        break;
                    case InputKeyLeft:
                        if(sample_time > 500) sample_time -= 500;
                        break;
                    case InputKeyOk:
                        // toggle sniffing or re-detect NRF24
                        if(!plugin_state->is_nrf24_connected) {
                            active_handle = nrf24_get_handle();
                            plugin_state->is_nrf24_connected = nrf24_check_connected(active_handle);
                        }

                        if(plugin_state->is_nrf24_connected) {
                            sniffing_state = !sniffing_state;
                            if(sniffing_state) {
                                clear_cache();
                                start_sniffing(active_handle);
                                start = furi_get_tick();
                            } else {
                                wrap_up(active_handle, storage, notification);
                            }
                        } else {
                            notification_message(notification, &sequence_error);
                        }

                        break;
                    case InputKeyBack:
                        if(active_handle && sniffing_state) {
                            wrap_up(active_handle, storage, notification);
                        }
                        processing = false;
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        if(sniffing_state && active_handle) {
            for(int i = 0; i < 20; i++) {
                if(nrf24_sniff_address(active_handle, 5, address)) {
                    furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);
                    if(!previously_confirmed(address)) {
                        int idx = get_addr_index(address, 5);
                        if(idx == -1)
                            insert_addr(address, 5);
                        else
                            counts[idx]++;

                        uint8_t* top_addr = candidates[get_highest_idx()];
                        hexlify(top_addr, 5, top_address);
                        needs_redraw = true;
                    }
                    furi_mutex_release(plugin_state->mutex);
                    break;
                }
                furi_delay_us(100);
            }

            if(furi_get_tick() - start >= sample_time) {
                target_channel++;
                if(target_channel > LOGITECH_MAX_CHANNEL) target_channel = 2;
                wrap_up(active_handle, storage, notification);
                start_sniffing(active_handle);
                start = furi_get_tick();
                needs_redraw = true;
            }
        }

        uint32_t now = furi_get_tick();
        if(needs_redraw || (sniffing_state && (now - last_redraw >= 150))) {
            view_port_update(view_port);
            last_redraw = now;
        }
    }

    clear_cache();
    sample_time = DEFAULT_SAMPLE_TIME;
    target_rate = 8; // rate can be either 8 (2Mbps) or 0 (1Mbps)
    sniffing_state = false;
    nrf24_deinit();

    if(furi_hal_power_is_otg_enabled() && !otg_was_enabled) {
        furi_hal_power_disable_otg();
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(plugin_state->mutex);
    free(plugin_state);

    return 0;
}
