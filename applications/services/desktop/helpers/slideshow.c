#include "slideshow.h"

#include <furi.h>
#include <storage/storage.h>
#include <gui/icon.h>
#include <core/dangerous_defines.h>

#define TAG                             "Slideshow"
#define SLIDESHOW_MAGIC                 0x72676468
#define SLIDESHOW_MAX_SUPPORTED_VERSION 1

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t width;
    uint8_t height;
    uint8_t frame_count;
} SlideshowFileHeader;
_Static_assert(sizeof(SlideshowFileHeader) == 8, "Incorrect SlideshowFileHeader size");

typedef struct {
    uint16_t size;
} SlideshowFrameHeader;
_Static_assert(sizeof(SlideshowFrameHeader) == 2, "Incorrect SlideshowFrameHeader size");

#pragma pack(pop)

Slideshow* slideshow_alloc(void) {
    return calloc(1, sizeof(Slideshow));
}

void slideshow_free(Slideshow* slideshow) {
    Icon* icon = &slideshow->icon;
    if(icon && icon->frames) {
        for(int frame_idx = 0; frame_idx < icon->frame_count; ++frame_idx) {
            uint8_t* frame_data = (uint8_t*)icon->frames[frame_idx];
            if(frame_data) {
                free(frame_data);
            }
        }
        free((uint8_t**)icon->frames);
    }
    free(slideshow);
}

bool slideshow_load(Slideshow* slideshow, const char* fspath) {
    FURI_LOG_I(TAG, "Loading slideshow from %s", fspath);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* slideshow_file = storage_file_alloc(storage);
    slideshow->loaded = false;
    do {
        if(!storage_file_open(slideshow_file, fspath, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "Failed to open slideshow file %s", fspath);
            break;
        }
        SlideshowFileHeader header;
        if(storage_file_read(slideshow_file, &header, sizeof(header)) != sizeof(header)) {
            FURI_LOG_E(TAG, "Failed to read slideshow header");
            break;
        }
        if(header.magic != SLIDESHOW_MAGIC) {
            FURI_LOG_E(
                TAG,
                "Invalid magic number: 0x%08lX (expected 0x%08lX)",
                (uint32_t)header.magic,
                (uint32_t)SLIDESHOW_MAGIC);
            break;
        }
        if(header.version > SLIDESHOW_MAX_SUPPORTED_VERSION) {
            FURI_LOG_E(
                TAG,
                "Unsupported version: %d (max %d)",
                header.version,
                SLIDESHOW_MAX_SUPPORTED_VERSION);
            break;
        }
        FURI_LOG_I(
            TAG,
            "Slideshow header: w=%d, h=%d, frames=%d",
            header.width,
            header.height,
            header.frame_count);
        Icon* icon = &slideshow->icon;
        FURI_CONST_ASSIGN(icon->frame_count, header.frame_count);
        FURI_CONST_ASSIGN(icon->width, header.width);
        FURI_CONST_ASSIGN(icon->height, header.height);
        icon->frames = calloc(header.frame_count, sizeof(uint8_t*));
        for(int frame_idx = 0; frame_idx < header.frame_count; ++frame_idx) {
            SlideshowFrameHeader frame_header;
            if(storage_file_read(slideshow_file, &frame_header, sizeof(frame_header)) !=
               sizeof(frame_header)) {
                FURI_LOG_E(TAG, "Failed to read frame %d header", frame_idx);
                break;
            }
            FURI_CONST_ASSIGN_PTR(icon->frames[frame_idx], malloc(frame_header.size));
            uint8_t* frame_data = (uint8_t*)icon->frames[frame_idx];
            if(storage_file_read(slideshow_file, frame_data, frame_header.size) !=
               frame_header.size) {
                FURI_LOG_E(
                    TAG,
                    "Failed to read frame %d data (expected size %d)",
                    frame_idx,
                    frame_header.size);
                break;
            }
            slideshow->loaded = (frame_idx + 1) == header.frame_count;
        }
    } while(false);
    storage_file_free(slideshow_file);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "Slideshow load status: %s", slideshow->loaded ? "success" : "failed");
    return slideshow->loaded;
}

bool slideshow_is_loaded(Slideshow* slideshow) {
    return slideshow->loaded;
}

bool slideshow_is_one_page(Slideshow* slideshow) {
    return slideshow->loaded && (slideshow->icon.frame_count == 1);
}

bool slideshow_advance(Slideshow* slideshow) {
    uint8_t next_frame = slideshow->current_frame + 1;
    if(next_frame < slideshow->icon.frame_count) {
        slideshow->current_frame = next_frame;
        return true;
    }
    return false;
}

void slideshow_goback(Slideshow* slideshow) {
    if(slideshow->current_frame > 0) {
        slideshow->current_frame--;
    }
}

void slideshow_draw(Slideshow* slideshow, Canvas* canvas, uint8_t x, uint8_t y) {
    furi_assert(slideshow->current_frame < slideshow->icon.frame_count);
    canvas_draw_bitmap(
        canvas,
        x,
        y,
        slideshow->icon.width,
        slideshow->icon.height,
        slideshow->icon.frames[slideshow->current_frame]);
}
