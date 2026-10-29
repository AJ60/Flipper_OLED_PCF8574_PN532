#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_pcf8574.h>
#include <flipper.h>
#include <alt_boot.h>
#include <assets_icons.h>
#include <toolbox/compress.h>
#include <gui/canvas.h>
#include <gui/canvas_i.h>

#define COUNTER_VALUE (136U)

static void flipper_boot_recovery_draw_progress(Canvas* canvas, size_t progress) {
    if(progress < COUNTER_VALUE) {
        // Fill the progress bar while the progress is going down
        canvas_draw_rframe(canvas, 59, 41, 69, 8, 2);
        size_t width = (COUNTER_VALUE - progress) * 68 / COUNTER_VALUE;
        canvas_draw_box(canvas, 60, 42, width, 6);
    } else {
        canvas_draw_rframe(canvas, 59, 41, 69, 8, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 60, 42, 67, 6);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_commit(canvas);
}

void flipper_boot_recovery_draw_splash(Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    canvas_draw_icon(canvas, 0, 0, &I_Erase_pin_128x64);

    canvas_commit(canvas);
}

void flipper_boot_recovery_exec(void) {
    Canvas* canvas = canvas_init();

    // Show recovery splashscreen
    flipper_boot_recovery_draw_splash(canvas);

    furi_hal_pcf8574_init();

    size_t counter = COUNTER_VALUE;
    uint32_t idle_timeout = 3000; // 30 seconds idle timeout (3000 * 10ms)
    while(counter && idle_timeout) {
        uint8_t port_val = 0xFF;
        bool read_ok = furi_hal_pcf8574_read_port(&port_val);

        if(read_ok) {
            // Down (P1) or Back (P5) cancels recovery mode
            if((port_val & (1 << 1)) == 0 || (port_val & (1 << 5)) == 0) {
                break;
            }

            // Right (P3) held down proceeds with recovery format
            if((port_val & (1 << 3)) == 0) {
                counter--;
            } else {
                counter = COUNTER_VALUE;
            }
        } else {
            // If expander fails to communicate, abort safely to avoid infinite stall
            break;
        }

        flipper_boot_recovery_draw_progress(canvas, counter);
        furi_delay_ms(10);
        idle_timeout--;
    }

    if(!counter) {
        furi_hal_rtc_reset_registers();
        furi_hal_rtc_set_flag(FuriHalRtcFlagStorageFormatInternal);
    }
}
