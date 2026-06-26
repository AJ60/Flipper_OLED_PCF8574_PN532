#include "input.h"

#include "input_settings.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <furi.h>
#include <furi_hal_gpio.h>
#define USE_MCP23017
#ifdef USE_MCP23017
#include <furi_hal_mcp23017.h>
#endif
#include <furi_hal_vibro.h>
#include <toolbox/cli/cli_command.h>
#include <cli/cli_main_commands.h>
#include <toolbox/pipe.h>

#define INPUT_DEBOUNCE_TICKS_HALF (INPUT_DEBOUNCE_TICKS / 2)
#define INPUT_PRESS_TICKS 200
#define INPUT_LONG_PRESS_COUNTS 5
#define INPUT_THREAD_FLAG_ISR 0x00000001

// Wake the input thread at least this often even without an interrupt. This lets
// us poll the MCP and recover from a silent reset that would otherwise leave the
// buttons dead because no further INT edge ever arrives.
#define INPUT_IDLE_WAIT_TICKS 500
// How often to verify/restore MCP configuration while idle. Kept long because
// the probe touches the shared power I2C bus; doing it too often starves other
// consumers (battery monitor) and hurts responsiveness under heavy SPI load.
#define INPUT_MCP_RESTORE_PERIOD_MS 3000

#define TAG "Input"

typedef struct {
    const InputPin* pin;
    volatile bool state;
    volatile uint8_t debounce;
    FuriTimer* press_timer;
    FuriPubSub* event_pubsub;
    volatile uint8_t press_counter;
    volatile uint32_t counter;
} InputPinState;

// Cached MCP GPIO state. Initialized to all-high because the buttons are
// active-low with pull-ups; an unread MCP must not look like "all pressed".
static volatile uint16_t g_mcp_gpio_state = 0xFFFF;

// Default mapping from input indices to MCP pins.
// Update this array to match your hardware wiring.
static const uint8_t mcp_pin_map_default[] = {
    0, // Up
    4, // Down
    1, // Right
    5, // Left
    2, // OK
    3, // Back
};

// Return the MCP bit mask for a given input index.
static uint16_t input_mcp_mask_for_index(size_t idx) {
    size_t cnt = sizeof(mcp_pin_map_default) / sizeof(mcp_pin_map_default[0]);
    if(idx < cnt) {
        return (uint16_t)(1u << mcp_pin_map_default[idx]);
    }
    return 0;
}

// Read the cached MCP state for a button by input index.
#define GPIO_Read_MCP_BY_IDX(idx) \
    (((g_mcp_gpio_state & input_mcp_mask_for_index(idx)) != 0) ^ (input_pins[idx].inverted))

// Trigger vibro feedback if the current input event type is enabled in settings.
static void input_vibro_notify(InputSettings* settings, InputType type) {
    if(settings->vibro_touch_level &&
       ((1 << type) & settings->vibro_touch_trigger_mask)) {
        furi_delay_tick(1);
        furi_hal_vibro_on(true);
        furi_delay_tick(settings->vibro_touch_level * 10);
        furi_hal_vibro_on(false);
    }
}

void input_press_timer_callback(void* arg) {
    if(!arg) return;
    InputPinState* input_pin = arg;
    if(!input_pin->pin) return;

    InputEvent event;
    event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
    event.sequence_counter = input_pin->counter;
    event.key = input_pin->pin->key;

    input_pin->press_counter++;

    if(input_pin->press_counter == INPUT_LONG_PRESS_COUNTS) {
        event.type = InputTypeLong;
        if(input_pin->event_pubsub) {
            FURI_LOG_D(
                TAG,
                "Publish: key=%s type=Long seq=%u",
                input_pin->pin->name,
                (unsigned)event.sequence_counter);
            furi_pubsub_publish(input_pin->event_pubsub, &event);
        }
    } else if(input_pin->press_counter > INPUT_LONG_PRESS_COUNTS) {
        input_pin->press_counter--;
        event.type = InputTypeRepeat;
        if(input_pin->event_pubsub) {
            FURI_LOG_D(
                TAG,
                "Publish: key=%s type=Repeat seq=%u",
                input_pin->pin->name,
                (unsigned)event.sequence_counter);
            furi_pubsub_publish(input_pin->event_pubsub, &event);
        }
    }
}

// Interrupt callback: wake the input thread when MCP23017 INT fires.
void input_isr(void* _ctx) {
    FuriThreadId thread_id = (FuriThreadId)_ctx;
    furi_thread_flags_set(thread_id, INPUT_THREAD_FLAG_ISR);
}

const char* input_get_key_name(InputKey key) {
    for(size_t i = 0; i < input_pins_count; i++) {
        if(input_pins[i].key == key) {
            return input_pins[i].name;
        }
    }
    return "Unknown";
}

const char* input_get_type_name(InputType type) {
    switch(type) {
    case InputTypePress:
        return "Press";
    case InputTypeRelease:
        return "Release";
    case InputTypeShort:
        return "Short";
    case InputTypeLong:
        return "Long";
    case InputTypeRepeat:
        return "Repeat";
    default:
        return "Unknown";
    }
}

int32_t input_srv(void* p) {
    UNUSED(p);

    const FuriThreadId thread_id = furi_thread_get_current_id();
    FuriPubSub* event_pubsub = furi_pubsub_alloc();
    FuriPubSub* ascii_pubsub = furi_pubsub_alloc();
    uint32_t counter = 1;
    furi_record_create(RECORD_INPUT_EVENTS, event_pubsub);
    furi_record_create(RECORD_ASCII_EVENTS, ascii_pubsub);

    // Load input settings and expose them through the input settings record.
    InputSettings* settings = malloc(sizeof(InputSettings));
    input_settings_load(settings);
    furi_record_create(RECORD_INPUT_SETTINGS, settings);

    InputPinState* pin_states = malloc(sizeof(InputPinState) * input_pins_count);
    if(!pin_states) {
        FURI_LOG_E(TAG, "Failed to allocate pin states");
        return 0;
    }

    // Initialize pin state structures.
    for(size_t i = 0; i < input_pins_count; i++) {
        pin_states[i].pin = &input_pins[i];
        pin_states[i].state = false;
        pin_states[i].debounce = INPUT_DEBOUNCE_TICKS_HALF;
        pin_states[i].press_timer = furi_timer_alloc(
            input_press_timer_callback, FuriTimerTypePeriodic, &pin_states[i]);
        if(!pin_states[i].press_timer) {
            FURI_LOG_W(TAG, "Timer alloc failed for pin %u", (unsigned)i);
        }
        pin_states[i].event_pubsub = event_pubsub;
        pin_states[i].press_counter = 0;
        pin_states[i].counter = 0;

        FURI_LOG_I(
            TAG,
            "Init: idx=%u key=%s mcp_pin=%u",
            (unsigned)i,
            input_pins[i].name,
            (unsigned)mcp_pin_map_default[i]);
    }

#ifdef USE_MCP23017
    // Full interrupt mask for all mapped buttons; reused for periodic restore.
    uint16_t mcp_int_mask = 0;
    for(size_t i = 0; i < input_pins_count; i++) {
        mcp_int_mask |= input_mcp_mask_for_index(i);
    }

    // Initialize MCP23017 and configure the mapped inputs as interrupt-driven inputs.
    if(!furi_hal_mcp23017_init()) {
        FURI_LOG_E(TAG, "MCP23017 init failed");
    } else {
        FURI_LOG_I(TAG, "MCP interrupt mask: 0x%04X", mcp_int_mask);
        furi_hal_mcp23017_configure_interrupts(mcp_int_mask);

        // Seed the cache with the current MCP state to avoid spurious events.
        uint8_t tmp_state = 0;
        if(furi_hal_mcp23017_read_port(0, &tmp_state)) {
            g_mcp_gpio_state = (uint16_t)tmp_state | 0xFF00;
            FURI_LOG_I(TAG, "Initial MCP state: 0x%02X", (unsigned)tmp_state);

            for(size_t j = 0; j < input_pins_count; j++) {
                pin_states[j].state = GPIO_Read_MCP_BY_IDX(j);
                pin_states[j].debounce = INPUT_DEBOUNCE_TICKS;

                FURI_LOG_I(
                    TAG,
                    "Seed: idx=%u key=%s state=%u",
                    (unsigned)j,
                    input_pins[j].name,
                    (unsigned)pin_states[j].state);
            }
        } else {
            FURI_LOG_W(TAG, "Failed to read initial MCP state");
        }

        // Attach MCP interrupt to the input thread.
        furi_hal_mcp23017_attach_int(input_isr, (void*)thread_id);
        furi_hal_gpio_init(&gpio_mcp_int, GpioModeInterruptRiseFall, GpioPullUp, GpioSpeedLow);
        furi_hal_gpio_add_int_callback(
            &gpio_mcp_int,
            (GpioExtiCallback)furi_hal_mcp23017_handle_int,
            NULL);
        furi_hal_gpio_enable_int_callback(&gpio_mcp_int);

        FURI_LOG_I(TAG, "MCP interrupt callback attached");
    }
#endif

#ifdef USE_MCP23017
    uint32_t last_restore_tick = furi_get_tick();
#endif

    while(1) {
        bool is_changing = false;

#ifdef USE_MCP23017
        // Read MCP state once per loop and update the cached value.
        uint8_t new_state = 0;
        if(furi_hal_mcp23017_read_port(0, &new_state)) {
            uint16_t cached_val = (uint16_t)new_state | 0xFF00;
            if(g_mcp_gpio_state != cached_val) {
                FURI_LOG_D(
                    TAG,
                    "MCP state changed: 0x%02X -> 0x%02X",
                    (unsigned)(uint8_t)g_mcp_gpio_state,
                    (unsigned)new_state);
            }
            g_mcp_gpio_state = cached_val;
        } else {
            FURI_LOG_W(TAG, "Failed to read MCP state");
        }
#endif

        for(size_t i = 0; i < input_pins_count; i++) {
            bool state;
#ifdef USE_MCP23017
            state = GPIO_Read_MCP_BY_IDX(i);
#else
            state = GPIO_Read(pin_states[i]);
#endif

            if(state) {
                if(pin_states[i].debounce < INPUT_DEBOUNCE_TICKS) {
                    pin_states[i].debounce += 1;
                }
            } else {
                if(pin_states[i].debounce > 0) {
                    pin_states[i].debounce -= 1;
                }
            }

            if(pin_states[i].debounce > 0 && pin_states[i].debounce < INPUT_DEBOUNCE_TICKS) {
                is_changing = true;
            } else if(pin_states[i].state != state) {
                FURI_LOG_D(
                    TAG,
                    "Stable change: idx=%u key=%s raw=%u stable_prev=%u debounce=%u",
                    (unsigned)i,
                    input_pins[i].name,
                    (unsigned)state,
                    (unsigned)pin_states[i].state,
                    (unsigned)pin_states[i].debounce);

                pin_states[i].state = state;

                InputEvent base_event;
                base_event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
                base_event.key = pin_states[i].pin->key;

                if(state) {
                    pin_states[i].counter = counter++;
                    base_event.sequence_counter = pin_states[i].counter;

                    if(pin_states[i].press_timer) {
                        furi_timer_start(pin_states[i].press_timer, INPUT_PRESS_TICKS);
                    }

                    InputEvent press_event = base_event;
                    press_event.type = InputTypePress;

                    FURI_LOG_D(
                        TAG,
                        "Publish: key=%s type=Press seq=%u",
                        pin_states[i].pin->name,
                        (unsigned)press_event.sequence_counter);

                    furi_pubsub_publish(event_pubsub, &press_event);
                    input_vibro_notify(settings, press_event.type);
                } else {
				base_event.sequence_counter = pin_states[i].counter;

				if(pin_states[i].press_timer) {
					furi_timer_stop(pin_states[i].press_timer);
				}

				// Publish Short before Release for GUI compatibility.
				if(pin_states[i].press_counter < INPUT_LONG_PRESS_COUNTS) {
					InputEvent short_event = base_event;
					short_event.type = InputTypeShort;

					FURI_LOG_D(
						TAG,
						"Publish: key=%s type=Short seq=%u",
						pin_states[i].pin->name,
						(unsigned)short_event.sequence_counter);

					furi_pubsub_publish(event_pubsub, &short_event);
					input_vibro_notify(settings, short_event.type);
				}

				InputEvent release_event = base_event;
				release_event.type = InputTypeRelease;

				FURI_LOG_D(
					TAG,
					"Publish: key=%s type=Release seq=%u press_counter=%u",
					pin_states[i].pin->name,
					(unsigned)release_event.sequence_counter,
					(unsigned)pin_states[i].press_counter);

				furi_pubsub_publish(event_pubsub, &release_event);
				input_vibro_notify(settings, release_event.type);

                    pin_states[i].press_counter = 0;
                }
            }
        }

        if(is_changing) {
            furi_delay_tick(1);
        } else {
            // Wait for an interrupt, but wake periodically so we can self-heal a
            // silently reset MCP that would otherwise never raise INT again.
            // FuriFlagWaitAny clears matched flags on return, so we must NOT
            // clear again afterwards or we could drop an INT raised in between.
            furi_thread_flags_wait(
                INPUT_THREAD_FLAG_ISR, FuriFlagWaitAny, INPUT_IDLE_WAIT_TICKS);

#ifdef USE_MCP23017
            uint32_t now = furi_get_tick();
            if((now - last_restore_tick) >= INPUT_MCP_RESTORE_PERIOD_MS) {
                last_restore_tick = now;
                furi_hal_mcp23017_check_and_restore(mcp_int_mask);
            }
#endif
        }
    }

    return 0;
}