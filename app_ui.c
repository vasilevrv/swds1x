#include "app_cfg.h"
#if (__PROJECT_TL_RELAY_2CH__)
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "app_main.h"
#include "relay_control.h"
#include "relay_2ch.h"
#include "button_engine.h"
#include "app_ui.h"

static button_state_t buttons[3];
static const u32 buttonPins[] = {BUTTON1_PIN, BUTTON2_PIN, SERVICE_BUTTON_PIN};
static const u8 activeLevels[] = {BUTTON1_ACTIVE_LEVEL, BUTTON2_ACTIVE_LEVEL, SERVICE_BUTTON_ACTIVE_LEVEL};
static u32 ticks, timeMs;

void led_on(u32 pin) { drv_gpio_write(pin, LED_ON); }
void led_off(u32 pin) { drv_gpio_write(pin, LED_OFF); }
void led_init(void) { led_off(RELAY_LED_PIN); }

/* SDK hardware tick wraps about every 268 seconds. Accumulating deltas keeps
 * millisecond button deadlines valid across both hardware and ms wraps. */
static u32 nowMs(void)
{
    u32 delta = (u32)(clock_time() - ticks) / (CLOCK_16M_SYS_TIMER_CLK_1US * 1000UL);
    ticks += delta * CLOCK_16M_SYS_TIMER_CLK_1US * 1000UL;
    timeMs += delta;
    return timeMs;
}

static void buttonEvent(u8 channel, button_event_t event)
{
    if (event == BUTTON_RESET) relay2_factoryReset();
    else relay2_buttonEvent(channel, (u8)event);
}

void app_buttons_init(void)
{
    ticks = clock_time();
    timeMs = 0;
    for (u8 ch = 0; ch < 3; ch++) {
        bool pressed = drv_gpio_read(buttonPins[ch]) == activeLevels[ch];
        button_init(&buttons[ch], pressed, 0, ch < 2 ? g_relayConfig[ch].doubleClickEnabled : FALSE);
    }
}

void app_key_handler(void)
{
    if (gRelayCtx.factoryResetPending) return;
    u32 now = nowMs();
    /* Service key has priority over ordinary keys on a reset scan. */
    const u8 order[] = {2, 0, 1};
    for (u8 i = 0; i < 3 && !gRelayCtx.factoryResetPending; i++) {
        u8 ch = order[i];
        button_poll(&buttons[ch], ch, drv_gpio_read(buttonPins[ch]) == activeLevels[ch],
                    now, ch < 2 ? g_relayConfig[ch].doubleClickEnabled : FALSE,
                    ch == 2, buttonEvent);
    }
}
#endif
