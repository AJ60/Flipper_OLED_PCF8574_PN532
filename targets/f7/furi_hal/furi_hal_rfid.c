#include <furi_hal_rfid.h>
#include <furi_hal_ibutton.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_resources.h>
#include <furi_hal_bus.h>
#include <furi.h>

#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_comp.h>
#include <stm32wbxx_ll_dma.h>

#define FURI_HAL_RFID_READ_TIMER                TIM2
#define FURI_HAL_RFID_READ_TIMER_BUS            FuriHalBusTIM2
#define FURI_HAL_RFID_READ_TIMER_CHANNEL        LL_TIM_CHANNEL_CH1
#define FURI_HAL_RFID_READ_TIMER_CHANNEL_CONFIG LL_TIM_CHANNEL_CH1

#define FURI_HAL_RFID_EMULATE_TIMER         TIM2
#define FURI_HAL_RFID_EMULATE_TIMER_BUS     FuriHalBusTIM2
#define FURI_HAL_RFID_EMULATE_TIMER_IRQ     FuriHalInterruptIdTIM2
#define FURI_HAL_RFID_EMULATE_TIMER_CHANNEL LL_TIM_CHANNEL_CH3

#define RFID_CAPTURE_TIM     TIM1
#define RFID_CAPTURE_TIM_BUS FuriHalBusTIM1
#define RFID_CAPTURE_IND_CH  LL_TIM_CHANNEL_CH2
#define RFID_CAPTURE_DIR_CH  LL_TIM_CHANNEL_CH1

// Field presence detection
#define FURI_HAL_RFID_FIELD_FREQUENCY_MIN 80000
#define FURI_HAL_RFID_FIELD_FREQUENCY_MAX 200000

#define FURI_HAL_RFID_FIELD_COUNTER_TIMER         TIM2
#define FURI_HAL_RFID_FIELD_COUNTER_TIMER_BUS     FuriHalBusTIM2
#define FURI_HAL_RFID_FIELD_COUNTER_TIMER_CHANNEL LL_TIM_CHANNEL_CH3

#define FURI_HAL_RFID_FIELD_TIMEOUT_TIMER     TIM1
#define FURI_HAL_RFID_FIELD_TIMEOUT_TIMER_BUS FuriHalBusTIM1

#define FURI_HAL_RFID_FIELD_DMAMUX_DMA LL_DMAMUX_REQ_TIM1_UP

/* DMA Channels definition */
#define RFID_DMA             DMA2
#define RFID_DMA_CH1_CHANNEL LL_DMA_CHANNEL_1
#define RFID_DMA_CH2_CHANNEL LL_DMA_CHANNEL_2
#define RFID_DMA_CH1_IRQ     FuriHalInterruptIdDma2Ch1
#define RFID_DMA_CH1_DEF     RFID_DMA, RFID_DMA_CH1_CHANNEL
#define RFID_DMA_CH2_DEF     RFID_DMA, RFID_DMA_CH2_CHANNEL

typedef struct {
    uint32_t counter;
    uint32_t set_tim_counter_cnt;
} FuriHalRfidField;

typedef struct {
    FuriHalRfidDMACallback dma_callback;
    FuriHalRfidReadCaptureCallback read_capture_callback;
    void* context;
    FuriHalRfidField field;
    uint32_t capture_last_dwt;
    uint32_t capture_pulse_us;
    bool capture_have_last;
    bool capture_have_pulse;
} FuriHalRfid;

FuriHalRfid* furi_hal_rfid = NULL;

#define LFRFID_LL_READ_TIM            TIM1
#define LFRFID_LL_READ_CONFIG_CHANNEL LL_TIM_CHANNEL_CH1
#define LFRFID_LL_READ_CHANNEL        LL_TIM_CHANNEL_CH1N

#define LFRFID_LL_EMULATE_TIM     TIM2
#define LFRFID_LL_EMULATE_CHANNEL LL_TIM_CHANNEL_CH3

/* WeAct: PA0 (header A0 next to NR) = COMP1_OUT debug mirror. Same pin as IR RX. */
static const GpioPin gpio_rfid_comp_out = {.port = GPIOA, .pin = LL_GPIO_PIN_0};

static void furi_hal_rfid_comp_out_config(void) {
    furi_hal_gpio_init_ex(
        &gpio_rfid_comp_out,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedVeryHigh,
        GpioAltFn12COMP1);
}

void furi_hal_rfid_init(void) {
    furi_check(furi_hal_rfid == NULL);
    furi_hal_rfid = malloc(sizeof(FuriHalRfid));
    furi_check(furi_hal_rfid);
    furi_hal_rfid->field.counter = 0;
    furi_hal_rfid->field.set_tim_counter_cnt = 0;

    furi_hal_rfid_pins_reset();

    LL_COMP_InitTypeDef COMP_InitStruct = {0};
    COMP_InitStruct.PowerMode = LL_COMP_POWERMODE_MEDIUMSPEED;
    // STM32WB55 COMP1: IO1=PC5, IO2=PB2, IO3=PA1. DIY RX (LM2904) is on PA1.
    COMP_InitStruct.InputPlus = LL_COMP_INPUT_PLUS_IO3;
    // Signal mid-point ≈1.14V → VREFINT (≈1.2V) centres the threshold
    COMP_InitStruct.InputMinus = LL_COMP_INPUT_MINUS_VREFINT;
    // High hysteresis — LM2904 residual carrier causes COMP chatter (~8us)
    COMP_InitStruct.InputHysteresis = LL_COMP_HYSTERESIS_HIGH;
    // Must match stock Flipper: NONINVERTED + ISR level mapping below
    COMP_InitStruct.OutputPolarity = LL_COMP_OUTPUTPOL_NONINVERTED;
    COMP_InitStruct.OutputBlankingSource = LL_COMP_BLANKINGSRC_NONE;
    LL_COMP_Init(COMP1, &COMP_InitStruct);
    LL_COMP_SetCommonWindowMode(__LL_COMP_COMMON_INSTANCE(COMP1), LL_COMP_WINDOWMODE_DISABLE);

    furi_hal_rfid_comp_out_config();

    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_20);
    LL_EXTI_DisableEvent_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_20);

    NVIC_SetPriority(COMP_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 10, 0));
    NVIC_EnableIRQ(COMP_IRQn);
}

void furi_hal_rfid_pins_reset(void) {
    // ibutton bus disable
    furi_hal_ibutton_pin_reset();

    // pulldown rfid antenna
    furi_hal_gpio_init(&gpio_rfid_carrier_out, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_rfid_carrier_out, false);

    // Release PA2 (gpio_nfc_irq_rfid_pull) as analog mode when RFID is idle so
    // PN532 active-low IRQ is never short-circuited by a PushPull HIGH driver.
    furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    furi_hal_gpio_init_simple(&gpio_rfid_carrier, GpioModeAnalog);

    furi_hal_gpio_init(&gpio_rfid_data_in, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

static void furi_hal_rfid_pins_emulate(void) {
    // ibutton low
    furi_hal_ibutton_pin_configure();
    furi_hal_ibutton_pin_write(false);

    // pull pin to timer out (VeryHigh: sharper edges for EM4100/16 ~64 us half-bits)
    furi_hal_gpio_init_ex(
        &gpio_nfc_irq_rfid_pull,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedVeryHigh,
        GpioAltFn1TIM2);

    // Turn OFF active TX carrier on PA5 during Emulation
    furi_hal_gpio_init_simple(&gpio_rfid_carrier_out, GpioModeAnalog);
    furi_hal_gpio_init_simple(&gpio_rfid_carrier, GpioModeAnalog);
}

static void furi_hal_rfid_pins_read(void) {
    // ibutton low
    furi_hal_ibutton_pin_configure();
    furi_hal_ibutton_pin_write(false);

    // dont pull rfid antenna
    furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_nfc_irq_rfid_pull, false);

    // carrier pin to timer out (PA5 = TIM2_CH1 = AF1)
    furi_hal_gpio_init_ex(
        &gpio_rfid_carrier_out,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedVeryHigh,
        GpioAltFn1TIM2);

    // comparator in (PA1 must stay analog for COMP1 IO3)
    furi_hal_gpio_init(&gpio_rfid_data_in, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    // Re-claim PA0 for COMP1_OUT (IR RX may have taken the pin earlier)
    furi_hal_rfid_comp_out_config();
}

static void furi_hal_rfid_pins_field(void) {
    // ibutton low
    furi_hal_ibutton_pin_configure();
    furi_hal_ibutton_pin_write(false);

    // pull pin to timer out
    furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_nfc_irq_rfid_pull, false);

    // carrier pin to timer out (PA5 = TIM2_CH1 = AF1)
    furi_hal_gpio_init_ex(
        &gpio_rfid_carrier_out,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedVeryHigh,
        GpioAltFn1TIM2);

    furi_hal_gpio_init_ex(
        &gpio_rfid_carrier, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn1TIM2);
}

void furi_hal_rfid_pin_pull_release(void) {
    furi_hal_gpio_write(&gpio_nfc_irq_rfid_pull, true);
}

void furi_hal_rfid_pin_pull_pulldown(void) {
    furi_hal_gpio_write(&gpio_nfc_irq_rfid_pull, false);
}

void furi_hal_rfid_tim_read_start(float freq, float duty_cycle) {
    furi_hal_bus_enable(FURI_HAL_RFID_READ_TIMER_BUS);

    furi_hal_rfid_pins_read();

    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    TIM_InitStruct.Autoreload = (SystemCoreClock / freq) - 1;
    LL_TIM_Init(FURI_HAL_RFID_READ_TIMER, &TIM_InitStruct);
    LL_TIM_DisableARRPreload(FURI_HAL_RFID_READ_TIMER);

    LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
    TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_ENABLE;
    TIM_OC_InitStruct.CompareValue = TIM_InitStruct.Autoreload * duty_cycle;
    LL_TIM_OC_Init(
        FURI_HAL_RFID_READ_TIMER, FURI_HAL_RFID_READ_TIMER_CHANNEL_CONFIG, &TIM_OC_InitStruct);

    LL_TIM_EnableCounter(FURI_HAL_RFID_READ_TIMER);

    furi_hal_rfid_tim_read_continue();
}

void furi_hal_rfid_tim_read_continue(void) {
    LL_TIM_CC_EnableChannel(FURI_HAL_RFID_READ_TIMER, LL_TIM_CHANNEL_CH1);
    LL_TIM_EnableAllOutputs(FURI_HAL_RFID_READ_TIMER);
}

void furi_hal_rfid_tim_read_pause(void) {
    LL_TIM_CC_DisableChannel(FURI_HAL_RFID_READ_TIMER, LL_TIM_CHANNEL_CH1);
    LL_TIM_DisableAllOutputs(FURI_HAL_RFID_READ_TIMER);
}

void furi_hal_rfid_tim_read_stop(void) {
    furi_hal_bus_disable(FURI_HAL_RFID_READ_TIMER_BUS);
}

static void furi_hal_rfid_tim_emulate(void) {
    // Free-run 1 us tick (stock uses field ETR in RF cycles). Worker scales RF→µs (*8).
    // 1 us keeps EM4100/16 half-bits (~64 us) within ±1 tick; 8 us tick was too coarse.
    LL_TIM_SetPrescaler(FURI_HAL_RFID_EMULATE_TIMER, 64 - 1);
    LL_TIM_SetCounterMode(FURI_HAL_RFID_EMULATE_TIMER, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(FURI_HAL_RFID_EMULATE_TIMER, 1);
    LL_TIM_DisableARRPreload(FURI_HAL_RFID_EMULATE_TIMER);
    LL_TIM_SetRepetitionCounter(FURI_HAL_RFID_EMULATE_TIMER, 0);

    LL_TIM_SetClockDivision(FURI_HAL_RFID_EMULATE_TIMER, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetClockSource(FURI_HAL_RFID_EMULATE_TIMER, LL_TIM_CLOCKSOURCE_INTERNAL);

    LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
    TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_ENABLE;
    TIM_OC_InitStruct.CompareValue = 1;
    LL_TIM_OC_Init(
        FURI_HAL_RFID_EMULATE_TIMER, FURI_HAL_RFID_EMULATE_TIMER_CHANNEL, &TIM_OC_InitStruct);
    LL_TIM_OC_EnableFast(FURI_HAL_RFID_EMULATE_TIMER, FURI_HAL_RFID_EMULATE_TIMER_CHANNEL);

    LL_TIM_GenerateEvent_UPDATE(FURI_HAL_RFID_EMULATE_TIMER);
}

static void furi_hal_rfid_comp_capture_cb(bool level, void* context) {
    UNUSED(context);
    if(!furi_hal_rfid || !furi_hal_rfid->read_capture_callback) {
        return;
    }

    uint32_t now = DWT->CYCCNT;
    if(!furi_hal_rfid->capture_have_last) {
        furi_hal_rfid->capture_last_dwt = now;
        furi_hal_rfid->capture_have_last = true;
        furi_hal_rfid->capture_have_pulse = false;
        return;
    }

    uint32_t duration = (now - furi_hal_rfid->capture_last_dwt) / (SystemCoreClock / 1000000UL);

    // Discard chatter completely (same as when Viking first worked)
    if(duration < 8) {
        furi_hal_rfid->capture_last_dwt = now;
        return;
    }

    furi_hal_rfid->capture_last_dwt = now;

    // DIY polarity: rising(!level)=pulse, falling=period. Enforce strict alternation —
    // duplicate edges from COMP ringing reset varint_pair and kill EM4100/16.
    if(!level) {
        if(furi_hal_rfid->capture_have_pulse) {
            return;
        }
        furi_hal_rfid->capture_pulse_us = duration;
        furi_hal_rfid->capture_have_pulse = true;
        furi_hal_rfid->read_capture_callback(true, duration, furi_hal_rfid->context);
    } else {
        if(!furi_hal_rfid->capture_have_pulse) {
            return;
        }
        uint32_t period = furi_hal_rfid->capture_pulse_us + duration;
        furi_hal_rfid->capture_have_pulse = false;
        furi_hal_rfid->read_capture_callback(false, period, furi_hal_rfid->context);
    }
}

void furi_hal_rfid_tim_read_capture_start(FuriHalRfidReadCaptureCallback callback, void* context) {
    furi_check(furi_hal_rfid);

    furi_hal_rfid->read_capture_callback = callback;
    furi_hal_rfid->context = context;
    furi_hal_rfid->capture_last_dwt = 0;
    furi_hal_rfid->capture_pulse_us = 0;
    furi_hal_rfid->capture_have_last = false;
    furi_hal_rfid->capture_have_pulse = false;

    // TIM2 = 125 kHz TX on PA5. Capture via COMP1 EXTI + DWT timestamps.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_20);

    furi_hal_rfid_comp_set_callback(furi_hal_rfid_comp_capture_cb, NULL);
    furi_hal_rfid_comp_start();
}

void furi_hal_rfid_tim_read_capture_stop(void) {
    furi_hal_rfid_comp_set_callback(NULL, NULL);
    furi_hal_rfid_comp_stop();

    if(furi_hal_rfid) {
        furi_hal_rfid->read_capture_callback = NULL;
        furi_hal_rfid->capture_have_last = false;
        furi_hal_rfid->capture_have_pulse = false;
    }
}

static void furi_hal_rfid_dma_isr(void* context) {
    UNUSED(context);
#if RFID_DMA_CH1_CHANNEL == LL_DMA_CHANNEL_1
    if(LL_DMA_IsActiveFlag_HT1(RFID_DMA)) {
        LL_DMA_ClearFlag_HT1(RFID_DMA);
        furi_hal_rfid->dma_callback(true, furi_hal_rfid->context);
    }

    if(LL_DMA_IsActiveFlag_TC1(RFID_DMA)) {
        LL_DMA_ClearFlag_TC1(RFID_DMA);
        furi_hal_rfid->dma_callback(false, furi_hal_rfid->context);
    }
#else
#error Update this code. Would you kindly?
#endif
}

void furi_hal_rfid_tim_emulate_dma_start(
    uint32_t* duration,
    uint32_t* pulse,
    size_t length,
    FuriHalRfidDMACallback callback,
    void* context) {
    furi_check(furi_hal_rfid);

    // setup interrupts
    furi_hal_rfid->dma_callback = callback;
    furi_hal_rfid->context = context;

    // setup pins
    furi_hal_rfid_pins_emulate();

    // configure timer
    furi_hal_bus_enable(FURI_HAL_RFID_EMULATE_TIMER_BUS);
    furi_hal_rfid_tim_emulate();
    LL_TIM_OC_SetPolarity(
        FURI_HAL_RFID_EMULATE_TIMER, FURI_HAL_RFID_EMULATE_TIMER_CHANNEL, LL_TIM_OCPOLARITY_HIGH);
    // Both ARR and CCR must advance on the same UPDATE (stock Flipper). CCR on TIM2_CH3
    // desyncs pulse vs period and Flipper misreads EM4100 as Electra with junk epilogue.
    LL_TIM_EnableDMAReq_UPDATE(FURI_HAL_RFID_EMULATE_TIMER);

    // configure DMA "mem -> ARR" channel
    LL_DMA_InitTypeDef dma_config = {0};
    dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (FURI_HAL_RFID_EMULATE_TIMER->ARR);
    dma_config.MemoryOrM2MDstAddress = (uint32_t)duration;
    dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_config.Mode = LL_DMA_MODE_CIRCULAR;
    dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
    dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;
    dma_config.NbData = length;
    dma_config.PeriphRequest = LL_DMAMUX_REQ_TIM2_UP;
    dma_config.Priority = LL_DMA_MODE_NORMAL;
    LL_DMA_Init(RFID_DMA_CH1_DEF, &dma_config);
    LL_DMA_EnableChannel(RFID_DMA_CH1_DEF);

    // configure DMA "mem -> CCR3" channel
#if FURI_HAL_RFID_EMULATE_TIMER_CHANNEL == LL_TIM_CHANNEL_CH3
    dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (FURI_HAL_RFID_EMULATE_TIMER->CCR3);
#else
#error Update this code. Would you kindly?
#endif
    dma_config.MemoryOrM2MDstAddress = (uint32_t)pulse;
    dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_config.Mode = LL_DMA_MODE_CIRCULAR;
    dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
    dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;
    dma_config.NbData = length;
    dma_config.PeriphRequest = LL_DMAMUX_REQ_TIM2_UP;
    dma_config.Priority = LL_DMA_MODE_NORMAL;
    LL_DMA_Init(RFID_DMA_CH2_DEF, &dma_config);
    LL_DMA_EnableChannel(RFID_DMA_CH2_DEF);

    // attach interrupt to one of DMA channels
    furi_hal_interrupt_set_isr(RFID_DMA_CH1_IRQ, furi_hal_rfid_dma_isr, NULL);
    LL_DMA_EnableIT_TC(RFID_DMA_CH1_DEF);
    LL_DMA_EnableIT_HT(RFID_DMA_CH1_DEF);

    // start
    LL_TIM_CC_EnableChannel(FURI_HAL_RFID_EMULATE_TIMER, FURI_HAL_RFID_EMULATE_TIMER_CHANNEL);
    LL_TIM_EnableAllOutputs(FURI_HAL_RFID_EMULATE_TIMER);

    LL_TIM_SetCounter(FURI_HAL_RFID_EMULATE_TIMER, 0);
    LL_TIM_EnableCounter(FURI_HAL_RFID_EMULATE_TIMER);
}

void furi_hal_rfid_tim_emulate_dma_stop(void) {
    LL_TIM_DisableCounter(FURI_HAL_RFID_EMULATE_TIMER);
    LL_TIM_DisableAllOutputs(FURI_HAL_RFID_EMULATE_TIMER);

    furi_hal_interrupt_set_isr(RFID_DMA_CH1_IRQ, NULL, NULL);
    LL_DMA_DisableIT_TC(RFID_DMA_CH1_DEF);
    LL_DMA_DisableIT_HT(RFID_DMA_CH1_DEF);

    FURI_CRITICAL_ENTER();

    LL_DMA_DeInit(RFID_DMA_CH1_DEF);
    LL_DMA_DeInit(RFID_DMA_CH2_DEF);

    furi_hal_bus_disable(FURI_HAL_RFID_EMULATE_TIMER_BUS);

    FURI_CRITICAL_EXIT();
}

void furi_hal_rfid_set_read_period(uint32_t period) {
    LL_TIM_SetAutoReload(FURI_HAL_RFID_READ_TIMER, period);
}

void furi_hal_rfid_set_read_pulse(uint32_t pulse) {
    LL_TIM_OC_SetCompareCH1(FURI_HAL_RFID_READ_TIMER, pulse);
}

void furi_hal_rfid_comp_start(void) {
    // DIY: LM2904 → PA1 = COMP1 IO3 (not IO1/PC5)
    LL_COMP_SetInputPlus(COMP1, LL_COMP_INPUT_PLUS_IO3);
    LL_COMP_SetInputMinus(COMP1, LL_COMP_INPUT_MINUS_VREFINT);
    LL_COMP_SetInputHysteresis(COMP1, LL_COMP_HYSTERESIS_HIGH);
    LL_COMP_SetOutputPolarity(COMP1, LL_COMP_OUTPUTPOL_NONINVERTED);
    furi_hal_rfid_comp_out_config();
    LL_COMP_Enable(COMP1);
    // Magic
    uint32_t wait_loop_index = ((80 / 10UL) * ((SystemCoreClock / (100000UL * 2UL)) + 1UL));
    while(wait_loop_index) {
        wait_loop_index--;
    }
}

void furi_hal_rfid_comp_stop(void) {
    LL_COMP_Disable(COMP1);
}

FuriHalRfidCompCallback furi_hal_rfid_comp_callback = NULL;
void* furi_hal_rfid_comp_callback_context = NULL;

void furi_hal_rfid_comp_set_callback(FuriHalRfidCompCallback callback, void* context) {
    FURI_CRITICAL_ENTER();
    furi_hal_rfid_comp_callback = callback;
    furi_hal_rfid_comp_callback_context = context;
    __DMB();
    FURI_CRITICAL_EXIT();
}

/* Comparator trigger event */
void COMP_IRQHandler(void) {
    if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_21)) {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_21);
        // Do not auto-disable EXTI line 21 during timer capture
    }
    if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_20)) {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_20);
        // Do not auto-disable EXTI line 20 during timer capture
    }
    if(furi_hal_rfid_comp_callback) {
        furi_hal_rfid_comp_callback(
            (LL_COMP_ReadOutputLevel(COMP1) == LL_COMP_OUTPUT_LEVEL_LOW),
            furi_hal_rfid_comp_callback_context);
    }
}

static void furi_hal_rfid_field_tim_setup(void) {
    // setup timer counter
    furi_hal_bus_enable(FURI_HAL_RFID_FIELD_COUNTER_TIMER_BUS);

    LL_TIM_SetPrescaler(FURI_HAL_RFID_FIELD_COUNTER_TIMER, 0);
    LL_TIM_SetCounterMode(FURI_HAL_RFID_FIELD_COUNTER_TIMER, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(FURI_HAL_RFID_FIELD_COUNTER_TIMER, 0xFFFFFFFF);
    LL_TIM_DisableARRPreload(FURI_HAL_RFID_FIELD_COUNTER_TIMER);
    LL_TIM_SetRepetitionCounter(FURI_HAL_RFID_FIELD_COUNTER_TIMER, 0);

    LL_TIM_SetClockDivision(FURI_HAL_RFID_FIELD_COUNTER_TIMER, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetClockSource(FURI_HAL_RFID_FIELD_COUNTER_TIMER, LL_TIM_CLOCKSOURCE_EXT_MODE2);
    LL_TIM_ConfigETR(
        FURI_HAL_RFID_FIELD_COUNTER_TIMER,
        LL_TIM_ETR_POLARITY_INVERTED,
        LL_TIM_ETR_PRESCALER_DIV1,
        LL_TIM_ETR_FILTER_FDIV1);

    LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
    TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_ENABLE;
    TIM_OC_InitStruct.CompareValue = 1;
    LL_TIM_OC_Init(
        FURI_HAL_RFID_FIELD_COUNTER_TIMER,
        FURI_HAL_RFID_FIELD_COUNTER_TIMER_CHANNEL,
        &TIM_OC_InitStruct);

    LL_TIM_GenerateEvent_UPDATE(FURI_HAL_RFID_FIELD_COUNTER_TIMER);
    LL_TIM_OC_SetPolarity(
        FURI_HAL_RFID_FIELD_COUNTER_TIMER,
        FURI_HAL_RFID_FIELD_COUNTER_TIMER_CHANNEL,
        LL_TIM_OCPOLARITY_HIGH);
    LL_TIM_EnableDMAReq_UPDATE(FURI_HAL_RFID_FIELD_COUNTER_TIMER);

    // setup timer timeouts dma
    furi_hal_bus_enable(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER_BUS);

    LL_TIM_SetPrescaler(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER, 64000 - 1);
    LL_TIM_SetCounterMode(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER, 100 - 1); // 100 ms
    LL_TIM_SetClockDivision(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetClockSource(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER, LL_TIM_CLOCKSOURCE_INTERNAL);

    LL_TIM_DisableARRPreload(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER);

    LL_TIM_EnableDMAReq_UPDATE(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER);
    LL_TIM_GenerateEvent_UPDATE(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER);
}

void furi_hal_rfid_field_detect_start(void) {
    // setup pins
    furi_hal_rfid_pins_field();

    // configure timer
    furi_hal_rfid_field_tim_setup();

    // configure DMA "TIM_COUNTER_CNT -> counter"
    LL_DMA_SetMemoryAddress(RFID_DMA_CH1_DEF, (uint32_t) & (furi_hal_rfid->field.counter));
    LL_DMA_SetPeriphAddress(
        RFID_DMA_CH1_DEF, (uint32_t) & (FURI_HAL_RFID_FIELD_COUNTER_TIMER->CNT));
    LL_DMA_ConfigTransfer(
        RFID_DMA_CH1_DEF,
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY | LL_DMA_MODE_CIRCULAR | LL_DMA_PERIPH_NOINCREMENT |
            LL_DMA_MEMORY_NOINCREMENT | LL_DMA_PDATAALIGN_WORD | LL_DMA_MDATAALIGN_WORD |
            LL_DMA_PRIORITY_MEDIUM);
    LL_DMA_SetDataLength(RFID_DMA_CH1_DEF, 1);
    LL_DMA_SetPeriphRequest(RFID_DMA_CH1_DEF, FURI_HAL_RFID_FIELD_DMAMUX_DMA);
    LL_DMA_EnableChannel(RFID_DMA_CH1_DEF);

    // configure DMA "mem -> TIM_COUNTER_CNT"
    LL_DMA_SetMemoryAddress(
        RFID_DMA_CH2_DEF, (uint32_t) & (furi_hal_rfid->field.set_tim_counter_cnt));
    LL_DMA_SetPeriphAddress(
        RFID_DMA_CH2_DEF, (uint32_t) & (FURI_HAL_RFID_FIELD_COUNTER_TIMER->CNT));
    LL_DMA_ConfigTransfer(
        RFID_DMA_CH2_DEF,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MODE_CIRCULAR | LL_DMA_PERIPH_NOINCREMENT |
            LL_DMA_MEMORY_NOINCREMENT | LL_DMA_PDATAALIGN_WORD | LL_DMA_MDATAALIGN_WORD |
            LL_DMA_PRIORITY_LOW);
    LL_DMA_SetDataLength(RFID_DMA_CH2_DEF, 1);
    LL_DMA_SetPeriphRequest(RFID_DMA_CH2_DEF, FURI_HAL_RFID_FIELD_DMAMUX_DMA);
    LL_DMA_EnableChannel(RFID_DMA_CH2_DEF);

    // start tim counter
    LL_TIM_EnableAllOutputs(FURI_HAL_RFID_FIELD_COUNTER_TIMER);

    LL_TIM_SetCounter(FURI_HAL_RFID_FIELD_COUNTER_TIMER, 0);
    LL_TIM_EnableCounter(FURI_HAL_RFID_FIELD_COUNTER_TIMER);

    // start tim timeout
    LL_TIM_SetCounter(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER, 0);
    LL_TIM_EnableCounter(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER);
    LL_TIM_EnableIT_UPDATE(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER);
}

void furi_hal_rfid_field_detect_stop(void) {
    LL_TIM_DisableCounter(FURI_HAL_RFID_FIELD_COUNTER_TIMER);
    LL_TIM_DisableAllOutputs(FURI_HAL_RFID_FIELD_COUNTER_TIMER);

    LL_TIM_DisableCounter(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER);

    FURI_CRITICAL_ENTER();

    LL_DMA_DeInit(RFID_DMA_CH1_DEF);
    LL_DMA_DeInit(RFID_DMA_CH2_DEF);

    furi_hal_bus_disable(FURI_HAL_RFID_FIELD_COUNTER_TIMER_BUS);
    furi_hal_bus_disable(FURI_HAL_RFID_FIELD_TIMEOUT_TIMER_BUS);

    furi_hal_rfid_pins_reset();

    FURI_CRITICAL_EXIT();
}

bool furi_hal_rfid_field_is_present(uint32_t* frequency) {
    furi_check(frequency);

    *frequency = furi_hal_rfid->field.counter * 10;
    return (*frequency >= FURI_HAL_RFID_FIELD_FREQUENCY_MIN) &&
           (*frequency <= FURI_HAL_RFID_FIELD_FREQUENCY_MAX);
}
