#include "app_cfg.h"
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "app_main.h"
#include "app_ui.h"
#include "sensor_control.h"
#include "sensor_engine.h"
#include "battery.h"
static sensor_engine_t inputs;
static u32 ticks, timeMs, sleepStarted32;
static bool sleepClockPending;
void led_on(u32 pin) { drv_gpio_write(pin, LED_ON); }
void led_off(u32 pin) { drv_gpio_write(pin, LED_OFF); }
void led_init(void) { led_off(SENSOR_LED_PIN); }
u32 app_nowMs(void)
{
    u32 delta = (u32)(clock_time() - ticks) / (CLOCK_16M_SYS_TIMER_CLK_1US * 1000UL);
    ticks += delta * CLOCK_16M_SYS_TIMER_CLK_1US * 1000UL;
    timeMs += delta;
    return timeMs;
}
/* The 16 MHz hardware tick can restart at retention wake. Account for sleep
 * with the continuously running 32 kHz clock, then rebase the active tick. */
void app_clockResume(void)
{
    if (sleepClockPending) {
        u32 elapsed = (u32)(pm_get_32k_tick() - sleepStarted32);
#if CLOCK_32K_EXT_CRYSTAL
        timeMs += (elapsed / 4096U) * 125U + ((elapsed % 4096U) * 125U) / 4096U;
#else
        timeMs += elapsed / 32U;
#endif
        sleepClockPending = FALSE;
    }
    ticks = clock_time();
}
void app_buttons_init(void)
{
    ticks = clock_time();
    timeMs = 0;
    sleepClockPending = FALSE;
    sensor_engine_init(&inputs, !!drv_gpio_read(SENSOR_REED_PIN), !drv_gpio_read(SENSOR_BUTTON_PIN), 0);
    g_zcl_iasZoneAttrs.zoneStatus = inputs.reed.stable ? 0 : ZONE_STATUS_BIT_ALARM1;
    sensor_markPending();
}
void app_key_handler(void)
{
    if (gSensorCtx.factoryResetPending) return;
    unsigned events = sensor_engine_poll(&inputs, !!drv_gpio_read(SENSOR_REED_PIN),
                                        !drv_gpio_read(SENSOR_BUTTON_PIN), app_nowMs());
    if (events & 2) { sensor_factoryReset(); return; }
    if (events & 1) {
        g_zcl_iasZoneAttrs.zoneStatus = inputs.reed.stable ? 0 : ZONE_STATUS_BIT_ALARM1;
        sensor_ledFadeStart();
        sensor_markPending();
    }
}
void app_sleep(void)
{
    if (sensor_engine_busy(&inputs) || gSensorCtx.factoryResetPending || sensor_ledFadeActive()) return;
    if (!sensor_networkCanSleep() || !sensor_batteryCanSleep()) return;
    if (!bdb_isIdle() || tl_stackBusy() || !zb_isTaskDone()) return;
    /* Arm the opposite *stable* reed level, so an edge between scan and sleep
     * already satisfies wakeup. Never arm from a later raw sample. */
    drv_pm_pinCfg_t pins[] = {
        {SENSOR_BUTTON_PIN, PM_WAKEUP_LEVEL_LOW},
        {SENSOR_REED_PIN, inputs.reed.stable ? PM_WAKEUP_LEVEL_LOW : PM_WAKEUP_LEVEL_HIGH},
    };
    drv_pm_wakeupPinConfig(pins, 2);
    if (drv_pm_wakeupPinValid(pins, 2) != SUCCESS) {
        app_nowMs();
        sleepStarted32 = pm_get_32k_tick();
        sleepClockPending = TRUE;
        drv_pm_lowPowerEnter();
        app_clockResume(); /* Suspend can return without running user_init(). */
    }
}
