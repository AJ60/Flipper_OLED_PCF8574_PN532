#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAM_SETTINGS_CALLSIGN_SIZE (16)
#define HAM_SETTINGS_PATH_SIZE     (16)
#define HAM_SETTINGS_LABEL_SIZE    (24)

/** Conservative defaults. TX stays disarmed until the operator supplies a
 * callsign and explicitly acknowledges the licensing conditions. */
#define HAM_SETTINGS_DEFAULT_POWER_LIMIT_DBM   (10)
#define HAM_SETTINGS_DEFAULT_DUTY_PERCENT       (10)
#define HAM_SETTINGS_DEFAULT_DUTY_WINDOW_MS     (60000)

typedef struct {
    char callsign[HAM_SETTINGS_CALLSIGN_SIZE];
    char path[HAM_SETTINGS_PATH_SIZE];
    char band_label[HAM_SETTINGS_LABEL_SIZE];
    bool tx_enabled;
    bool consent_accepted;
    uint8_t power_limit_dbm;
    uint8_t duty_cycle_percent;
    uint32_t duty_window_ms;
    uint32_t tx_time_ms;
    uint32_t last_tx_tick;
} HamSettings;

/** Fill instance with the safe defaults: no callsign, TX disarmed. */
void ham_settings_reset(HamSettings* instance);

/** Load settings from /ext/ham_radio/settings.
 * Missing or unreadable file leaves the instance at the safe defaults. */
bool ham_settings_load(HamSettings* instance);

/** Persist settings to /ext/ham_radio/settings. */
bool ham_settings_save(const HamSettings* instance);

/** True only when every precondition for amateur transmitting is met:
 * a non-empty callsign, operator consent recorded, and TX armed by the user.
 * The radio level region/duty-cycle checks live in the transmitter path. */
bool ham_settings_is_tx_armed(const HamSettings* instance);

/** Accumulates on-air time and returns false once the configured duty-cycle
 * percentage has been consumed inside the current window. */
bool ham_settings_duty_cycle_allows(const HamSettings* instance, uint32_t now_tick);

/** Fold a completed transmission of the given duration into the window. */
void ham_settings_tx_record(HamSettings* instance, uint32_t duration_ms);

/** Rolling on-air percentage over the active window, 0..100. */
uint8_t ham_settings_duty_cycle_percent_used(const HamSettings* instance);

#ifdef __cplusplus
}
#endif
