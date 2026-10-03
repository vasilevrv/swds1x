#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__)

#include "tl_common.h"
#include "zcl_include.h"
#include "app_main.h"
#include "plug_control.h"
#include "app_ui.h"

static bool ledIsOn;

void relay_init(void)
{
    drv_gpio_write(SMART_PLUG_RELAY_PIN, !SMART_PLUG_RELAY_ACTIVE_LEVEL);
}

void relay_restore(void)
{
    smartPlug_onOffInit();
}

void relay_set(bool on)
{
    drv_gpio_write(SMART_PLUG_RELAY_PIN,
                   on ? SMART_PLUG_RELAY_ACTIVE_LEVEL : !SMART_PLUG_RELAY_ACTIVE_LEVEL);
}

static s32 statusLedBlinkTimerCb(void *arg)
{
    (void)arg;

    if (ledIsOn) {
        led_off(SMART_PLUG_LED_PIN);
        ledIsOn = FALSE;
        if (gSmartPlugCtx.times) {
            gSmartPlugCtx.times--;
            if (!gSmartPlugCtx.times) {
                gSmartPlugCtx.timerLedEvt = NULL;
                return -1;
            }
        }
        return gSmartPlugCtx.ledOffTime;
    }

    led_on(SMART_PLUG_LED_PIN);
    ledIsOn = TRUE;
    return gSmartPlugCtx.ledOnTime;
}

void statusLed_blinkStart(u16 times, u16 onTimeMs, u16 offTimeMs)
{
    if (gSmartPlugCtx.timerLedEvt) {
        TL_ZB_TIMER_CANCEL(&gSmartPlugCtx.timerLedEvt);
    }

    gSmartPlugCtx.times = times;
    gSmartPlugCtx.ledOnTime = onTimeMs;
    gSmartPlugCtx.ledOffTime = offTimeMs;
    led_on(SMART_PLUG_LED_PIN);
    ledIsOn = TRUE;
    gSmartPlugCtx.timerLedEvt = TL_ZB_TIMER_SCHEDULE(statusLedBlinkTimerCb, NULL, onTimeMs);
    if (!gSmartPlugCtx.timerLedEvt) {
        statusLed_blinkStop();
    }
}

void statusLed_blinkStop(void)
{
    if (gSmartPlugCtx.timerLedEvt) {
        TL_ZB_TIMER_CANCEL(&gSmartPlugCtx.timerLedEvt);
    }
    gSmartPlugCtx.times = 0;
    led_off(SMART_PLUG_LED_PIN);
    ledIsOn = FALSE;
}

#endif
