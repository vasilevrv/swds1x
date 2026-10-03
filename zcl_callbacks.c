#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__)

#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "smart_plug.h"
#include "app_main.h"
#include "plug_control.h"

static ev_timer_event_t *identifyTimer;

static void processWriteRequest(zclIncoming_t *message)
{
    zclWriteCmd_t *write = (zclWriteCmd_t *)message->attrCmd;
    apsdeDataInd_t *indication = (apsdeDataInd_t *)message->msg;
    u16 clusterId = indication->indInfo.cluster_id;
    if ((indication->indInfo.dst_ep != SMART_PLUG_ENDPOINT) ||
        message->hdr.frmCtrl.bf.dir != ZCL_FRAME_CLIENT_SERVER_DIR) {
        return;
    }

    for (u8 i = 0; i < write->numAttr; i++) {
        u16 attrId = write->attrList[i].attrID;

        if ((clusterId == SMART_PLUG_CONFIG_CLUSTER_ID) &&
            (attrId == SMART_PLUG_ATTR_TELEMETRY_INTERVAL_MS)) {
            smartPlug_telemetryIntervalChanged();
        } else if ((clusterId == ZCL_CLUSTER_GEN_ON_OFF) &&
                   (attrId == ZCL_ATTRID_START_UP_ONOFF)) {
            zcl_onOffAttr_save();
        } else if ((clusterId == ZCL_CLUSTER_GEN_IDENTIFY) &&
                   (attrId == ZCL_ATTRID_IDENTIFY_TIME)) {
            smartPlug_zclIdentifyCmdHandler(SMART_PLUG_ENDPOINT, 0xFFFE,
                                            g_zcl_identifyAttrs.identifyTime);
        }
    }
}

void smartPlug_zclProcessIncomingMsg(zclIncoming_t *message)
{
#ifdef ZCL_WRITE
    if ((message->hdr.cmd == ZCL_CMD_WRITE) ||
        (message->hdr.cmd == ZCL_CMD_WRITE_NO_RSP) ||
        (message->hdr.cmd == ZCL_CMD_WRITE_UNDIVIDED)) {
        processWriteRequest(message);
    }
#else
    (void)message;
#endif
}

status_t smartPlug_basicCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload)
{
    (void)payload;

    if ((address->dstEp != SMART_PLUG_ENDPOINT) ||
        (address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR)) {
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    if (commandId != ZCL_CMD_BASIC_RESET_FAC_DEFAULT) {
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    smartPlug_onOffUpdate(ZCL_CMD_ONOFF_OFF);
    gSmartPlugCtx.factoryResetPending = TRUE;
    smartPlug_factoryReset();
    zb_factoryReset();
    return ZCL_STA_SUCCESS;
}

static s32 identifyTimerCb(void *arg)
{
    (void)arg;
    if (!g_zcl_identifyAttrs.identifyTime) {
        statusLed_blinkStop();
        identifyTimer = NULL;
        return -1;
    }
    g_zcl_identifyAttrs.identifyTime--;
    if (!g_zcl_identifyAttrs.identifyTime) {
        statusLed_blinkStop();
        identifyTimer = NULL;
        return -1;
    }
    return 0;
}

static void identifyStop(void)
{
    if (identifyTimer) {
        TL_ZB_TIMER_CANCEL(&identifyTimer);
    }
    statusLed_blinkStop();
}

void smartPlug_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 identifyTime)
{
    if (endpoint != SMART_PLUG_ENDPOINT || gSmartPlugCtx.factoryResetPending) {
        return;
    }
    (void)srcAddr;
    g_zcl_identifyAttrs.identifyTime = identifyTime;

    if (!identifyTime) {
        identifyStop();
        return;
    }

    identifyStop();
    g_zcl_identifyAttrs.identifyTime = identifyTime;
    statusLed_blinkStart(identifyTime, 500, 500);
    identifyTimer = TL_ZB_TIMER_SCHEDULE(identifyTimerCb, NULL, 1000);
    if (!identifyTimer) {
        g_zcl_identifyAttrs.identifyTime = 0;
        statusLed_blinkStop();
    }
}

static void triggerIdentifyEffect(const zcl_triggerEffect_t *effect)
{
    switch (effect->effectId) {
    case IDENTIFY_EFFECT_BLINK:
        statusLed_blinkStart(1, 500, 500);
        break;
    case IDENTIFY_EFFECT_BREATHE:
        statusLed_blinkStart(15, 300, 700);
        break;
    case IDENTIFY_EFFECT_OKAY:
        statusLed_blinkStart(2, 250, 250);
        break;
    case IDENTIFY_EFFECT_CHANNEL_CHANGE:
        statusLed_blinkStart(1, 500, 7500);
        break;
    case IDENTIFY_EFFECT_FINISH_EFFECT:
        statusLed_blinkStart(1, 300, 700);
        break;
    case IDENTIFY_EFFECT_STOP_EFFECT:
        identifyStop();
        g_zcl_identifyAttrs.identifyTime = 0;
        break;
    default:
        break;
    }
}

status_t smartPlug_identifyCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload)
{
    if ((address->dstEp != SMART_PLUG_ENDPOINT) ||
        (address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR)) {
        return ZCL_STA_SUCCESS;
    }

    if (commandId == ZCL_CMD_IDENTIFY) {
        smartPlug_zclIdentifyCmdHandler(address->dstEp, address->srcAddr,
                                        ((zcl_identifyCmd_t *)payload)->identifyTime);
        return ZCL_STA_SUCCESS;
    }
    if (commandId == ZCL_CMD_TRIGGER_EFFECT) {
        triggerIdentifyEffect((zcl_triggerEffect_t *)payload);
        return ZCL_STA_SUCCESS;
    }
    return ZCL_STA_UNSUP_CLUSTER_COMMAND;
}

#endif
