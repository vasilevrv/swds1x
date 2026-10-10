#include "app_cfg.h"

#if (__PROJECT_TL_SWDS1X__)

#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "app_main.h"
#include "sensor_control.h"
#include "app_ui.h"

static bool ledIsOn, fadeActive;
static u32 fadeStarted;
static u16 fadeLevel;
/* PB4 supports PWM4. Compare is HIGH time: LED sinks current when LOW. */
static void ledLevel(u16 onTicks)
{
    if (!onTicks || onTicks == 1000) {
        drv_pwm_stop(PWM4_ID);
        drv_gpio_write(SENSOR_LED_PIN, onTicks ? LED_ON : LED_OFF);
        drv_gpio_func_set(SENSOR_LED_PIN);
    } else {
        pwm_set_clk(CLOCK_SYS_CLOCK_HZ, 1000000);
        drv_pwm_cfg(PWM4_ID, 1000 - onTicks, 1000);
        gpio_set_func(SENSOR_LED_PIN, AS_PWM4);
        drv_pwm_start(PWM4_ID);
    }
    ledIsOn = onTicks != 0;
}
bool sensor_ledFadeActive(void) { return fadeActive; }
void sensor_ledRestore(void)
{
    ledLevel(fadeActive ? fadeLevel : (ledIsOn ? 1000 : 0));
}

static s32 statusLedBlinkTimerCb(void *arg)
{
    (void)arg;

    if (ledIsOn) {
        ledLevel(0);
        if (gSensorCtx.times) {
            gSensorCtx.times--;
            if (!gSensorCtx.times) {
                gSensorCtx.timerLedEvt = NULL;
                return -1;
            }
        }
        return gSensorCtx.ledOffTime;
    }

    ledLevel(1000);
    return gSensorCtx.ledOnTime;
}

void statusLed_blinkStart(u16 times, u16 onTimeMs, u16 offTimeMs)
{
    statusLed_blinkStop();

    gSensorCtx.times = times;
    gSensorCtx.ledOnTime = onTimeMs;
    gSensorCtx.ledOffTime = offTimeMs;
    ledLevel(1000);
    gSensorCtx.timerLedEvt = TL_ZB_TIMER_SCHEDULE(statusLedBlinkTimerCb, NULL, onTimeMs);
    if (!gSensorCtx.timerLedEvt) {
        statusLed_blinkStop();
    }
}

void statusLed_blinkStop(void)
{
    if (gSensorCtx.timerLedEvt) {
        TL_ZB_TIMER_CANCEL(&gSensorCtx.timerLedEvt);
    }
    gSensorCtx.times = 0;
    fadeActive = FALSE;
    fadeLevel = 0;
    ledLevel(0);
}


static s32 ledFadeCb(void *arg)
{
    (void)arg;
    u32 elapsed = (u32)(app_nowMs() - fadeStarted);
    if (elapsed >= 1000) {
        fadeActive = FALSE;
        fadeLevel = 0;
        ledLevel(0);
        gSensorCtx.timerLedEvt = NULL;
        return -1;
    }
    u32 ramp = elapsed <= 500 ? elapsed : 1000 - elapsed;
    /* Squared duty gives a gentler visual ramp than linear electrical duty. */
    fadeLevel = ramp * ramp / 250U;
    ledLevel(fadeLevel);
    return 10;
}
void sensor_ledFadeStart(void)
{
    statusLed_blinkStop();
    fadeActive = TRUE;
    fadeStarted = app_nowMs();
    gSensorCtx.timerLedEvt = TL_ZB_TIMER_SCHEDULE(ledFadeCb, NULL, 10);
    if (!gSensorCtx.timerLedEvt) statusLed_blinkStop();
}


/* Small ordered queue preserves close/open transitions while APS is busy.
 * Offline transitions are replaced by the current physical state on rejoin. */
#define EVENT_QUEUE_SIZE 8
static u16 events[EVENT_QUEUE_SIZE];
static u8 eventHead, eventCount;
static u32 lastAttempt;
void sensor_markPending(void)
{
    if (!zb_isDeviceJoinedNwk()) { eventHead = eventCount = 0; }
    if (eventCount == EVENT_QUEUE_SIZE) {
        /* Preserve oldest events, replace the newest with the latest state. */
        events[(eventHead + eventCount - 1) % EVENT_QUEUE_SIZE] = g_zcl_iasZoneAttrs.zoneStatus;
        return;
    }
    events[(eventHead + eventCount++) % EVENT_QUEUE_SIZE] = g_zcl_iasZoneAttrs.zoneStatus;
}
void sensor_notifyTask(void)
{
    if (!eventCount || !zb_isDeviceJoinedNwk() ||
        g_zcl_iasZoneAttrs.zoneState != ZONE_STATE_ENROLLED ||
        !zb_bindingTblSearched(ZCL_CLUSTER_SS_IAS_ZONE, 1)) return;
    u32 now = app_nowMs();
    if ((u32)(now - lastAttempt) < 100U) return;
    lastAttempt = now;
    epInfo_t dst = {0};
    dst.txOptions = APS_TX_OPT_ACK_TX;
    dst.profileId = HA_PROFILE_ID;
    dst.dstAddrMode = APS_DSTADDR_EP_NOTPRESETNT;
    zoneStatusChangeNoti_t notification = {0};
    notification.zoneStatus = events[eventHead];
    notification.zoneId = g_zcl_iasZoneAttrs.zoneId;
    if (zcl_iasZone_statusChangeNotificationCmd(1, &dst, TRUE, &notification) == ZCL_STA_SUCCESS) {
        eventHead = (eventHead + 1) % EVENT_QUEUE_SIZE;
        --eventCount;
    }
}
static s32 resetCb(void *arg)
{
    (void)arg;
    zb_factoryReset(); /* SDK clears network, binding, IAS and APP NV and reboots. */
    return -1;
}
void sensor_factoryReset(void)
{
    if (gSensorCtx.factoryResetPending) return;
    gSensorCtx.factoryResetPending = TRUE;
    sensor_identifyStop();
    statusLed_blinkStart(3, 100, 100);
    if (!TL_ZB_TIMER_SCHEDULE(resetCb, NULL, 700)) resetCb(NULL);
}

#endif /* __PROJECT_TL_SWDS1X__ */
