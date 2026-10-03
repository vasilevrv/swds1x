#define HOST_CONTROL_TEST
#include "host_sdk.h"
#define ZCL_SCENE
#define ZCL_ONOFF_STATUS_OFF 0
#define ZCL_ONOFF_STATUS_ON 1
#define ZCL_CMD_ONOFF_OFF 0
#define ZCL_CMD_ONOFF_ON 1
#define ZCL_CMD_ONOFF_TOGGLE 2
#define ZCL_CMD_OFF_WITH_EFFECT 0x40
#define ZCL_CMD_ON_WITH_RECALL_GLOBAL_SCENE 0x41
#define ZCL_CMD_ON_WITH_TIMED_OFF 0x42
#define ZCL_FRAME_CLIENT_SERVER_DIR 0
#define ZCL_STA_SUCCESS 0
#define ZCL_STA_UNSUP_CLUSTER_COMMAND 0x81
typedef u8 status_t;
typedef struct { u16 onTime,offWaitTime; u8 startUpOnOff; bool onOff,globalSceneControl; } zcl_onOffAttr_t;
typedef struct { bool sceneValid; } zcl_sceneAttr_t;
typedef struct { u8 dstEp,dirCluster; } zclIncomingAddrInfo_t;
typedef struct { struct { struct { u8 acceptOnlyWhenOn; } bits; } onOffCtrl; u16 onTime,offWaitTime; } zcl_onoff_onWithTimeOffCmd_t;
typedef struct { u8 effectId,effectVariant; } zcl_onoff_offWithEffectCmd_t;
typedef struct { s32 (*cb)(void *); void *arg; u32 period; } ev_timer_event_t;
static ev_timer_event_t timer;
static bool timer_fail;
static struct { bool factoryResetPending; } gSmartPlugCtx;
static zcl_onOffAttr_t onoff;
static zcl_sceneAttr_t scene;
#define zcl_onoffAttrGet() (&onoff)
#define zcl_sceneAttrGet() (&scene)
static bool relay;
static u32 changes,saves;
static void relay_set(bool on) { relay=on; }
static u8 zcl_onOffAttr_save(void) { saves++; return 0; }
static void smartPlug_onRelayStateChanged(bool on) { assert(relay==on); changes++; }
static ev_timer_event_t *schedule(s32 (*cb)(void *),void *arg,u32 period) {
    if (timer_fail) return NULL;
    timer=(ev_timer_event_t){cb,arg,period}; return &timer;
}
#define TL_ZB_TIMER_SCHEDULE(cb,arg,period) schedule(cb,arg,period)
#define TL_ZB_TIMER_CANCEL(p) (*(p)=NULL)
#include "control_sources.inc"

int main(void) {
    zclIncomingAddrInfo_t addr={SMART_PLUG_ENDPOINT,0};
    onoff.onOff=true; /* SDK already applied StartUpOnOff before relay_restore. */
    smartPlug_onOffInit(); assert(relay && onoff.onOff && saves==1);
    smartPlug_onOffCb(&addr,ZCL_CMD_ONOFF_OFF,NULL);
    assert(!relay && changes==1);
    smartPlug_onOffCb(&addr,ZCL_CMD_ONOFF_OFF,NULL); assert(changes==1);
    smartPlug_onOffCb(&addr,ZCL_CMD_ONOFF_TOGGLE,NULL); assert(relay && changes==2);
    assert(smartPlug_onOffCb(&addr,0xFF,NULL)==ZCL_STA_UNSUP_CLUSTER_COMMAND);
    addr.dirCluster=1; smartPlug_onOffCb(&addr,ZCL_CMD_ONOFF_OFF,NULL); assert(relay);
    addr.dirCluster=0; gSmartPlugCtx.factoryResetPending=true;
    smartPlug_onOffUpdate(ZCL_CMD_ONOFF_OFF);
    smartPlug_onOffUpdate(ZCL_CMD_ONOFF_ON); assert(!relay);
    gSmartPlugCtx.factoryResetPending=false;
    zcl_onoff_onWithTimeOffCmd_t cmd={{{0}},1,2};
    smartPlug_onOffCb(&addr,ZCL_CMD_ON_WITH_TIMED_OFF,&cmd);
    assert(relay && onoff.onTime==1 && onoff.offWaitTime==2 && timer.period==100);
    assert(timer.cb(NULL)==0); assert(!relay && onoff.offWaitTime==2);
    assert(timer.cb(NULL)==0 && onoff.offWaitTime==1);
    assert(timer.cb(NULL)==-1 && onoff.offWaitTime==0);
    cmd.onTime=0xFFFF; cmd.offWaitTime=1;
    smartPlug_onOffCb(&addr,ZCL_CMD_ON_WITH_TIMED_OFF,&cmd);
    timer.cb(NULL); assert(relay && onoff.onTime==0xFFFF);
    smartPlug_onOffUpdate(ZCL_CMD_ONOFF_OFF);
    onoff.offWaitTime=0; timer_fail=true; cmd.onTime=10;
    smartPlug_onOffCb(&addr,ZCL_CMD_ON_WITH_TIMED_OFF,&cmd);
    assert(!relay && !onoff.onTime && !onoff.offWaitTime);
    puts("PASS: relay restore, idempotent commands, direction, reset lock, timed-off wait, infinite on-time");
    return 0;
}
