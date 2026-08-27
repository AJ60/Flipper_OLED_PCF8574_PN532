#include <furi_hal_pwm.h>
#include <furi_hal_resources.h>
#include <furi_hal_bus.h>

#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_lptim.h>
#include <stm32wbxx_ll_rcc.h>

#include <furi.h>

const uint32_t lptim_psc_table[] = {
    LL_LPTIM_PRESCALER_DIV1,
    LL_LPTIM_PRESCALER_DIV2,
    LL_LPTIM_PRESCALER_DIV4,
    LL_LPTIM_PRESCALER_DIV8,
    LL_LPTIM_PRESCALER_DIV16,
    LL_LPTIM_PRESCALER_DIV32,
    LL_LPTIM_PRESCALER_DIV64,
    LL_LPTIM_PRESCALER_DIV128,
};

// Ownership flag for the TIM17-backed PWM channel. TIM17 is shared with the
// NFC HAL (block-tx timer), so the bus clock state alone cannot distinguish
// "PWM running" from "NFC active" — track PWM ownership explicitly.
static bool pwm_tim17_active = false;

void furi_hal_pwm_start(FuriHalPwmOutputId channel, uint32_t freq, uint8_t duty) {
    if(channel == FuriHalPwmOutputIdTim1PA7) {
        // Board remap: the header pin labelled "PA7" is physically PB5 (SPI1
        // MOSI) and has no timer output, so this channel drives the physical
        // PA7 pin instead (header "C0", gpio_ext_pc0) via TIM17_CH1 (AF14).
        // The enum name is kept for API compatibility (signal generator, JS,
        // MicroPython).
        //
        // CAUTION: PA7 is also I2C3 SCL (I2C scanner / JS i2c / CLI) and TIM17
        // doubles as the NFC HAL block-tx timer during active NFC sessions.
        // One function at a time: do not start PWM while I2C3 or NFC is in use.
        // Starting PWM while NFC holds TIM17 trips the furi_hal_bus_enable()
        // furi_check and crashes on purpose.
        if(pwm_tim17_active) {
            // Already running: update the parameters in place.
            furi_hal_pwm_set_params(channel, freq, duty);
            return;
        }

        furi_hal_bus_enable(FuriHalBusTIM17);

        furi_hal_gpio_init_ex(
            &gpio_ext_pc0,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn14TIM17);

        LL_TIM_SetCounterMode(TIM17, LL_TIM_COUNTERMODE_UP);
        LL_TIM_SetClockSource(TIM17, LL_TIM_CLOCKSOURCE_INTERNAL);
        LL_TIM_SetClockDivision(TIM17, LL_TIM_CLOCKDIVISION_DIV1);
        LL_TIM_EnableARRPreload(TIM17);

        LL_TIM_OC_EnablePreload(TIM17, LL_TIM_CHANNEL_CH1);
        LL_TIM_OC_SetMode(TIM17, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
        LL_TIM_OC_SetPolarity(TIM17, LL_TIM_CHANNEL_CH1, LL_TIM_OCPOLARITY_HIGH);
        LL_TIM_OC_DisableFast(TIM17, LL_TIM_CHANNEL_CH1);
        LL_TIM_CC_EnableChannel(TIM17, LL_TIM_CHANNEL_CH1);

        LL_TIM_EnableAllOutputs(TIM17);

        furi_hal_pwm_set_params(channel, freq, duty);

        LL_TIM_EnableCounter(TIM17);
        pwm_tim17_active = true;
    } else if(channel == FuriHalPwmOutputIdLptim2PA4) {
        furi_hal_gpio_init_ex(
            &gpio_ext_pa4,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn14LPTIM2);

        furi_hal_bus_enable(FuriHalBusLPTIM2);

        LL_LPTIM_SetUpdateMode(LPTIM2, LL_LPTIM_UPDATE_MODE_ENDOFPERIOD);
        LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM2_CLKSOURCE_PCLK1);
        LL_LPTIM_SetClockSource(LPTIM2, LL_LPTIM_CLK_SOURCE_INTERNAL);
        LL_LPTIM_ConfigOutput(
            LPTIM2, LL_LPTIM_OUTPUT_WAVEFORM_PWM, LL_LPTIM_OUTPUT_POLARITY_INVERSE);
        LL_LPTIM_SetCounterMode(LPTIM2, LL_LPTIM_COUNTER_MODE_INTERNAL);

        LL_LPTIM_Enable(LPTIM2);

        furi_hal_pwm_set_params(channel, freq, duty);

        LL_LPTIM_StartCounter(LPTIM2, LL_LPTIM_OPERATING_MODE_CONTINUOUS);
    } else {
        furi_crash();
    }
}

void furi_hal_pwm_stop(FuriHalPwmOutputId channel) {
    if(channel == FuriHalPwmOutputIdTim1PA7) {
        // Only tear down if PWM actually owns TIM17 — it may belong to the NFC
        // HAL (block-tx timer) or PA7 may be in use as I2C3 SCL; do not touch
        // either if PWM never started.
        if(!pwm_tim17_active) return;
        pwm_tim17_active = false;
        furi_hal_gpio_init_simple(&gpio_ext_pc0, GpioModeAnalog);
        if(furi_hal_bus_is_enabled(FuriHalBusTIM17)) {
            furi_hal_bus_disable(FuriHalBusTIM17);
        }
    } else if(channel == FuriHalPwmOutputIdLptim2PA4) {
        furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeAnalog);
        furi_hal_bus_disable(FuriHalBusLPTIM2);
    } else {
        furi_crash();
    }
}

bool furi_hal_pwm_is_running(FuriHalPwmOutputId channel) {
    if(channel == FuriHalPwmOutputIdTim1PA7) {
        return pwm_tim17_active;
    } else if(channel == FuriHalPwmOutputIdLptim2PA4) {
        return furi_hal_bus_is_enabled(FuriHalBusLPTIM2);
    }

    furi_crash();
}

void furi_hal_pwm_set_params(FuriHalPwmOutputId channel, uint32_t freq, uint8_t duty) {
    furi_assert(freq > 0);
    uint32_t freq_div = 64000000LU / freq;

    if(channel == FuriHalPwmOutputIdTim1PA7) {
        uint32_t prescaler = freq_div / 0x10000LU;
        uint32_t period = freq_div / (prescaler + 1);
        uint32_t compare = period * duty / 100;

        LL_TIM_SetPrescaler(TIM17, prescaler);
        LL_TIM_SetAutoReload(TIM17, period - 1);
        LL_TIM_OC_SetCompareCH1(TIM17, compare);
    } else if(channel == FuriHalPwmOutputIdLptim2PA4) {
        uint32_t prescaler = 0;
        uint32_t period = 0;

        bool clock_lse = false;

        do {
            period = freq_div / (1UL << prescaler);
            if(period <= 0xFFFF) {
                break;
            }
            prescaler++;
            if(prescaler > 7) {
                prescaler = 0;
                clock_lse = true;
                period = 32768LU / freq;
                break;
            }
        } while(1);

        uint32_t compare = period * duty / 100;

        LL_LPTIM_SetPrescaler(LPTIM2, lptim_psc_table[prescaler]);
        LL_LPTIM_SetAutoReload(LPTIM2, period);
        LL_LPTIM_SetCompare(LPTIM2, compare);

        if(clock_lse) {
            LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM2_CLKSOURCE_LSE);
        } else {
            LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM2_CLKSOURCE_PCLK1);
        }
    } else {
        furi_crash();
    }
}
