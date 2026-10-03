#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_watchdog_init(void);
void furi_hal_watchdog_iwdg_refresh(void);
void furi_hal_watchdog_wwdg_refresh(void);

#ifdef __cplusplus
}
#endif