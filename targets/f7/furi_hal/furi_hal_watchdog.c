#include <furi_hal_watchdog.h>
#include <stm32wbxx_ll_iwdg.h>
#include <stm32wbxx_ll_wwdg.h>
#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_bus.h>
#include <furi.h>

#define TAG "FuriHalWatchdog"

void furi_hal_watchdog_init(void) {
    // IWDG: Independent Watchdog - standard mode, ~4s timeout
    // LSI ~32kHz, prescaler 256 -> 125Hz, reload 500 -> 4s
    LL_RCC_LSI1_Enable();
    while(!LL_RCC_LSI1_IsReady());

    LL_IWDG_Enable(IWDG);
    LL_IWDG_EnableWriteAccess(IWDG);
    LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_256);
    LL_IWDG_SetReloadCounter(IWDG, 500);  // ~4s
    LL_IWDG_SetWindow(IWDG, 0x0FFF);
    while(LL_IWDG_IsReady(IWDG) == 0);
    LL_IWDG_ReloadCounter(IWDG);          // Initial refresh

    FURI_LOG_I(TAG, "IWDG initialized (4s timeout)");
}

void furi_hal_watchdog_iwdg_refresh(void) {
    LL_IWDG_ReloadCounter(IWDG);
}

void furi_hal_watchdog_wwdg_refresh(void) {
    // Kept as safe hook for tick monitoring; hardware WWDG disabled to prevent sleep/delay resets
}