#include "app_cfg.h"

#if (__PROJECT_TL_RELAY_2CH__)

#include "tl_common.h"
#include "zcl_include.h"
#include "app_main.h"
#include "relay_control.h"
#include "app_ui.h"

static bool ledIsOn;

void relay_init(void)
{
    relay_set(0, FALSE);
    relay_set(1, FALSE);
}

void relay_restore(void)
{
    relay2_onOffInit();
}

void relay_set(u8 channel, bool on)
{
    if (channel >= 2) return;
    u32 pin = channel ? RELAY2_PIN : RELAY1_PIN;
    bool active = channel ? RELAY2_ACTIVE_LEVEL : RELAY1_ACTIVE_LEVEL;
    drv_gpio_write(pin, on ? active : !active);
}

static s32 statusLedBlinkTimerCb(void *arg)
{
    (void)arg;

    if (ledIsOn) {
        led_off(RELAY_LED_PIN);
        ledIsOn = FALSE;
        if (gRelayCtx.times) {
            gRelayCtx.times--;
            if (!gRelayCtx.times) {
                gRelayCtx.timerLedEvt = NULL;
                return -1;
            }
        }
        return gRelayCtx.ledOffTime;
    }

    led_on(RELAY_LED_PIN);
    ledIsOn = TRUE;
    return gRelayCtx.ledOnTime;
}

void statusLed_blinkStart(u16 times, u16 onTimeMs, u16 offTimeMs)
{
    if (gRelayCtx.timerLedEvt) {
        TL_ZB_TIMER_CANCEL(&gRelayCtx.timerLedEvt);
    }

    gRelayCtx.times = times;
    gRelayCtx.ledOnTime = onTimeMs;
    gRelayCtx.ledOffTime = offTimeMs;
    led_on(RELAY_LED_PIN);
    ledIsOn = TRUE;
    gRelayCtx.timerLedEvt = TL_ZB_TIMER_SCHEDULE(statusLedBlinkTimerCb, NULL, onTimeMs);
    if (!gRelayCtx.timerLedEvt) {
        statusLed_blinkStop();
    }
}

void statusLed_blinkStop(void)
{
    if (gRelayCtx.timerLedEvt) {
        TL_ZB_TIMER_CANCEL(&gRelayCtx.timerLedEvt);
    }
    gRelayCtx.times = 0;
    led_off(RELAY_LED_PIN);
    ledIsOn = FALSE;
}

#endif
