#include <furi_hal_vibro.h>
#include <furi_hal_gpio.h>
#include <core/common_defines.h>

#define TAG "FuriHalVibro"

void furi_hal_vibro_init(void) {
    // VIBRO not present on this board revision (see furi_hal_resources.h:224).
    FURI_LOG_I(TAG, "Vibro not present on this board revision");
}

void furi_hal_vibro_on(bool value) {
    UNUSED(value);
    // VIBRO not present on this board revision.
}
