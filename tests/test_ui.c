#define HOST_CONTROL_TEST
#include "host_sdk.h"
#define SMART_PLUG_RELAY_PIN 3
#define SMART_PLUG_RELAY_ACTIVE_LEVEL 1
#define SMART_PLUG_LED_PIN 2
#define VK_SW1 1
#define ZCL_ONOFF_STATUS_ON 1
#define ZCL_ONOFF_STATUS_OFF 0
#define ZCL_CMD_ONOFF_OFF 0
#define ZCL_CMD_ONOFF_ON 1
#define ZCL_STA_SUCCESS 0
#define ZCL_STA_UNSUP_CLUSTER_COMMAND 0x81
#define ZCL_STA_UNSUP_MANU_CLUSTER_COMMAND 0x83
#define MANUFACTURER_CODE_NONE 0
#define ZCL_FRAME_CLIENT_SERVER_DIR 0
#define ZCL_CLUSTER_GEN_ON_OFF 6
#define ZCL_CMD_SCENE_STORE_SCENE 4
#define ZCL_CMD_SCENE_RECALL_SCENE 5
#define ZCL_MAX_SCENE_EXT_FIELD_SIZE 4
#define BUILD_U16(lo,hi) ((u16)(lo) | ((u16)(hi) << 8))
#define LO_UINT16(v) ((u8)(v))
#define HI_UINT16(v) ((u8)((v) >> 8))
typedef u8 status_t;
typedef struct { s32 (*cb)(void *); void *arg; u32 period; } ev_timer_event_t;
static ev_timer_event_t timers[8];
static u8 timer_count;
static bool timer_fail, relay, led;
static u32 resets, energy_resets, toggles;
static struct {
    ev_timer_event_t *timerLedEvt;
    u32 keyPressedTime;
    u8 state;
    bool factoryResetPending;
    u16 ledOnTime, ledOffTime, times;
} gSmartPlugCtx;
typedef struct { bool onOff; } zcl_onOffAttr_t;
static zcl_onOffAttr_t onoff;
#define zcl_onoffAttrGet() (&onoff)
typedef struct { u8 dstEp, dirCluster; } zclIncomingAddrInfo_t;
typedef struct { u8 extFieldLen, extField[4]; } zcl_sceneEntry_t;
typedef struct { struct { u16 manufCode; } hdr; } zclIncoming_t;
typedef u8 zclAttrInfo_t;
typedef void (*cluster_forAppCb_t)(void);
static status_t (*config_handler)(zclIncoming_t *);
static status_t zcl_registerCluster(u8 ep, u16 cluster, u16 manufacturer, u8 count,
                                   const zclAttrInfo_t *attrs,
                                   status_t (*handler)(zclIncoming_t *), cluster_forAppCb_t cb) {
    (void)ep; (void)cluster; (void)manufacturer; (void)count; (void)attrs; (void)cb;
    config_handler=handler; return 0;
}
static void drv_gpio_write(u32 pin, bool value) {
    if (pin==SMART_PLUG_RELAY_PIN) relay=value; else led=value;
}
static ev_timer_event_t *schedule(s32 (*cb)(void *),void *arg,u32 period) {
    if (timer_fail) return NULL;
    assert(timer_count<8);
    timers[timer_count]=(ev_timer_event_t){cb,arg,period};
    return &timers[timer_count++];
}
#define TL_ZB_TIMER_SCHEDULE(cb,arg,period) schedule(cb,arg,period)
#define TL_ZB_TIMER_CANCEL(p) (*(p)=NULL)
static struct { u8 cnt,keycode[1]; } kb_event;
static bool key_event;
static bool kb_scan_key(u8 a,u8 b) { (void)a; (void)b; bool v=key_event; key_event=false; return v; }
static void zb_factoryReset(void) { resets++; }
void smartPlug_factoryReset(void) { energy_resets++; }
static void smartPlug_onOffInit(void) {}
static void smartPlug_onOffUpdate(u8 cmd) {
    onoff.onOff=(cmd==1); relay=onoff.onOff; toggles++;
}
static status_t smartPlug_onOffCb(zclIncomingAddrInfo_t *address,u8 command,void *payload) {
    (void)address; (void)payload; smartPlug_onOffUpdate(command); return 0;
}
#include "ui_sources.inc"

static void key(bool pressed) {
    kb_event.cnt=pressed; kb_event.keycode[0]=VK_SW1; key_event=true;
    app_key_handler();
}
int main(void) {
    key(true); now_ticks+=100000U*16; key(false); assert(relay && toggles==1);
    key(true); now_ticks+=100000U*16; key(false); assert(!relay && toggles==2);
    key(true); now_ticks+=9999000U*16; app_key_handler(); assert(!resets);
    now_ticks+=2000U*16; app_key_handler();
    assert(gSmartPlugCtx.factoryResetPending && !relay && led && timer_count==2);
    key(false); assert(toggles==3); /* long release must not toggle */
    assert(timers[1].period==700); timers[1].cb(NULL);
    assert(resets==1 && energy_resets==1);
    statusLed_blinkStart(1,150,1);
    ev_timer_event_t *blink=gSmartPlugCtx.timerLedEvt;
    assert(led && blink->cb(NULL)==-1 && !led && !gSmartPlugCtx.timerLedEvt);
    timer_fail=true; statusLed_blinkStart(1,150,1); assert(!led);
    gSmartPlugCtx.factoryResetPending=false;
    buttonKeepPressed(VK_SW1); assert(resets==2 && energy_resets==2 && !led);

    zclIncomingAddrInfo_t address={SMART_PLUG_ENDPOINT,0};
    zcl_sceneEntry_t valid={4,{6,0,1,1}};
    sceneRecall(&address,&valid); assert(relay);
    u32 before=toggles;
    zcl_sceneEntry_t invalid={4,{6,0,0,1}}; /* valid empty TLV followed by garbage */
    sceneRecall(&address,&invalid); assert(toggles==before);
    invalid=(zcl_sceneEntry_t){4,{6,0,1,2}};
    sceneRecall(&address,&invalid); assert(toggles==before);
    smartPlugConfigRegister(1,0,0,NULL,NULL); assert(config_handler);
    zclIncoming_t message={{0}};
    assert(config_handler(&message)==ZCL_STA_UNSUP_CLUSTER_COMMAND);
    message.hdr.manufCode=1;
    assert(config_handler(&message)==ZCL_STA_UNSUP_MANU_CLUSTER_COMMAND);
    puts("PASS: button, 10-second hold, reset timer fallback, LED, scenes, custom cluster dispatch");
    return 0;
}
