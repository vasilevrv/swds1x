#include "host_sdk.h"
#define ZCL_BASIC_MAX_LENGTH 32
#define ZCL_SCENE
#define ZCL_WRITE
#define NV_ENABLE 1
#define NV_MODULE_APP 6
#define NV_SUCC 0
#define APS_SHORT_DSTADDR_WITHEP 2
#define APS_TX_OPT_ACK_TX 4
#define HA_PROFILE_ID 0x0104
#define MANUFACTURER_CODE_NONE 0
#define ZCL_SEQ_NUM 0xFF
#define ZCL_FRAME_SERVER_CLIENT_DIR 1
#define ZCL_FRAME_CLIENT_SERVER_DIR 0
#define ZCL_CMD_ONOFF_OFF 0
#define ZCL_CMD_ONOFF_ON 1
#define ZCL_CMD_ONOFF_TOGGLE 2
#define ZCL_ONOFF_STATUS_OFF 0
#define ZCL_ONOFF_STATUS_ON 1
#define ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS 0xFF
#define ZCL_CMD_OFF_WITH_EFFECT 0x40
#define ZCL_CMD_ON_WITH_RECALL_GLOBAL_SCENE 0x41
#define ZCL_CMD_ON_WITH_TIMED_OFF 0x42
#define ZCL_CMD_WRITE 2
#define ZCL_CMD_WRITE_UNDIVIDED 3
#define ZCL_CMD_WRITE_NO_RSP 5
#define ZCL_CLUSTER_GEN_IDENTIFY 3
#define ZCL_CLUSTER_GEN_ON_OFF 6
#define ZCL_ATTRID_START_UP_ONOFF 0x4003
#define ZCL_ATTRID_ON_TIME 0x4001
#define ZCL_ATTRID_OFF_WAIT_TIME 0x4002
#define ZCL_ATTRID_IDENTIFY_TIME 0
#define ZCL_CMD_BASIC_RESET_FAC_DEFAULT 0
#define ZCL_CMD_IDENTIFY 0
#define ZCL_CMD_TRIGGER_EFFECT 0x40
#define IDENTIFY_EFFECT_BLINK 0
#define IDENTIFY_EFFECT_BREATHE 1
#define IDENTIFY_EFFECT_OKAY 2
#define IDENTIFY_EFFECT_CHANNEL_CHANGE 0x0B
#define IDENTIFY_EFFECT_FINISH_EFFECT 0xFE
#define IDENTIFY_EFFECT_STOP_EFFECT 0xFF
#define ZCL_STA_SUCCESS 0
#define ZCL_STA_UNSUP_CLUSTER_COMMAND 0x81
#define ZCL_STA_UNSUP_MANU_CLUSTER_COMMAND 0x83
#define ZCL_CMD_SCENE_STORE_SCENE 4
#define ZCL_CMD_SCENE_RECALL_SCENE 5
#define ZCL_MAX_SCENE_EXT_FIELD_SIZE 4
#define BUILD_U16(lo,hi) ((u16)(lo) | ((u16)(hi) << 8))
#define LO_UINT16(v) ((u8)(v))
#define HI_UINT16(v) ((u8)((v) >> 8))
#define RELAY1_PIN 1
#define RELAY2_PIN 2
#define RELAY_LED_PIN 3
#define BUTTON1_PIN 4
#define BUTTON2_PIN 5
#define SERVICE_BUTTON_PIN 6
#define RELAY_VBAT_ADC_PIN 7
#define RELAY1_ACTIVE_LEVEL 1
#define RELAY2_ACTIVE_LEVEL 0
#define RELAY_LED_ACTIVE_LEVEL 0
#define BUTTON1_ACTIVE_LEVEL 0
#define BUTTON2_ACTIVE_LEVEL 1
#define SERVICE_BUTTON_ACTIVE_LEVEL 0
#define LED_ON 0
#define LED_OFF 1
#define PM_PIN_PULLDOWN_100K 0
#define PM_PIN_PULLUP_10K 1
#define TL_SETSTRUCTCONTENT(s,v) memset(&(s),(v),sizeof(s))
typedef u8 status_t;
typedef u8 bdb_commissionSetting_t;
typedef u8 bdb_appCb_t;
typedef u8 zcl_specClusterInfo_t;
typedef u8 af_simple_descriptor_t;
typedef u8 nlme_leave_cnf_t;
typedef u8 nlme_leave_ind_t;
typedef u8 zdo_nwk_status_ind_t;
typedef struct { s32 (*cb)(void *); void *arg; u32 period; bool active; } ev_timer_event_t;
typedef struct { u8 dstEp, dirCluster; u16 srcAddr; } zclIncomingAddrInfo_t;
typedef struct { u8 extFieldLen, extField[4]; } zcl_sceneEntry_t;
typedef struct { struct { struct { u8 acceptOnlyWhenOn; } bits; } onOffCtrl; u16 onTime, offWaitTime; } zcl_onoff_onWithTimeOffCmd_t;
typedef struct { u16 identifyTime; } zcl_identifyCmd_t;
typedef struct { u8 effectId; } zcl_triggerEffect_t;
typedef struct { struct { u16 cluster_id; u8 dst_ep; } indInfo; } apsdeDataInd_t;
typedef struct { u8 numAttr; struct { u16 attrID; } attrList[2]; } zclWriteCmd_t;
typedef struct { struct { u8 cmd; struct { struct { u8 dir; } bf; } frmCtrl; } hdr; void *msg, *attrCmd; } zclIncoming_t;
typedef struct { struct { u16 shortAddr; } dstAddr; u8 dstAddrMode, dstEp, txOptions; u16 profileId; } epInfo_t;
#include "app_main.h"
#include "relay_2ch.h"
#include "relay_control.h"
#include "button_engine.h"
#include "app_ui.h"
app_ctx_t gRelayCtx;
zcl_onOffAttr_t g_zcl_onOffAttrs[2];
zcl_identifyAttr_t g_zcl_identifyAttrs[2];
zcl_sceneAttr_t g_zcl_sceneAttrs[2];
static ev_timer_event_t timers[32];
static bool failTimer, flashPresent, failFlash, isJoined=true;
static u8 flashState[64];
static unsigned writes, resetCount, sentCount;
static bool gpioValue[8], inputValue[8], outputEnable[8], gpioWritten[8];
static struct { u8 endpoint, event; } sent[16];
static bool joined(void) { return isJoined; }
#define zb_isDeviceJoinedNwk() joined()
static ev_timer_event_t *schedule(s32 (*cb)(void *), void *arg, u32 period) {
    if (failTimer) return NULL;
    for (unsigned i=0;i<32;i++) if (!timers[i].active) {
        timers[i]=(ev_timer_event_t){cb,arg,period,true}; return &timers[i];
    }
    assert(!"timer pool exhausted"); return NULL;
}
static void cancel(ev_timer_event_t **timer) { (*timer)->active=false; *timer=NULL; }
#define TL_ZB_TIMER_SCHEDULE(cb,arg,period) schedule(cb,arg,period)
#define TL_ZB_TIMER_CANCEL(p) cancel(p)
static s32 fire(ev_timer_event_t *timer) {
    assert(timer && timer->active);
    s32 result=timer->cb(timer->arg);
    if (result<0) timer->active=false;
    return result;
}
static void drv_gpio_write(u32 pin,bool value) { assert(pin<8); gpioWritten[pin]=true; gpioValue[pin]=value; }
static bool drv_gpio_read(u32 pin) { return inputValue[pin]; }
static void drv_gpio_func_set(u32 pin) { (void)pin; }
static void drv_gpio_input_en(u32 pin,bool en) { (void)pin; (void)en; }
static void drv_gpio_output_en(u32 pin,bool en) { if (en) assert(gpioWritten[pin]); outputEnable[pin]=en; }
static void drv_gpio_up_down_resistor(u32 pin,u8 pull) { (void)pin; (void)pull; }
static u8 nv_flashReadNew(u8 fresh,u8 module,u8 item,u32 len,u8 *data) {
    assert(fresh==1 && module==6 && item==0x71 && len<=64);
    if (!flashPresent) return 1;
    memcpy(data,flashState,len); return 0;
}
static u8 nv_flashWriteNew(u8 fresh,u8 module,u8 item,u32 len,u8 *data) {
    assert(fresh==1 && module==6 && item==0x71 && len<=64); writes++;
    if (failFlash) return 1;
    memcpy(flashState,data,len); flashPresent=true; return 0;
}
static void zb_factoryReset(void) { resetCount++; flashPresent=false; }
static status_t zcl_sendCmd(u8 ep,epInfo_t *dst,u16 cluster,u8 cmd,u8 specific,u8 dir,
                            u8 disable,u16 manufacturer,u8 seq,u16 len,u8 *payload) {
    assert(ep==1 || ep==2); assert(cluster==0xFC00 && cmd==0 && specific==1);
    assert(dir==1 && disable==1 && manufacturer==0 && seq==0xFF && len==1);
    assert(dst->txOptions==APS_TX_OPT_ACK_TX);
    assert(dst->dstEp==1 && dst->dstAddr.shortAddr==0 && dst->profileId==0x104);
    assert(sentCount<16); sent[sentCount].endpoint=ep; sent[sentCount++].event=*payload; return 0;
}
#include "relay_sources.inc"

static void command(u8 ep,u8 cmd) {
    zclIncomingAddrInfo_t address={ep,0,0};
    assert(relay2_onOffCb(&address,cmd,NULL)==0);
}
static void scan(u32 ms) { now_ticks=ms*16000U; app_key_handler(); }
static void key(u8 ch,bool pressed,u32 ms) {
    inputValue[buttonPins[ch]]=pressed ? activeLevels[ch] : !activeLevels[ch];
    scan(ms); scan(ms+10);
}
int main(void) {
    board_io_init(); assert(outputEnable[1] && outputEnable[2] && !gpioValue[1] && gpioValue[2]);
    assert(!outputEnable[4] && !outputEnable[5] && !outputEnable[6] && gpioValue[3]);
    relay2_init(); relay2_onOffInit();
    assert(writes==1 && !g_zcl_onOffAttrs[0].onOff && !g_zcl_onOffAttrs[1].onOff);
    command(1,1); assert(gpioValue[1] && gpioValue[2]);
    command(2,1); assert(gpioValue[1] && !gpioValue[2]);
    unsigned before=writes; command(2,1); assert(writes==before);
    command(1,0); assert(!gpioValue[1] && !gpioValue[2]);
    zclIncomingAddrInfo_t bad={3,0,0}; assert(relay2_onOffCb(&bad,1,NULL)==0x81);
    bad=(zclIncomingAddrInfo_t){2,1,0}; assert(relay2_onOffCb(&bad,0,NULL)==0x81 && g_zcl_onOffAttrs[1].onOff);

    g_relayConfig[0]=(relay_config_t){0,1}; relay2_configChanged(0);
    relay2_init(); assert(!g_relayConfig[0].buttonLinked && g_relayConfig[0].doubleClickEnabled);
    assert(!g_zcl_onOffAttrs[0].onOff && g_zcl_onOffAttrs[1].onOff);
    /* Fixed OFF/ON startup policies must not write on ordinary toggles. */
    g_zcl_onOffAttrs[0].startUpOnOff=0;
    g_zcl_onOffAttrs[1].startUpOnOff=1;
    relay2_saveState(); before=writes;
    for (unsigned i=0;i<100;i++) { command(1,2); command(2,2); }
    relay2_saveState(); assert(writes==before); /* Also ignore state in combined snapshots. */
    assert(((relay_nv_t *)flashState)->onOff[0]==0 && ((relay_nv_t *)flashState)->onOff[1]==1);
    relay2_init(); relay2_onOffInit();
    assert(writes==before && !g_zcl_onOffAttrs[0].onOff && g_zcl_onOffAttrs[1].onOff);
    command(1,1); command(2,0); assert(writes==before);
    /* Switching to previous/toggle immediately captures the current state. */
    apsdeDataInd_t startupInd={{6,1}}; zclWriteCmd_t startupWrite={1,{{0x4003}}};
    zclIncoming_t startupMsg={{2,{{0}}},&startupInd,&startupWrite};
    g_zcl_onOffAttrs[0].startUpOnOff=0xFF;
    relay2_zclProcessIncomingMsg(&startupMsg); assert(writes==before+1);
    assert(((relay_nv_t *)flashState)->onOff[0]==1);
    before=writes; command(2,1); assert(writes==before);
    command(1,0); assert(writes==before+1);
    g_zcl_onOffAttrs[1].startUpOnOff=2; startupInd.indInfo.dst_ep=2;
    relay2_zclProcessIncomingMsg(&startupMsg); before=writes;
    command(2,0); assert(writes==before+1 && ((relay_nv_t *)flashState)->onOff[1]==0);
    relay2_init(); assert(!g_zcl_onOffAttrs[0].onOff && !g_zcl_onOffAttrs[1].onOff);
    assert(g_zcl_onOffAttrs[0].startUpOnOff==0xFF && g_zcl_onOffAttrs[1].startUpOnOff==2);
    /* Restore the default policy/state for the remaining integration scenarios. */
    g_zcl_onOffAttrs[1].startUpOnOff=0xFF; relay2_saveState(); command(2,1);
    before=writes; failFlash=true; g_relayConfig[1].doubleClickEnabled=1; relay2_configChanged(1);
    assert(writes==before+1); relay2_task(); relay2_task(); assert(writes==before+1);
    now_ticks+=5000001U*16; failFlash=false; relay2_task(); assert(writes==before+2);
    relay_nv_t *stored=(relay_nv_t *)flashState;
    stored->version=2; relay2_init(); assert(g_relayConfig[0].buttonLinked && !g_relayConfig[0].doubleClickEnabled);
    stored->version=1; stored->buttonLinked[0]=2; relay2_init(); assert(g_relayConfig[0].buttonLinked);
    stored->buttonLinked[0]=0; stored->magic=0x504C5547; relay2_init(); assert(!g_zcl_onOffAttrs[1].onOff);
    flashPresent=false; relay2_init();

    g_relayConfig[0].buttonLinked=0; relay2_buttonEvent(0,BUTTON_SINGLE);
    assert(!g_zcl_onOffAttrs[0].onOff && sentCount==1 && sent[0].endpoint==1 && sent[0].event==0);
    relay2_buttonEvent(1,BUTTON_SINGLE); assert(g_zcl_onOffAttrs[1].onOff && sent[1].endpoint==2);
    relay2_buttonEvent(1,BUTTON_DOUBLE); relay2_buttonEvent(1,BUTTON_HOLD);
    assert(g_zcl_onOffAttrs[1].onOff && sent[2].event==1 && sent[3].event==2);
    isJoined=false; relay2_buttonEvent(1,BUTTON_SINGLE); assert(!g_zcl_onOffAttrs[1].onOff && sentCount==4); isJoined=true;

    zcl_sceneEntry_t scene={0}; zclIncomingAddrInfo_t address={2,0,0};
    command(2,1); assert(relay2_sceneCb(&address,4,&scene)==0 && scene.extField[3]==1);
    command(2,0); assert(relay2_sceneCb(&address,5,&scene)==0 && g_zcl_onOffAttrs[1].onOff && !g_zcl_onOffAttrs[0].onOff);
    scene.extField[3]=2; assert(relay2_sceneCb(&address,5,&scene)==0 && g_zcl_onOffAttrs[1].onOff);

    zcl_onoff_onWithTimeOffCmd_t timed={{{0}},1,2};
    address.dstEp=1; relay2_onOffCb(&address,0x42,&timed);
    address.dstEp=2; timed.onTime=3; timed.offWaitTime=4; relay2_onOffCb(&address,0x42,&timed);
    ev_timer_event_t *t0=timedOff[0], *t1=timedOff[1]; assert(t0 && t1 && t0!=t1);
    fire(t0); assert(!g_zcl_onOffAttrs[0].onOff && g_zcl_onOffAttrs[1].onOff);
    fire(t1); assert(g_zcl_onOffAttrs[1].onTime==2);
    fire(t0); assert(g_zcl_onOffAttrs[0].offWaitTime==1); assert(fire(t0)==-1);
    relay2_cancelTimedOff(); assert(!t1->active && !timedOff[1]);
    /* Infinite ON has no timer; a later OFF must start the finite wait. */
    address.dstEp=1; timed.onTime=0xFFFF; timed.offWaitTime=2;
    relay2_onOffCb(&address,0x42,&timed);
    assert(g_zcl_onOffAttrs[0].onOff && !timedOff[0]);
    command(1,0); assert(timedOff[0]);
    fire(timedOff[0]); assert(g_zcl_onOffAttrs[0].offWaitTime==1);
    assert(fire(timedOff[0])==-1 && !g_zcl_onOffAttrs[0].offWaitTime);
    /* Writable countdown attributes use the same timer machinery. */
    command(1,1); g_zcl_onOffAttrs[0].onTime=1;
    apsdeDataInd_t timerInd={{6,1}}; zclWriteCmd_t timerWrite={1,{{0x4001}}};
    zclIncoming_t timerMsg={{2,{{0}}},&timerInd,&timerWrite};
    relay2_zclProcessIncomingMsg(&timerMsg); assert(timedOff[0]);
    fire(timedOff[0]); assert(!g_zcl_onOffAttrs[0].onOff);
    address.dstEp=2;
    command(2,0); failTimer=true; timed.onTime=10; timed.offWaitTime=0;
    relay2_onOffCb(&address,0x42,&timed); assert(!g_zcl_onOffAttrs[1].onOff); failTimer=false;

    g_relayConfig[1].buttonLinked=0;
    apsdeDataInd_t ind={{0xFC00,2}}; zclWriteCmd_t write={1,{{0}}};
    zclIncoming_t msg={{2,{{0}}},&ind,&write};
    relay2_zclProcessIncomingMsg(&msg); relay2_init(); assert(!g_relayConfig[1].buttonLinked);
    relay2_zclIdentifyCmdHandler(2,0,2); assert(g_zcl_identifyAttrs[1].identifyTime==2 && !gpioValue[3]);
    relay2_zclIdentifyCmdHandler(1,0,1); assert(!g_zcl_identifyAttrs[1].identifyTime && g_zcl_identifyAttrs[0].identifyTime==1);
    fire(identifyTimer); assert(!identifyTimer && gpioValue[3]);

    /* Exercise the real GPIO adapter together with the button engine. */
    sentCount=0; now_ticks=0; g_relayConfig[0]=(relay_config_t){1,0}; g_relayConfig[1]=(relay_config_t){1,1};
    command(1,0); command(2,0);
    inputValue[4]=1; inputValue[5]=0; inputValue[6]=1; app_buttons_init();
    key(0,true,100); key(0,false,200); assert(g_zcl_onOffAttrs[0].onOff && sentCount==1);
    key(1,true,300); key(1,false,400); assert(!g_zcl_onOffAttrs[1].onOff && sentCount==1);
    key(1,true,600); key(1,false,700); assert(!g_zcl_onOffAttrs[1].onOff && sentCount==2 && sent[1].event==1);
    key(1,true,1000); scan(2000); assert(sentCount==2); scan(2001); assert(sentCount==3 && sent[2].event==2); key(1,false,2100);
    /* Adapter clock accumulation across hardware tick wrap. */
    ticks=UINT32_MAX-100000U; now_ticks=ticks; timeMs=0; app_buttons_init();
    button_init(&buttons[0],false,0,false); inputValue[4]=0; app_key_handler();
    now_ticks+=160000U; app_key_handler(); inputValue[4]=1; now_ticks+=1600000U; app_key_handler();
    now_ticks+=160000U; app_key_handler(); assert(sentCount==4 && sent[3].event==0);

    /* Service reset immediately disables both relays and blocks later events. */
    now_ticks=0; inputValue[4]=1; inputValue[5]=0; inputValue[6]=1; app_buttons_init();
    command(1,1); command(2,1); key(2,true,100); scan(10100);
    assert(gRelayCtx.factoryResetPending && !gpioValue[1] && gpioValue[2]);
    before=sentCount; relay2_buttonEvent(1,0); command(1,1); assert(sentCount==before && !gpioValue[1]);
    for (unsigned i=0;i<32;i++) if (timers[i].active && timers[i].cb==factoryResetCb) fire(&timers[i]);
    assert(resetCount==1 && !flashPresent);
    gRelayCtx.factoryResetPending=false; failTimer=true; relay2_factoryReset(); assert(resetCount==2); failTimer=false;
    puts("PASS: real relay/UI/NV/ZCL callback/scene logic, independent channels/timers, mixed GPIO polarities, persistent settings, fixed startup modes without state writes, previous/toggle transitions, retry, old/corrupt NV, events/offline control, Identify, tick wrap, service reset and fallback");
}
