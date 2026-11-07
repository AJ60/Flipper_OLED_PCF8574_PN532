#include "jammer_freq_edit.h"
#include <stdio.h>
#include <stddef.h>

uint16_t freq_hz_to_index(uint32_t freq_hz) {
    if(freq_hz < FREQ_MIN_HZ) {
        return 0;
    }
    if(freq_hz > FREQ_MAX_HZ) {
        return FREQ_STEP_COUNT - 1;
    }
    return (uint16_t)((freq_hz - FREQ_MIN_HZ) / FREQ_STEP_HZ);
}

uint32_t freq_index_to_hz(uint16_t index) {
    if(index >= FREQ_STEP_COUNT) {
        return FREQ_MAX_HZ;
    }
    return FREQ_MIN_HZ + (uint32_t)index * FREQ_STEP_HZ;
}

const char* freq_hz_to_text(uint32_t freq_hz, char* buf, size_t buf_size) {
    float mhz = (float)freq_hz / 1000000.0f;
    snprintf(buf, buf_size, "%.2f MHz", (double)mhz);
    return buf;
}

int32_t freq_edit_step_index(int32_t index, int32_t delta) {
    if(index < 0 || index >= (int32_t)FREQ_STEP_COUNT || (delta != -1 && delta != 1)) {
        return -1;
    }
    int64_t next = (int64_t)index + delta;
    if(next < 0) {
        next = 0;
    }
    if(next >= (int64_t)FREQ_STEP_COUNT) {
        next = FREQ_STEP_COUNT - 1;
    }
    return (int32_t)next;
}
