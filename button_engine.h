#pragma once
#include <stdint.h>
#include <stdbool.h>

#define BUTTON_DEBOUNCE_MS       10U
#define BUTTON_DOUBLE_CLICK_MS   400U
#define BUTTON_HOLD_MS           1000U
#define SERVICE_RESET_MS         10000U

typedef enum {
    BUTTON_SINGLE = 0,
    BUTTON_DOUBLE = 1,
    BUTTON_HOLD = 2,
    BUTTON_RESET = 3,
} button_event_t;

typedef struct {
    uint32_t rawSinceMs, pressedSinceMs, firstReleasedMs;
    bool rawPressed, pressed, holdSent, pendingSingle, secondPress;
    bool armed, doubleEnabled;
} button_state_t;

typedef void (*button_emit_t)(uint8_t channel, button_event_t event);
void button_init(button_state_t *state, bool rawPressed, uint32_t nowMs, bool doubleEnabled);
/* Times are monotonic milliseconds; unsigned subtraction supports wrapping. */
void button_poll(button_state_t *state, uint8_t channel, bool rawPressed,
                 uint32_t nowMs, bool doubleEnabled, bool service, button_emit_t emit);
