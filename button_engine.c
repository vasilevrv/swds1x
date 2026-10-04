#include "button_engine.h"
#include <string.h>

void button_init(button_state_t *s, bool rawPressed, uint32_t now, bool enabled)
{
    memset(s, 0, sizeof(*s));
    s->rawPressed = s->pressed = rawPressed;
    s->rawSinceMs = s->pressedSinceMs = now;
    s->doubleEnabled = enabled;
    /* A key already held during boot is ignored until its first release. */
    s->armed = !rawPressed;
}

static void finishSingle(button_state_t *s, uint8_t channel, button_emit_t emit)
{
    if (s->pendingSingle) {
        s->pendingSingle = false;
        emit(channel, BUTTON_SINGLE);
    }
    s->secondPress = false;
}

static void checkHold(button_state_t *s, uint8_t channel, uint32_t now,
                      bool service, button_emit_t emit)
{
    uint32_t duration = now - s->pressedSinceMs;
    if (s->armed && s->pressed && !s->holdSent &&
        (service ? duration >= SERVICE_RESET_MS : duration > BUTTON_HOLD_MS)) {
        if (!service) finishSingle(s, channel, emit);
        s->holdSent = true;
        emit(channel, service ? BUTTON_RESET : BUTTON_HOLD);
    }
}

void button_poll(button_state_t *s, uint8_t channel, bool raw, uint32_t now,
                 bool enabled, bool service, button_emit_t emit)
{
    if (s->doubleEnabled != enabled) {
        /* Complete a previously accepted click once; do not reinterpret an
         * in-progress second press after a configuration change. */
        finishSingle(s, channel, emit);
        s->doubleEnabled = enabled;
    }
    if (raw != s->rawPressed) {
        s->rawPressed = raw;
        s->rawSinceMs = now;
    }
    if (raw != s->pressed && (uint32_t)(now - s->rawSinceMs) >= BUTTON_DEBOUNCE_MS) {
        uint32_t edge = s->rawSinceMs;
        if (raw) {
            s->secondPress = s->pendingSingle &&
                (uint32_t)(edge - s->firstReleasedMs) <= BUTTON_DOUBLE_CLICK_MS;
            if (s->pendingSingle && !s->secondPress) finishSingle(s, channel, emit);
            s->pressed = true;
            s->pressedSinceMs = edge;
            s->holdSent = false;
        } else {
            /* Use the physical edge time: debounce must not turn a short press
             * ending exactly at the hold threshold into a hold. */
            checkHold(s, channel, edge, service, emit);
            s->pressed = false;
            if (!s->armed) {
                s->armed = true;
            } else if (!service && !s->holdSent) {
                if (!enabled) emit(channel, BUTTON_SINGLE);
                else if (s->secondPress) {
                    s->pendingSingle = s->secondPress = false;
                    emit(channel, BUTTON_DOUBLE);
                } else {
                    s->pendingSingle = true;
                    s->firstReleasedMs = edge;
                }
            }
        }
    }
    /* Wait for release debounce instead of classifying a released key as held. */
    if (s->rawPressed) checkHold(s, channel, now, service, emit);
    if (s->pendingSingle && !s->secondPress &&
        (uint32_t)(now - s->firstReleasedMs) >= BUTTON_DOUBLE_CLICK_MS) {
        /* Preserve a second press at the boundary while it is debouncing. */
        bool possibleSecond = s->rawPressed && !s->pressed &&
            (uint32_t)(s->rawSinceMs - s->firstReleasedMs) <= BUTTON_DOUBLE_CLICK_MS;
        if (!possibleSecond) finishSingle(s, channel, emit);
    }
}
