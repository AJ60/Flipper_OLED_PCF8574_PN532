#pragma once

#include "u8g2.h"
#include <stdbool.h>

uint8_t u8g2_gpio_and_delay_stm32(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

uint8_t u8x8_hw_i2c_stm32(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

uint8_t u8x8_byte_hw_i2c_stm32(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

void u8g2_Setup_ssd1306_flipper(
    u8g2_t* u8g2,
    const u8g2_cb_t* rotation,
    u8x8_msg_cb byte_cb,
    u8x8_msg_cb gpio_and_delay_cb);

void u8x8_d_ssd1306_init(u8x8_t* u8x8, uint8_t contrast, uint8_t regulation_ratio, bool bias);

void u8x8_d_ssd1306_set_contrast(u8x8_t* u8x8, int32_t contrast_val);
void u8x8_d_ssd1306_set_invert(u8x8_t* u8x8, bool invert);

extern volatile bool display_needs_reinit;
extern volatile bool display_is_sleeping;

