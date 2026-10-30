#include "jammer_signal.h"

#include <furi.h>
#include <furi_hal_random.h>

#define TAG "SubGhzJammerSignal"

static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

JammerSignal* jammer_signal_alloc(void) {
    JammerSignal* signal = calloc(1, sizeof(JammerSignal));
    if(signal == NULL) {
        FURI_LOG_E(TAG, "alloc failed");
        return NULL;
    }
    signal->mode = JammerSignalModeCarrier;
    signal->level = false;
    signal->carrier_period_us = 1000; /**< 1000us full period -> 500us half period (1 kHz modulation) */
    signal->noise_dur_min_us = 200;
    signal->noise_dur_max_us = 1000;
    signal->rng_state = furi_hal_random_get();
    if(signal->rng_state == 0) {
        signal->rng_state = 0x854321u;
    }
    return signal;
}

void jammer_signal_free(JammerSignal* signal) {
    furi_assert(signal);
    free(signal);
}

LevelDuration jammer_signal_yield(JammerSignal* signal) {
    furi_assert(signal);
    LevelDuration level_duration;

    switch(signal->mode) {
    case JammerSignalModeCarrier:
    case JammerSignalModeSweep:
    case JammerSignalModePingPong:
    case JammerSignalModeCount: {
        bool level = signal->level;
        signal->level = !level;
        uint32_t half_period = signal->carrier_period_us / 2;
        if(half_period < 2) {
            half_period = 2;
        }
        level_duration = level_duration_make(level, half_period);
        break;
    }
    case JammerSignalModeNoise: {
        uint32_t r = xorshift32(&signal->rng_state);
        bool level = (r & 1u) ? true : false;
        uint32_t dmin = signal->noise_dur_min_us;
        uint32_t dmax = signal->noise_dur_max_us;
        if(dmax <= dmin) {
            dmax = dmin + 1;
        }
        uint32_t dur = dmin + (r % (dmax - dmin));
        if(dur < 2) {
            dur = 2;
        }
        level_duration = level_duration_make(level, dur);
        break;
    }
    }

    return level_duration;
}

void jammer_signal_set_mode(JammerSignal* signal, JammerSignalMode mode) {
    furi_assert(signal);
    signal->mode = mode;
}

void jammer_signal_reset(JammerSignal* signal) {
    furi_assert(signal);
    signal->level = false;
}

void jammer_signal_set_frequency(JammerSignal* signal, uint32_t freq_hz) {
    furi_assert(signal);
    if(freq_hz > 0 && freq_hz <= 50000) {
        signal->carrier_period_us = 1000000 / freq_hz;
    } else {
        // Baseband modulation tone: 1 kHz square wave = 1000 us full period (500 us high / 500 us low)
        signal->carrier_period_us = 1000;
    }
}

uint32_t jammer_signal_hop_frequency(
    uint32_t band_min,
    uint32_t band_max,
    uint32_t hop_index,
    uint32_t hop_count) {
    if(hop_count < 2) return band_min;
    if(band_max <= band_min) return band_min;
    uint32_t span = band_max - band_min;
    uint32_t step = span / (hop_count - 1);
    if(hop_index >= hop_count) hop_index = hop_count - 1;
    return band_min + step * hop_index;
}
