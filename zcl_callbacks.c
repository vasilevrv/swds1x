#include "app_cfg.h"
#if (__PROJECT_TL_RELAY_2CH__)
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "relay_2ch.h"
#include "app_main.h"
#include "relay_control.h"

static ev_timer_event_t *identifyTimer;
static u8 identifyChannel;

void relay2_identifyStop(void)
{
    if (identifyTimer) TL_ZB_TIMER_CANCEL(&identifyTimer);
    for (u8 ch = 0; ch < 2; ch++) g_zcl_identifyAttrs[ch].identifyTime = 0;
    statusLed_blinkStop();
}

static s32 identifyTimerCb(void *arg)
{
    (void)arg;
    u16 *time = &g_zcl_identifyAttrs[identifyChannel].identifyTime;
    if (*time) --*time;
    if (!*time) {
        identifyTimer = NULL;
        statusLed_blinkStop();
        return -1;
    }
    return 0;
}

void relay2_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 time)
{
    int ch = relay2_channelFromEndpoint(endpoint);
    if (ch < 0 || gRelayCtx.factoryResetPending) return;
    (void)srcAddr;
    relay2_identifyStop();
    if (!time) return;
    identifyChannel = ch;
    g_zcl_identifyAttrs[ch].identifyTime = time;
    statusLed_blinkStart(time, 500, 500);
    identifyTimer = TL_ZB_TIMER_SCHEDULE(identifyTimerCb, NULL, 1000);
    if (!identifyTimer) relay2_identifyStop();
}

void relay2_zclProcessIncomingMsg(zclIncoming_t *message)
{
#ifdef ZCL_WRITE
    if (message->hdr.cmd != ZCL_CMD_WRITE && message->hdr.cmd != ZCL_CMD_WRITE_NO_RSP &&
        message->hdr.cmd != ZCL_CMD_WRITE_UNDIVIDED) return;
    apsdeDataInd_t *ind = (apsdeDataInd_t *)message->msg;
    int ch = relay2_channelFromEndpoint(ind->indInfo.dst_ep);
    if (ch < 0 || message->hdr.frmCtrl.bf.dir != ZCL_FRAME_CLIENT_SERVER_DIR) return;
    zclWriteCmd_t *write = (zclWriteCmd_t *)message->attrCmd;
    for (u8 i = 0; i < write->numAttr; i++) {
        u16 attr = write->attrList[i].attrID;
        if (ind->indInfo.cluster_id == RELAY_2CH_CONFIG_CLUSTER_ID &&
            (attr == RELAY_ATTR_BUTTON_LINKED || attr == RELAY_ATTR_DOUBLE_CLICK)) relay2_configChanged(ch);
        else if (ind->indInfo.cluster_id == ZCL_CLUSTER_GEN_ON_OFF &&
                 attr == ZCL_ATTRID_START_UP_ONOFF) relay2_saveState();
        else if (ind->indInfo.cluster_id == ZCL_CLUSTER_GEN_ON_OFF &&
                 (attr == ZCL_ATTRID_ON_TIME || attr == ZCL_ATTRID_OFF_WAIT_TIME))
            relay2_onOffTimerSync(ch);
        else if (ind->indInfo.cluster_id == ZCL_CLUSTER_GEN_IDENTIFY &&
                 attr == ZCL_ATTRID_IDENTIFY_TIME)
            relay2_zclIdentifyCmdHandler(ch + 1, 0xFFFE, g_zcl_identifyAttrs[ch].identifyTime);
    }
#else
    (void)message;
#endif
}

status_t relay2_basicCb(zclIncomingAddrInfo_t *address, u8 cmd, void *payload)
{
    (void)payload;
    if (relay2_channelFromEndpoint(address->dstEp) < 0 ||
        address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR || cmd != ZCL_CMD_BASIC_RESET_FAC_DEFAULT)
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    relay2_factoryReset();
    return ZCL_STA_SUCCESS;
}

status_t relay2_identifyCb(zclIncomingAddrInfo_t *address, u8 cmd, void *payload)
{
    if (relay2_channelFromEndpoint(address->dstEp) < 0 ||
        address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR || gRelayCtx.factoryResetPending)
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    if (cmd == ZCL_CMD_IDENTIFY) {
        relay2_zclIdentifyCmdHandler(address->dstEp, address->srcAddr, ((zcl_identifyCmd_t *)payload)->identifyTime);
    } else if (cmd == ZCL_CMD_TRIGGER_EFFECT) {
        relay2_identifyStop();
        switch (((zcl_triggerEffect_t *)payload)->effectId) {
        case IDENTIFY_EFFECT_BLINK: statusLed_blinkStart(1, 500, 500); break;
        case IDENTIFY_EFFECT_BREATHE: statusLed_blinkStart(15, 300, 700); break;
        case IDENTIFY_EFFECT_OKAY: statusLed_blinkStart(2, 250, 250); break;
        case IDENTIFY_EFFECT_CHANNEL_CHANGE: statusLed_blinkStart(1, 500, 7500); break;
        case IDENTIFY_EFFECT_FINISH_EFFECT: statusLed_blinkStart(1, 300, 700); break;
        case IDENTIFY_EFFECT_STOP_EFFECT: break;
        default: break;
        }
    } else return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    return ZCL_STA_SUCCESS;
}
#endif
