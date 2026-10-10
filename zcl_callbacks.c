#include "app_cfg.h"
#if (__PROJECT_TL_SWDS1X__)
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "app_main.h"
#include "sensor_control.h"
#include "app_ui.h"

static ev_timer_event_t *identifyTimer;

void sensor_identifyStop(void)
{
    if (identifyTimer) TL_ZB_TIMER_CANCEL(&identifyTimer);
    g_zcl_identifyAttrs.identifyTime = 0;
    statusLed_blinkStop();
}

static s32 identifyTimerCb(void *arg)
{
    (void)arg;
    u16 *time = &g_zcl_identifyAttrs.identifyTime;
    if (*time) --*time;
    if (!*time) {
        identifyTimer = NULL;
        statusLed_blinkStop();
        return -1;
    }
    return 0;
}

void sensor_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 time)
{
    if (endpoint != 1 || gSensorCtx.factoryResetPending) return;
    (void)srcAddr;
    sensor_identifyStop();
    if (!time) return;
    g_zcl_identifyAttrs.identifyTime = time;
    statusLed_blinkStart(time, 500, 500);
    identifyTimer = TL_ZB_TIMER_SCHEDULE(identifyTimerCb, NULL, 1000);
    if (!identifyTimer) sensor_identifyStop();
}

status_t sensor_basicCb(zclIncomingAddrInfo_t *address, u8 cmd, void *payload)
{
    (void)payload;
    if (address->dstEp != 1 ||
        address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR || cmd != ZCL_CMD_BASIC_RESET_FAC_DEFAULT)
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    sensor_factoryReset();
    return ZCL_STA_SUCCESS;
}

status_t sensor_identifyCb(zclIncomingAddrInfo_t *address, u8 cmd, void *payload)
{
    if (address->dstEp != 1 ||
        address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR || gSensorCtx.factoryResetPending)
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    if (cmd == ZCL_CMD_IDENTIFY) {
        sensor_zclIdentifyCmdHandler(address->dstEp, address->srcAddr, ((zcl_identifyCmd_t *)payload)->identifyTime);
    } else if (cmd == ZCL_CMD_TRIGGER_EFFECT) {
        sensor_identifyStop();
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

/* IAS enrollment state is saved only when its configuration changes, never on
 * contact transitions. Failed NV operations are retried while the device polls. */
static bool iasDirty, enrollNeeded;
static u32 iasAttempt;
static u16 cieShort;
static u8 cieEndpoint = 1;
static addrExt_t previousCie;
void sensor_iasInit(void)
{
    ZB_IEEE_ADDR_COPY(previousCie, g_zcl_iasZoneAttrs.iasCieAddr);
}
void sensor_zclProcessIncomingMsg(zclIncoming_t *message)
{
    if (gSensorCtx.factoryResetPending ||
        message->hdr.frmCtrl.bf.dir != ZCL_FRAME_CLIENT_SERVER_DIR) return;
    if (message->hdr.cmd != ZCL_CMD_WRITE && message->hdr.cmd != ZCL_CMD_WRITE_NO_RSP &&
        message->hdr.cmd != ZCL_CMD_WRITE_UNDIVIDED) return;
    apsdeDataInd_t *ind = (apsdeDataInd_t *)message->msg;
    if (ind->indInfo.dst_ep != 1) return;
    zclWriteCmd_t *write = (zclWriteCmd_t *)message->attrCmd;
    for (u8 i = 0; i < write->numAttr; ++i) {
        if (ind->indInfo.cluster_id == ZCL_CLUSTER_SS_IAS_ZONE &&
            write->attrList[i].attrID == ZCL_ATTRID_IAS_CIE_ADDR) {
            if (!ZB_IEEE_ADDR_CMP(previousCie, g_zcl_iasZoneAttrs.iasCieAddr)) {
                g_zcl_iasZoneAttrs.zoneState = ZONE_STATE_NOT_ENROLLED;
                g_zcl_iasZoneAttrs.zoneId = ZCL_ZONE_ID_INVALID;
                ZB_IEEE_ADDR_COPY(previousCie, g_zcl_iasZoneAttrs.iasCieAddr);
            }
            cieShort = ind->indInfo.src_short_addr;
            cieEndpoint = ind->indInfo.src_ep;
            enrollNeeded = TRUE;
            iasDirty = TRUE;
        } else if (ind->indInfo.cluster_id == ZCL_CLUSTER_GEN_IDENTIFY &&
                   write->attrList[i].attrID == ZCL_ATTRID_IDENTIFY_TIME) {
            sensor_zclIdentifyCmdHandler(1, 0, g_zcl_identifyAttrs.identifyTime);
        }
    }
}
static void cieAddressResponse(void *arg)
{
    zdo_zdpDataInd_t *ind = arg;
    if (gSensorCtx.factoryResetPending || !ind || ind->status != SUCCESS ||
        !ind->zpdu || ind->length < 12) return;
    zdo_nwk_addr_resp_t *rsp = (zdo_nwk_addr_resp_t *)ind->zpdu;
    if (rsp->status == SUCCESS &&
        ZB_IEEE_ADDR_CMP(rsp->ieee_addr_remote, g_zcl_iasZoneAttrs.iasCieAddr)) {
        cieShort = rsp->nwk_addr_remote;
        enrollNeeded = TRUE;
    }
}
void sensor_iasTask(void)
{
    u32 now = app_nowMs();
    if ((u32)(now - iasAttempt) < 1000U) return;
    iasAttempt = now;
    if (iasDirty && sensor_iasSave() == NV_SUCC) iasDirty = FALSE;
    if (!zb_isDeviceJoinedNwk() || g_zcl_iasZoneAttrs.zoneState == ZONE_STATE_ENROLLED ||
        ZB_IEEE_ADDR_IS_ZERO(g_zcl_iasZoneAttrs.iasCieAddr) ||
        ZB_IEEE_ADDR_IS_INVALID(g_zcl_iasZoneAttrs.iasCieAddr)) return;
    /* Recovered partial enrollment can resolve the CIE from its IEEE address. */
    if (!enrollNeeded && zb_getNwkAddrByExtAddr(g_zcl_iasZoneAttrs.iasCieAddr, &cieShort) != SUCCESS) {
        zdo_nwk_addr_req_t request = {0};
        u8 sequence = 0;
        ZB_IEEE_ADDR_COPY(request.ieee_addr_interest, g_zcl_iasZoneAttrs.iasCieAddr);
        request.req_type = ZDO_ADDR_REQ_SINGLE_RESP;
        zb_zdoNwkAddrReq(NWK_BROADCAST_ROUTER_COORDINATOR, &request, &sequence, cieAddressResponse);
        return;
    }
    epInfo_t dst = {0};
    dst.profileId = HA_PROFILE_ID;
    dst.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dst.dstAddr.shortAddr = cieShort;
    dst.dstEp = cieEndpoint;
    zoneEnrollReq_t req = {0};
    req.zoneType = ZONE_TYPE_CONTACT_SWITCH;
    req.manufacturerCode = MANUFACTURER_CODE;
    zcl_iasZone_enrollReqCmd(1, &dst, TRUE, &req);
}
status_t sensor_iasCb(zclIncomingAddrInfo_t *address, u8 cmd, void *payload)
{
    if (address->dstEp != 1 || address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR ||
        cmd != ZCL_CMD_ZONE_ENROLL_RSP || gSensorCtx.factoryResetPending)
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    zoneEnrollRsp_t *rsp = payload;
    addrExt_t source;
    if (rsp->code != ZONE_ENROLL_SUCC || rsp->zoneId == ZCL_ZONE_ID_INVALID ||
        zb_getExtAddrByNwkAddr(address->srcAddr, source) != SUCCESS ||
        !ZB_IEEE_ADDR_CMP(source, g_zcl_iasZoneAttrs.iasCieAddr)) return ZCL_STA_FAILURE;
    g_zcl_iasZoneAttrs.zoneState = ZONE_STATE_ENROLLED;
    g_zcl_iasZoneAttrs.zoneId = rsp->zoneId;
    ZB_IEEE_ADDR_COPY(previousCie, g_zcl_iasZoneAttrs.iasCieAddr);
    iasDirty = TRUE;
    enrollNeeded = FALSE;
    sensor_markPending();
    return ZCL_STA_SUCCESS;
}

#endif /* __PROJECT_TL_SWDS1X__ */
