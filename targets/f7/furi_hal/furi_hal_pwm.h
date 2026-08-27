/**
 * @file furi_hal_pwm.h
 * PWM contol HAL
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FuriHalPwmOutputIdNone,
    /** TIM17_CH1 on the physical PA7 pin (header "C0", gpio_ext_pc0).
     *
     * On this DIY board the header pin labelled "PA7" is physically wired to
     * PB5 (SPI1 MOSI) and has no timer output, so the legacy name
     * FuriHalPwmOutputIdTim1PA7 is kept for API compatibility but now maps to
     * TIM17_CH1 on PA7. CAUTION: PA7 doubles as I2C3 SCL and TIM17 doubles as
     * the NFC HAL block-tx timer during active NFC sessions — PWM, I2C3 and
     * NFC must not be used simultaneously.
     */
    FuriHalPwmOutputIdTim1PA7,
    FuriHalPwmOutputIdLptim2PA4,
} FuriHalPwmOutputId;

/** Enable PWM channel and set parameters
 * 
 * @param[in]  channel  PWM channel (FuriHalPwmOutputId)
 * @param[in]  freq  Frequency in Hz
 * @param[in]  duty  Duty cycle value in %
*/
void furi_hal_pwm_start(FuriHalPwmOutputId channel, uint32_t freq, uint8_t duty);

/** Disable PWM channel
 * 
 * @param[in]  channel  PWM channel (FuriHalPwmOutputId)
*/
void furi_hal_pwm_stop(FuriHalPwmOutputId channel);

/** Set PWM channel parameters
 * 
 * @param[in]  channel  PWM channel (FuriHalPwmOutputId)
 * @param[in]  freq  Frequency in Hz
 * @param[in]  duty  Duty cycle value in %
*/
void furi_hal_pwm_set_params(FuriHalPwmOutputId channel, uint32_t freq, uint8_t duty);

/** Is PWM channel running?
 * 
 * @param[in]  channel  PWM channel (FuriHalPwmOutputId)
 * @return bool - true if running
*/
bool furi_hal_pwm_is_running(FuriHalPwmOutputId channel);

#ifdef __cplusplus
}
#endif
