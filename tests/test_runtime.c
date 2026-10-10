#include "host_sdk.h"
#include "sensor_engine.h"
#define GPIO_PB4 4
#define GPIO_PD3 3
#define GPIO_PD7 7
#define GPIO_PC5 5
#include "sensor_pins.h"
#define LED_ON 0
#define LED_OFF 1
#define PM_PIN_PULLUP_10K 1
#define PM_PIN_PULLDOWN_100K 2
#define PM_WAKEUP_LEVEL_LOW 0
#define PM_WAKEUP_LEVEL_HIGH 1
#define ZONE_STATUS_BIT_ALARM1 1
#define ZONE_STATE_ENROLLED 1
#define ZCL_CLUSTER_SS_IAS_ZONE 0x500
#define HA_PROFILE_ID 0x104
#define APS_DSTADDR_EP_NOTPRESETNT 0
#define APS_TX_OPT_ACK_TX 4
#define SUCCESS 0
#define FAILURE 1
#define ZCL_STA_SUCCESS 0
static bool online=true;
#undef zb_isDeviceJoinedNwk
#define zb_isDeviceJoinedNwk() online
typedef struct { s32 (*callback)(void *); u32 period; } ev_timer_event_t;
static ev_timer_event_t timerSlots[16];
static u8 timerUsed;
static bool timerFail;
static ev_timer_event_t *schedule(s32 (*cb)(void *), void *arg, u32 ms) {
    (void)arg; if (timerFail) return NULL;
    assert(timerUsed<16); timerSlots[timerUsed]=(ev_timer_event_t){cb,ms}; return &timerSlots[timerUsed++];
}
#define TL_ZB_TIMER_SCHEDULE(cb,arg,ms) schedule(cb,arg,ms)
#define TL_ZB_TIMER_CANCEL(ptr) (*(ptr)=NULL)
static struct { ev_timer_event_t *timerLedEvt; bool factoryResetPending; u16 times,ledOnTime,ledOffTime; } gSensorCtx;
static struct {u16 zoneStatus;u8 zoneId,zoneState;} g_zcl_iasZoneAttrs;
static bool level[16], output[16], input[16];
static bool pwmRunning, pwmPin;
static u8 pulls[16];
static void drv_gpio_output_en(u32 pin,bool value) {output[pin]=value;}
static void drv_gpio_input_en(u32 pin,bool value) {input[pin]=value;}
static void drv_gpio_func_set(u32 pin) {if(pin==4) pwmPin=false;}
static void drv_gpio_up_down_resistor(u32 pin,u8 value) {pulls[pin]=value;}
static void drv_gpio_write(u32 pin,bool value) {level[pin]=value;}
static bool drv_gpio_read(u32 pin) {return level[pin];}
#define PWM4_ID 4
#define AS_PWM4 24
#define CLOCK_SYS_CLOCK_HZ 48000000
static u16 pwmCompare, pwmCycle;
static void drv_pwm_stop(u8 id) {assert(id==4);pwmRunning=false;}
static void drv_pwm_start(u8 id) {assert(id==4);pwmRunning=true;}
static void pwm_set_clk(u32 hz,u32 pwmHz) {assert(hz==48000000 && pwmHz==1000000);}
static void drv_pwm_cfg(u8 id,u16 cmp,u16 cycle) {assert(id==4);pwmCompare=cmp;pwmCycle=cycle;}
static void gpio_set_func(u32 pin,u8 mode) {assert(pin==4 && mode==24);pwmPin=true;}
typedef struct {u32 wakeupPin;u8 wakeupLevel;} drv_pm_pinCfg_t;
static drv_pm_pinCfg_t wakePins[2];
static bool stackBusy, bdbBusy, taskDone=true, injectEdge;
static unsigned sleeps, resets;
static u32 tick32;
static u32 pm_get_32k_tick(void) {return tick32;}
static bool batteryPending;
static bool sensor_batteryCanSleep(void) {return !batteryPending;}
static bool retryPending;
static bool sensor_networkCanSleep(void) {return !retryPending;}
static bool bdb_isIdle(void) {return !bdbBusy;}
static bool tl_stackBusy(void) {return stackBusy;}
static bool zb_isTaskDone(void) {return taskDone;}
static void drv_pm_wakeupPinConfig(drv_pm_pinCfg_t *p,u32 n) {
    assert(n==2); memcpy(wakePins,p,sizeof(wakePins));
    if (injectEdge) level[SENSOR_REED_PIN]=!level[SENSOR_REED_PIN];
}
#include "wakeup_sources.inc"
static void drv_pm_lowPowerEnter(void) {++sleeps;}
static void zb_factoryReset(void) {++resets;}
static void sensor_identifyStop(void) {}
typedef struct {u16 profileId;u8 dstAddrMode,txOptions;} epInfo_t;
typedef struct {u16 zoneStatus;u8 extStatus,zoneId;u16 delay;} zoneStatusChangeNoti_t;
static bool bound=true, sendFail;
static u16 sent[32];
static u8 sentCount;
static bool zb_bindingTblSearched(u16 cluster,u8 ep) {assert(cluster==0x500 && ep==1);return bound;}
#define _CODE_ZCL_
typedef u8 status_t;
#define LO_UINT16(n) ((u8)(n))
#define HI_UINT16(n) ((u8)((n)>>8))
#define ZCL_CMD_ZONE_STATUS_CHANGE_NOTIFICATION 0
#define ZCL_FRAME_SERVER_CLIENT_DIR 1
static status_t zcl_sendCmd(u8 ep,epInfo_t *dst,u16 cluster,u8 cmd,u8 specific,
    u8 dir,u8 disable,u16 manufacturer,u8 sequence,u16 length,u8 *payload)
{
    (void)sequence;
    assert(ep==1 && dst->profileId==HA_PROFILE_ID && dst->txOptions==APS_TX_OPT_ACK_TX);
    assert(cluster==0x500 && cmd==0 && specific && dir==1 && disable && manufacturer==0);
    assert(length==6 && payload[2]==0 && payload[3]==5 && payload[4]==0 && payload[5]==0);
    if(sendFail) return 1;
    sent[sentCount++]=(u16)payload[0] | ((u16)payload[1]<<8);
    return 0;
}
#include "zone_wire_sources.inc"
#define zcl_iasZone_statusChangeNotificationCmd(ep,dst,disable,msg) zcl_iasZone_statusChangeNotification(ep,dst,disable,0,msg)
u32 app_nowMs(void);
void led_on(u32); void led_off(u32);
void sensor_markPending(void); void sensor_factoryReset(void);
void sensor_ledFadeStart(void); bool sensor_ledFadeActive(void);
void statusLed_blinkStart(u16,u16,u16); void statusLed_blinkStop(void);
#include "runtime_sources.inc"
static void at(u32 ms) {now_ticks=ms*16000U;tick32=ms*32U;app_key_handler();}
int main(void) {
    board_io_init();
    assert(output[4] && level[4] && !input[4]);
    assert(input[3] && input[7] && !output[3] && !output[7]);
    assert(pulls[3]==PM_PIN_PULLUP_10K && pulls[7]==PM_PIN_PULLDOWN_100K);
    level[3]=true;level[7]=true;
    app_buttons_init(); assert(g_zcl_iasZoneAttrs.zoneStatus==0);
    app_sleep();assert(sleeps==1 && wakePins[1].wakeupLevel==0);
    stackBusy=true;app_sleep();assert(sleeps==1);stackBusy=false;
    bdbBusy=true;app_sleep();assert(sleeps==1);bdbBusy=false;
    taskDone=false;app_sleep();assert(sleeps==1);taskDone=true;
    retryPending=true;app_sleep();assert(sleeps==1);retryPending=false;
    batteryPending=true;app_sleep();assert(sleeps==1);batteryPending=false;
    injectEdge=true;app_sleep();assert(sleeps==1);injectEdge=false;
    at(1);app_sleep();assert(sleeps==1);at(21);
    assert(g_zcl_iasZoneAttrs.zoneStatus==1 && level[4]);
    assert(sensor_ledFadeActive() && gSensorCtx.timerLedEvt->period==10);
    app_sleep();assert(sleeps==1); // PWM must run continuously through the fade.
    at(121);gSensorCtx.timerLedEvt->callback(NULL);
    assert(pwmRunning && pwmPin && pwmCycle==1000 && pwmCompare==960);
    at(271);gSensorCtx.timerLedEvt->callback(NULL);assert(pwmCompare==750);
    at(521);gSensorCtx.timerLedEvt->callback(NULL);assert(!pwmRunning && !level[4]);
    at(771);gSensorCtx.timerLedEvt->callback(NULL);assert(pwmRunning && pwmCompare==750);
    ev_timer_event_t *oldFade=gSensorCtx.timerLedEvt;
    sensor_ledFadeStart(); // Restart immediately from dark, without finishing old fade.
    assert(gSensorCtx.timerLedEvt!=oldFade && level[4] && !pwmRunning);
    at(1271);gSensorCtx.timerLedEvt->callback(NULL);assert(!level[4]);
    at(1770);gSensorCtx.timerLedEvt->callback(NULL);assert(sensor_ledFadeActive());
    at(1771);assert(gSensorCtx.timerLedEvt->callback(NULL)==-1);
    assert(!sensor_ledFadeActive() && level[4] && !pwmRunning && !pwmPin && !gSensorCtx.timerLedEvt);
    level[4]=true;ledIsOn=true;board_io_init();sensor_ledRestore();assert(!level[4]);
    ledIsOn=false;sensor_ledRestore();assert(level[4]);
    app_sleep();assert(sleeps==2 && wakePins[1].wakeupLevel==1);
    g_zcl_iasZoneAttrs.zoneState=1;g_zcl_iasZoneAttrs.zoneId=5;
    bound=false;now_ticks=2000*16000U;sensor_notifyTask();assert(!sentCount);
    bound=true;sendFail=true;sensor_notifyTask();assert(!sentCount && eventCount==2);
    sendFail=false;now_ticks+=101*16000U;sensor_notifyTask();
    now_ticks+=101*16000U;sensor_notifyTask();assert(sentCount==2 && sent[0]==0 && sent[1]==1);
    // Offline edges coalesce to current state; no fake delivery before rejoin.
    online=false;g_zcl_iasZoneAttrs.zoneStatus=0;sensor_markPending();
    g_zcl_iasZoneAttrs.zoneStatus=1;sensor_markPending();assert(eventCount==1);
    sensor_notifyTask();assert(sentCount==2);online=true;
    now_ticks+=101*16000U;sensor_notifyTask();assert(sent[2]==1);
    // Full queue is bounded and preserves latest state.
    for(u8 i=0;i<20;++i) {g_zcl_iasZoneAttrs.zoneStatus=i;sensor_markPending();}
    assert(eventCount==8 && events[(eventHead+7)%8]==19);
    eventCount=0;
    // Real reed edges restart the fade and preserve both queued Zigbee events.
    level[7]=true;at(2500);at(2520);
    assert(fadeActive && eventCount==1 && events[eventHead]==0);
    oldFade=gSensorCtx.timerLedEvt;
    level[7]=false;at(2600);at(2620);
    assert(fadeActive && gSensorCtx.timerLedEvt!=oldFade && fadeStarted==app_nowMs());
    assert(eventCount==2 && events[(eventHead+1)%8]==1 && level[4]);
    level[3]=false;at(3000);at(3019);app_sleep();assert(sleeps==2);
    assert(!gSensorCtx.factoryResetPending);at(3020);
    assert(gSensorCtx.factoryResetPending && !level[4] && !fadeActive && !pwmRunning);
    assert(timerSlots[timerUsed-1].period==700);timerSlots[timerUsed-1].callback(NULL);assert(resets==1);
    // Hardware tick restarts while the 32 kHz tick keeps time across sleep.
    gSensorCtx.factoryResetPending=false;
    level[3]=true;level[7]=true;now_ticks=250000U*16000U;app_buttons_init();
    u32 savedTime=timeMs;sleepStarted32=tick32;sleepClockPending=true;
    tick32+=5000U*32U;now_ticks=16;app_clockResume();app_key_handler();
    assert(timeMs==savedTime+5000U && !sensor_engine_busy(&inputs));
    level[3]=false;now_ticks=16016;app_key_handler();
    now_ticks+=19*16000U;app_key_handler();assert(!gSensorCtx.factoryResetPending);
    now_ticks+=16000U;app_key_handler();assert(gSensorCtx.factoryResetPending);
    // Sleep compensation also handles the 32 kHz hardware wrap exactly once.
    u32 beforeWrap=timeMs;sleepStarted32=UINT32_MAX-31999U;
    tick32=0;sleepClockPending=true;app_clockResume();
    assert(timeMs==beforeWrap+1000U);app_clockResume();assert(timeMs==beforeWrap+1000U);
    // Exhausted timer pool cannot leave reset stuck or LED permanently lit.
    gSensorCtx.factoryResetPending=false;timerFail=true;sensor_factoryReset();
    assert(resets==2 && level[4]);
    gSensorCtx.factoryResetPending=false;sensor_ledFadeStart();assert(!fadeActive && !pwmRunning && level[4]);
    puts("PASS: production GPIO, one-second PWM fade/restart/sleep gating, opposite-level wake/race, sleep gating, ordered APS retry, queue bounds, offline state, reset fallback");
}
