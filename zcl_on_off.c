#include "app_cfg.h"
#if (__PROJECT_TL_RELAY_2CH__)
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "app_main.h"
#include "relay_control.h"
#include "relay_2ch.h"

static ev_timer_event_t *timedOff[2];
static u8 timerChannels[2] = {0, 1};

void relay2_onOffInit(void)
{
    /* Registration applies StartUpOnOff once, separately for each endpoint. */
    for (u8 ch = 0; ch < 2; ch++) relay_set(ch, g_zcl_onOffAttrs[ch].onOff);
    relay2_saveState();
}

void relay2_onOffUpdate(u8 ch, u8 cmd)
{
    if (ch >= 2 || (gRelayCtx.factoryResetPending && cmd != ZCL_CMD_ONOFF_OFF)) return;
    zcl_onOffAttr_t *attr = zcl_onoffAttrGet(ch);
    bool previous = attr->onOff;
    if (cmd == ZCL_CMD_ONOFF_ON) attr->onOff = TRUE;
    else if (cmd == ZCL_CMD_ONOFF_OFF) attr->onOff = FALSE;
    else if (cmd == ZCL_CMD_ONOFF_TOGGLE) attr->onOff = !attr->onOff;
    else return;
    if (attr->onOff) {
        attr->globalSceneControl = TRUE;
        if (!attr->onTime) attr->offWaitTime = 0;
    } else {
        attr->onTime = 0;
    }
#ifdef ZCL_SCENE
    zcl_sceneAttrGet(ch)->sceneValid = FALSE;
#endif
    relay_set(ch, attr->onOff);
    if (previous != attr->onOff) relay2_onRelayStateChanged(ch);
    relay2_onOffTimerSync(ch);
}

void relay2_cancelTimedOff(void)
{
    for (u8 ch = 0; ch < 2; ch++) {
        if (timedOff[ch]) TL_ZB_TIMER_CANCEL(&timedOff[ch]);
        g_zcl_onOffAttrs[ch].onTime = g_zcl_onOffAttrs[ch].offWaitTime = 0;
    }
}

static s32 timedOffCb(void *arg)
{
    u8 ch = *(u8 *)arg;
    zcl_onOffAttr_t *attr = zcl_onoffAttrGet(ch);
    if (attr->onOff && attr->onTime && attr->onTime != 0xFFFF) {
        if (!--attr->onTime) relay2_onOffUpdate(ch, ZCL_CMD_ONOFF_OFF);
    } else if (!attr->onOff && attr->offWaitTime && attr->offWaitTime != 0xFFFF) {
        --attr->offWaitTime;
    }
    if ((attr->onOff && attr->onTime && attr->onTime != 0xFFFF) ||
        (!attr->onOff && attr->offWaitTime && attr->offWaitTime != 0xFFFF)) return 0;
    timedOff[ch] = NULL;
    return -1;
}

void relay2_onOffTimerSync(u8 ch)
{
    if (ch >= 2 || gRelayCtx.factoryResetPending || timedOff[ch]) return;
    zcl_onOffAttr_t *attr = zcl_onoffAttrGet(ch);
    if ((attr->onOff && attr->onTime && attr->onTime != 0xFFFF) ||
        (!attr->onOff && attr->offWaitTime && attr->offWaitTime != 0xFFFF)) {
        timedOff[ch] = TL_ZB_TIMER_SCHEDULE(timedOffCb, &timerChannels[ch], 100);
        if (!timedOff[ch]) {
            /* Clear counters first: OFF must not recursively allocate a timer. */
            attr->onTime = attr->offWaitTime = 0;
            relay2_onOffUpdate(ch, ZCL_CMD_ONOFF_OFF);
        }
    }
}

static void timedOffProcess(u8 ch, const zcl_onoff_onWithTimeOffCmd_t *cmd)
{
    zcl_onOffAttr_t *attr = zcl_onoffAttrGet(ch);
    if (cmd->onOffCtrl.bits.acceptOnlyWhenOn && !attr->onOff) return;
    if (timedOff[ch]) TL_ZB_TIMER_CANCEL(&timedOff[ch]);
    if (attr->offWaitTime && !attr->onOff) {
        attr->offWaitTime = min2(attr->offWaitTime, cmd->offWaitTime);
    } else {
        attr->onTime = max2(attr->onTime, cmd->onTime);
        attr->offWaitTime = cmd->offWaitTime;
        relay2_onOffUpdate(ch, ZCL_CMD_ONOFF_ON);
    }
    relay2_onOffTimerSync(ch);
}

status_t relay2_onOffCb(zclIncomingAddrInfo_t *address, u8 cmd, void *payload)
{
    int ch = relay2_channelFromEndpoint(address->dstEp);
    if (ch < 0 || address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR)
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    switch (cmd) {
    case ZCL_CMD_ONOFF_ON:
    case ZCL_CMD_ONOFF_OFF:
    case ZCL_CMD_ONOFF_TOGGLE:
        relay2_onOffUpdate(ch, cmd);
        break;
    case ZCL_CMD_OFF_WITH_EFFECT:
        g_zcl_onOffAttrs[ch].globalSceneControl = FALSE;
        relay2_onOffUpdate(ch, ZCL_CMD_ONOFF_OFF);
        break;
    case ZCL_CMD_ON_WITH_RECALL_GLOBAL_SCENE:
        relay2_onOffUpdate(ch, ZCL_CMD_ONOFF_ON);
        break;
    case ZCL_CMD_ON_WITH_TIMED_OFF:
        if (!payload) return ZCL_STA_UNSUP_CLUSTER_COMMAND;
        timedOffProcess(ch, (const zcl_onoff_onWithTimeOffCmd_t *)payload);
        break;
    default:
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }
    return ZCL_STA_SUCCESS;
}
#endif
