#include <furi_hal_resources.h>
#include <furi_hal_bus.h>
#include <furi.h>

#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_pwr.h>

#define TAG "FuriHalResources"

// Legacy/debug GPIOs removed: keep main pin definitions only.

//const GpioPin gpio_button_IRQ = {.port = GPIOA, .pin = LL_GPIO_PIN_9};
const GpioPin gpio_swdio = {.port = GPIOA, .pin = LL_GPIO_PIN_13};
const GpioPin gpio_swclk = {.port = GPIOA, .pin = LL_GPIO_PIN_14};

// vibro not used on this board variant
const GpioPin gpio_ibutton = {.port = iBTN_GPIO_Port, .pin = iBTN_Pin};

const GpioPin gpio_subghz_cs = {.port = CC1101_CS_GPIO_Port, .pin = CC1101_CS_Pin};
// WARNING: gpio_cc1101_g0 and gpio_rfid_data_in are BOTH wired to PA1.
// Sub-GHz CC1101 GDO0 and the LF-RFID 125kHz data input share one pin; the
// LF-RFID HAL reconfigures PA1 at runtime, so the two subsystems must not be
// used simultaneously.
const GpioPin gpio_cc1101_g0 = {.port = CC1101_G0_GPIO_Port, .pin = CC1101_G0_Pin};

const GpioPin gpio_pcf_int = {.port = PCF_INT_GPIO_Port, .pin = PCF_INT_Pin};
const GpioPin gpio_ina_alert = {.port = INA_ALERT_GPIO_Port, .pin = INA_ALERT_Pin};
// RF switch pin omitted for this board

// const GpioPin gpio_display_cs = {.port = DISPLAY_CS_GPIO_Port, .pin = DISPLAY_CS_Pin};
// const GpioPin gpio_display_rst_n = {.port = DISPLAY_RST_GPIO_Port, .pin = DISPLAY_RST_Pin};
// const GpioPin gpio_display_di = {.port = DISPLAY_DI_GPIO_Port, .pin = DISPLAY_DI_Pin};
const GpioPin gpio_sdcard_cs = {.port = SD_CS_GPIO_Port, .pin = SD_CS_Pin};
// SD card CD not used
const GpioPin gpio_nfc_cs = {.port = NFC_CS_GPIO_Port, .pin = NFC_CS_Pin};
const GpioPin gpio_nfc_miso = {.port = GPIOB, .pin = LL_GPIO_PIN_4};
const GpioPin gpio_nfc_irq = {.port = NFC_IRQ_GPIO_Port, .pin = NFC_IRQ_Pin};
// WARNING: gpio_nfc_irq_rfid_pull is aliased to the same physical pin as gpio_nfc_irq (PA2).
// The RFID HAL drives it as OutputPushPull during LFRFID — this CONFLICTS with NFC IRQ mode.
// NFC and RFID cannot be used simultaneously without additional mutual exclusion on PA2.
const GpioPin gpio_nfc_irq_rfid_pull = {.port = NFC_IRQ_GPIO_Port, .pin = NFC_IRQ_Pin};
const GpioPin gpio_rfid_carrier_out = {.port = GPIOA, .pin = LL_GPIO_PIN_5};
// WARNING: gpio_rfid_data_in (PA1) is aliased to gpio_cc1101_g0 (CC1101 GDO0).
const GpioPin gpio_rfid_data_in = {.port = GPIOA, .pin = LL_GPIO_PIN_1};
// WARNING: gpio_rfid_carrier aliases gpio_rfid_carrier_out (both = PA5).
// The RFID HAL switches PA5 between OutputPushPull and AltFn TIM2 using these two handles.
const GpioPin gpio_rfid_carrier = {.port = GPIOA, .pin = LL_GPIO_PIN_5};

// MCU button GpioPin definitions removed — board uses PCF8574 for inputs.

const GpioPin gpio_spi_miso = {.port = SPI_MISO_GPIO_Port, .pin = SPI_MISO_Pin};
const GpioPin gpio_spi_mosi = {.port = SPI_MOSI_GPIO_Port, .pin = SPI_MOSI_Pin};
const GpioPin gpio_spi_r_mosi = {.port = SPI_MOSI_GPIO_Port, .pin = SPI_MOSI_Pin};
//const GpioPin gpio_spi_mosi1 = {.port = SPI_MOSI_GPIO_Port1, .pin = SPI_MOSI_Pin1};
const GpioPin gpio_spi_sck = {.port = SPI_SCK_GPIO_Port, .pin = SPI_SCK_Pin};

const GpioPin gpio_ext_pc0 = {.port = PC0_GPIO_Port, .pin = PC0_Pin};
const GpioPin gpio_ext_pc1 = {.port = PC1_GPIO_Port, .pin = PC1_Pin};
const GpioPin gpio_ext_pc3 = {.port = PC3_GPIO_Port, .pin = PC3_Pin};
const GpioPin gpio_ext_pb2 = {.port = PB2_GPIO_Port, .pin = PB2_Pin};
const GpioPin gpio_ext_pb3 = {.port = PB3_GPIO_Port, .pin = PB3_Pin};
const GpioPin gpio_ext_pa4 = {.port = PA4_GPIO_Port, .pin = PA4_Pin};
const GpioPin gpio_ext_pa6 = {.port = PA6_GPIO_Port, .pin = PA6_Pin};
// WARNING: On this DIY board the header pin labelled "PA7" is physically wired
// to MCU PB5 = SPI1 MOSI (shared with SD/NFC/Sub-GHz). The logical name
// "PA7"/gpio_ext_pa7 is kept for app compatibility, but PB5 has no ADC input
// and no timer output, and it must NOT be driven as a plain GPIO while SPI1 is
// in use. The physical PA7 MCU pin is exposed on the header as "C0"
// (gpio_ext_pc0): it is I2C3 SCL and also carries the PWM channel (TIM17_CH1).
const GpioPin gpio_ext_pa7 = {.port = PA7_GPIO_Port, .pin = PA7_Pin};

const GpioPin gpio_button_up;
const GpioPin gpio_button_down;
const GpioPin gpio_button_right;
const GpioPin gpio_button_left;
const GpioPin gpio_button_ok;
const GpioPin gpio_button_back;

const GpioPin gpio_infrared_rx = {.port = IR_RX_GPIO_Port, .pin = IR_RX_Pin};
const GpioPin gpio_infrared_tx = {.port = IR_TX_GPIO_Port, .pin = IR_TX_Pin};

const GpioPin gpio_usart_tx = {.port = USART1_TX_Port, .pin = USART1_TX_Pin};
const GpioPin gpio_usart_rx = {.port = USART1_RX_Port, .pin = USART1_RX_Pin};

const GpioPin gpio_i2c_1_sda = {.port = I2C_1_SDA_GPIO_Port, .pin = I2C_1_SDA_Pin};
const GpioPin gpio_i2c_1_scl = {.port = I2C_1_SCL_GPIO_Port, .pin = I2C_1_SCL_Pin};

const GpioPin gpio_i2c_3_sda = {.port = I2C_3_SDA_GPIO_Port, .pin = I2C_3_SDA_Pin};
const GpioPin gpio_i2c_3_scl = {.port = I2C_3_SCL_GPIO_Port, .pin = I2C_3_SCL_Pin};

const GpioPin gpio_speaker = {.port = SPEAKER_GPIO_Port, .pin = SPEAKER_Pin};

// peripheral power control not present on this board

const GpioPin gpio_usb_dm = {.port = GPIOA, .pin = LL_GPIO_PIN_11};
const GpioPin gpio_usb_dp = {.port = GPIOA, .pin = LL_GPIO_PIN_12};

// const GpioPin gpio_adc_battery_voltage = {.port = ADC_BATTERY_VOLTAGE_GPIO_Port, .pin = ADC_BATTERY_VOLTAGE_Pin};

const GpioPinRecord gpio_pins[] = {
    // 5V: 1
    // Header "PA7" is wired to MCU PB5 = SPI1 MOSI (shared with SD/NFC/Sub-GHz).
    // PB5 has no ADC input and no timer output -> hidden from the GPIO app and
    // JS; the SPI HAL owns this pin. Logical "PA7" name kept for compatibility.
    {.pin = &gpio_ext_pa7,
     .name = "PA7",
     .channel = FuriHalAdcChannelNone,
     .number = 2,
     .debug = true},
    // Header "PA6" is wired to MCU PA6 = SPI1 MISO (shared with SD/NFC/Sub-GHz).
    // MCU PA6 = ADC_IN11 (STM32WB55 map: PA_n = IN_(n+5), NOT the L4 layout).
    // The pin belongs to the SPI bus -> hidden from the GPIO app.
    {.pin = &gpio_ext_pa6,
     .name = "PA6",
     .channel = FuriHalAdcChannel11,
     .number = 3,
     .debug = true},
    // Header "PA4" is wired to MCU PA4 = ADC_IN9 + LPTIM2_OUT (AF14). Free pin.
    // (Stock Flipper uses Ch9 for PA4 on the same MCU.)
    {.pin = &gpio_ext_pa4,
     .name = "PA4",
     .channel = FuriHalAdcChannel9,
     .pwm_output = FuriHalPwmOutputIdLptim2PA4,
     .number = 4,
     .debug = false},
    // Header "PB3" is wired to MCU PB3 = SPI1 SCK (shared with SD/NFC/Sub-GHz)
    // and doubles as TIM2_CH2 (Sub-GHz input capture) -> not GPIO safe.
    {.pin = &gpio_ext_pb3,
     .name = "PB3",
     .channel = FuriHalAdcChannelNone,
     .number = 5,
     .debug = true},
    // Header "PB2" is wired to MCU PB2 = ADC_IN15. No timer output. Free pin.
    {.pin = &gpio_ext_pb2,
     .name = "PB2",
     .channel = FuriHalAdcChannel15,
     .number = 6,
     .debug = false},
    // Header "PC3" is wired to MCU PA5 = LF-RFID 125kHz carrier (TIM2_CH1);
    // MCU PA5 = ADC_IN10, but it belongs to the RFID HAL -> hidden from the
    // GPIO app.
    {.pin = &gpio_ext_pc3,
     .name = "PC3",
     .channel = FuriHalAdcChannel10,
     .number = 7,
     .debug = true},
    // GND: 8
    // Space
    // 3v3: 9
    {.pin = &gpio_swclk,
     .name = "PA14",
     .channel = FuriHalAdcChannelNone,
     .number = 10,
     .debug = true},
    // GND: 11
    {.pin = &gpio_swdio,
     .name = "PA13",
     .channel = FuriHalAdcChannelNone,
     .number = 12,
     .debug = true},
    {.pin = &gpio_usart_tx,
     .name = "PB6",
     .channel = FuriHalAdcChannelNone,
     .number = 13,
     .debug = true},
    {.pin = &gpio_usart_rx,
     .name = "PB7",
     .channel = FuriHalAdcChannelNone,
     .number = 14,
     .debug = true},
    // Header "PC1" is wired to MCU PB4 = ST25R3916 NFC MISO (also I2C3 SDA).
    // Not GPIO-safe while NFC/SPI is in use -> hidden from the GPIO app.
    {.pin = &gpio_ext_pc1,
     .name = "PC1",
     .channel = FuriHalAdcChannelNone,
     .number = 15,
     .debug = true},
    // Header "PC0" is wired to MCU PA7. I2C3 SCL (used by the I2C scanner / JS
    // i2c / CLI) and PWM-capable via TIM17_CH1 (AF14); MCU PA7 = ADC_IN12
    // (stock Flipper uses Ch12 for PA7 on the same MCU). One function at a
    // time: PWM while I2C3 is in use will corrupt the bus.
    {.pin = &gpio_ext_pc0,
     .name = "PC0",
     .channel = FuriHalAdcChannel12,
     .pwm_output = FuriHalPwmOutputIdTim1PA7,
     .number = 16,
     .debug = false},
    {.pin = &gpio_ibutton,
     .name = "PA3",
     .channel = FuriHalAdcChannelNone,
     .number = 17,
     .debug = true},
    // GND: 18

    /* Dangerous pins, may damage hardware */
    {.pin = &gpio_speaker,
     .name = "PB8",
     .channel = FuriHalAdcChannelNone,
     .number = 0,
     .debug = true},
    {.pin = &gpio_infrared_tx,
     .name = "PA8",
     .channel = FuriHalAdcChannelNone,
     .number = 0,
     .debug = true},
};

const size_t gpio_pins_count = COUNT_OF(gpio_pins);

// Old MCU-driven input pin array removed — input is handled via PCF8574 on this board.

const InputPin input_pins[] = {
    {.gpio = NULL, .key = InputKeyUp, .inverted = true, .name = "Up"},
    {.gpio = NULL, .key = InputKeyDown, .inverted = true, .name = "Down"},
    {.gpio = NULL, .key = InputKeyRight, .inverted = true, .name = "Right"},
    {.gpio = NULL, .key = InputKeyLeft, .inverted = true, .name = "Left"},
    {.gpio = NULL, .key = InputKeyOk, .inverted = true, .name = "OK"},
    {.gpio = NULL, .key = InputKeyBack, .inverted = true, .name = "Back"},
};

const size_t input_pins_count = COUNT_OF(input_pins);

// static void furi_hal_resources_init_input_pins(GpioMode mode) {
//     for(size_t i = 0; i < input_pins_count; i++) {
//         if(input_pins[i].gpio != NULL) {
//             furi_hal_gpio_init(
//                 input_pins[i].gpio,
//                 mode,
//                 (input_pins[i].inverted) ? GpioPullUp : GpioPullDown,
//                 GpioSpeedLow);
//         }
//     }
// }

static void furi_hal_resources_init_gpio_pins(GpioMode mode) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        if(!gpio_pins[i].debug) {
            furi_hal_gpio_init(gpio_pins[i].pin, mode, GpioPullNo, GpioSpeedLow);
        }
    }
}

void furi_hal_resources_init_early(void) {
    furi_hal_bus_enable(FuriHalBusGPIOA);
    furi_hal_bus_enable(FuriHalBusGPIOB);
    furi_hal_bus_enable(FuriHalBusGPIOC);
    furi_hal_bus_enable(FuriHalBusGPIOD);
    furi_hal_bus_enable(FuriHalBusGPIOE);
    furi_hal_bus_enable(FuriHalBusGPIOH);

    // furi_hal_resources_init_input_pins(GpioModeInput);

    // Explicit, surviving reset, pulls
    LL_PWR_EnablePUPDCfg();
    // LL_PWR_EnableGPIOPullDown(LL_PWR_GPIO_A, LL_PWR_GPIO_BIT_8); // gpio_vibro
    // LL_PWR_EnableGPIOPullDown(LL_PWR_GPIO_A, LL_PWR_GPIO_BIT_6); // gpio_speaker
    // LL_PWR_EnableGPIOPullDown(LL_PWR_GPIO_B, LL_PWR_GPIO_BIT_9); // gpio_infrared_tx

    // SD Card stepdown control
    // furi_hal_gpio_write(&gpio_periph_power, 1);
    // furi_hal_gpio_init(&gpio_periph_power, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);

    // Display pins
    // furi_hal_gpio_write(&gpio_display_rst_n, 0);
    // furi_hal_gpio_init_simple(&gpio_display_rst_n, GpioModeOutputPushPull);
    // LL_PWR_EnableGPIOPullUp(LL_PWR_GPIO_B, LL_PWR_GPIO_BIT_0); // gpio_display_rst_n
    // furi_hal_gpio_write(&gpio_display_di, 0);
    // furi_hal_gpio_init_simple(&gpio_display_di, GpioModeOutputPushPull);
    // LL_PWR_EnableGPIOPullDown(LL_PWR_GPIO_B, LL_PWR_GPIO_BIT_1); // gpio_display_di

    // Hard reset USB
    furi_hal_gpio_write(&gpio_usb_dm, 1);
    furi_hal_gpio_write(&gpio_usb_dp, 1);
    furi_hal_gpio_init_simple(&gpio_usb_dm, GpioModeOutputOpenDrain);
    furi_hal_gpio_init_simple(&gpio_usb_dp, GpioModeOutputOpenDrain);
    furi_hal_gpio_write(&gpio_usb_dm, 0);
    furi_hal_gpio_write(&gpio_usb_dp, 0);
    furi_delay_us(5); // Device Driven disconnect: 2.5us + extra to compensate cables
    furi_hal_gpio_write(&gpio_usb_dm, 1);
    furi_hal_gpio_write(&gpio_usb_dp, 1);
    furi_hal_gpio_init_simple(&gpio_usb_dm, GpioModeAnalog);
    furi_hal_gpio_init_simple(&gpio_usb_dp, GpioModeAnalog);
    furi_hal_gpio_write(&gpio_usb_dm, 0);
    furi_hal_gpio_write(&gpio_usb_dp, 0);

    // External header pins
    furi_hal_resources_init_gpio_pins(GpioModeAnalog);
}

void furi_hal_resources_deinit_early(void) {
    furi_hal_bus_disable(FuriHalBusGPIOA);
    furi_hal_bus_disable(FuriHalBusGPIOB);
    furi_hal_bus_disable(FuriHalBusGPIOC);
    furi_hal_bus_disable(FuriHalBusGPIOD);
    furi_hal_bus_disable(FuriHalBusGPIOE);
    furi_hal_bus_disable(FuriHalBusGPIOH);
}

void furi_hal_resources_init(void) {
    furi_hal_gpio_init(&gpio_ibutton, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    //furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeInterruptRiseFall, GpioPullUp, GpioSpeedLow);
    // FURI_LOG_T(TAG, "IRQ4");

    //  furi_hal_gpio_init(&gpio_rf_sw_0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);

    // EXTI0: PB0 = gpio_pcf_int (PCF8574 INT). NVIC is enabled by
    //         furi_hal_pcf8574_attach_int() when the input service starts.
    //         NOTE: IR RX (PA0) does NOT use EXTI0 — the IR HAL uses TIM2
    //         input capture (GpioAltFn1TIM2), so the EXTI0 line is free.
    // EXTI1: PB1 = gpio_ina_alert (INA226 ALERT). NVIC is enabled by
    //         furi_hal_ina226_enable_alert_interrupt() in power init.
    //         No other device uses EXTI line 1: RFID data-in (PA1) is routed
    //         through the analog comparator (EXTI lines 20/21), not EXTI1.

    // EXTI2: PA2 = gpio_nfc_irq. Enabled — NFC HAL expects this.
    NVIC_SetPriority(EXTI2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI2_IRQn);

    // EXTI3: PA3 = gpio_ibutton, initialized as GpioModeAnalog above.
    //         Do NOT enable EXTI3 — analog input cannot trigger EXTI and
    //         an uninitialized interrupt handler would fire spuriously.

    // EXTI4: PB4 = gpio_nfc_miso (SPI). Not an interrupt source.
    //         PA4 = gpio_ext_pa4. Not configured as interrupt.
    //         Do NOT enable EXTI4 here.

    // EXTI9_5: Used by NFC CS and other SPI signals. NFC HAL and SPI configure as needed.
    NVIC_SetPriority(EXTI9_5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI9_5_IRQn);

    // EXTI15_10: covers header/other pins 10-15 (none used as GPIO interrupts
    // on this board). It does NOT cover PB0 — that is EXTI0, enabled by the
    // PCF8574 driver. Kept enabled for the GPIO app / JS interrupts.
    NVIC_SetPriority(EXTI15_10_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI15_10_IRQn);

    FURI_LOG_I(TAG, "Init OK");
}

int32_t furi_hal_resources_get_ext_pin_number(const GpioPin* gpio) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        if(gpio_pins[i].pin == gpio) {
            return gpio_pins[i].number;
        }
    }
    return -1;
}

const GpioPinRecord* furi_hal_resources_pin_by_name(const char* name) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        const GpioPinRecord* record = &gpio_pins[i];
        if(strcasecmp(name, record->name) == 0) return record;
    }
    return NULL;
}

const GpioPinRecord* furi_hal_resources_pin_by_number(uint8_t number) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        const GpioPinRecord* record = &gpio_pins[i];
        if(record->number == number) return record;
    }
    return NULL;
}
