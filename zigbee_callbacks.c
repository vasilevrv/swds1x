/********************************************************************************************************
 * @file    zigbee_callbacks.c
 *
 * @brief   This is the source file for zb_appCb
 *
 * @author  Zigbee Group
 * @date    2021
 *
 * @par     Copyright (c) 2021, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Licensed under the Apache License, Version 2.0 (the "License");
 *          you may not use this file except in compliance with the License.
 *          You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 *          Unless required by applicable law or agreed to in writing, software
 *          distributed under the License is distributed on an "AS IS" BASIS,
 *          WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *          See the License for the specific language governing permissions and
 *          limitations under the License.
 *
 *******************************************************************************************************/
#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__)

/**********************************************************************
 * INCLUDES
 */
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "app_main.h"
#include "plug_control.h"

/**********************************************************************
 * TYPEDEFS
 */


/**********************************************************************
 * LOCAL FUNCTIONS
 */
void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork);
void zbdemo_bdbCommissioningCb(u8 status, void *arg);
void zbdemo_bdbIdentifyCb(u8 endpoint, u16 srcAddr, u16 identifyTime);

/**********************************************************************
 * GLOBAL VARIABLES
 */
bdb_appCb_t g_zbDemoBdbCb = {
    zbdemo_bdbInitCb,
    zbdemo_bdbCommissioningCb,
    zbdemo_bdbIdentifyCb,
    NULL
};

#ifdef ZCL_OTA
ota_callBack_t smartPlug_otaCb = {
    smartPlug_otaProcessMsgHandler,
};
#endif

/**********************************************************************
 * LOCAL VARIABLES
 */
static ev_timer_event_t *steerTimerEvt = NULL;
static ev_timer_event_t *rejoinBackoffTimerEvt = NULL;
static bool steerRetry;
static bool rejoinRetry;
static u32 retryStarted;

/**********************************************************************
 * FUNCTIONS
 */
static s32 smartPlug_bdbNetworkSteerStart(void *arg)
{
    (void)arg;
    steerTimerEvt = NULL;
    if (zb_isDeviceFactoryNew()) {
        bdb_networkSteerStart();
    }

    return -1;
}

void smartPlug_classicJoinStart(void)
{
    printf("start classic Join\n");

    u16 jitter = 1 + zb_random() % 0x0fff;

    if (!steerTimerEvt) {
        steerTimerEvt = TL_ZB_TIMER_SCHEDULE(smartPlug_bdbNetworkSteerStart, NULL, jitter);
        steerRetry = (steerTimerEvt == NULL);
    }
}

static s32 smartPlug_rejoinBackoff(void *arg)
{
    (void)arg;
    static bool rejoinMode = REJOIN_SECURITY;

    if (zb_isDeviceFactoryNew() || zb_isDeviceJoinedNwk()) {
        rejoinBackoffTimerEvt = NULL;
        return -1;
    }
    if (BDB_STATE_GET() != BDB_STATE_IDLE) {
        return 0;
    }

    printf("rejoin mode = %d\n", rejoinMode);

    zb_rejoinSecModeSet(rejoinMode);
    zb_rejoinReq(zb_apsChannelMaskGet(), g_bdbAttrs.scanDuration);

    if (!ss_securityModeIsDistributed()) {
        rejoinMode = !rejoinMode;
    }

    return 0;
}

void smartPlug_rejoinTimerStart(void)
{
    printf("start rejoin timer\n");

    if (!rejoinBackoffTimerEvt) {
        rejoinBackoffTimerEvt = TL_ZB_TIMER_SCHEDULE(smartPlug_rejoinBackoff, NULL, 60 * 1000);
        rejoinRetry = (rejoinBackoffTimerEvt == NULL);
    }
}

void smartPlug_networkTask(void)
{
    if (!clock_time_exceed(retryStarted, 1000000UL)) {
        return;
    }
    retryStarted = clock_time();
    if (steerRetry) smartPlug_classicJoinStart();
    if (rejoinRetry) smartPlug_rejoinTimerStart();
}

/*********************************************************************
 * @fn      zbdemo_bdbInitCb
 *
 * @brief   application callback for bdb initiation
 *
 * @param   status - the status of bdb init BDB_INIT_STATUS_SUCCESS or BDB_INIT_STATUS_FAILURE
 *
 * @param   joinedNetwork  - 1: node is on a network, 0: node isn't on a network
 *
 * @return  None
 */
void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork)
{
    printf("bdbInitCb: sta = %x, joined = %x\n", status, joinedNetwork);

    if (status == BDB_INIT_STATUS_SUCCESS) {
        if (joinedNetwork) {
#ifdef ZCL_OTA
            ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif
        } else {
#if (!ZBHCI_EN)
            smartPlug_classicJoinStart();
#endif
        }
    }
}

/*********************************************************************
 * @fn      zbdemo_bdbCommissioningCb
 *
 * @brief   application callback for bdb commissioning
 *
 * @param   status - the status of bdb commissioning
 *
 * @param   arg
 *
 * @return  None
 */
void zbdemo_bdbCommissioningCb(u8 status, void *arg)
{
    (void)arg;
    printf("bdbCommCb: sta = %x\n", status);

    switch (status) {
    case BDB_COMMISSION_STA_SUCCESS:
        statusLed_blinkStart(2, 200, 200);

#ifdef ZCL_OTA
        ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif

        break;
    case BDB_COMMISSION_STA_IN_PROGRESS:
        break;
    case BDB_COMMISSION_STA_NOT_AA_CAPABLE:
        break;
    case BDB_COMMISSION_STA_NO_NETWORK:
    case BDB_COMMISSION_STA_TCLK_EX_FAILURE:
    case BDB_COMMISSION_STA_TARGET_FAILURE:
        smartPlug_classicJoinStart();
        break;
    case BDB_COMMISSION_STA_FORMATION_FAILURE:
        break;
    case BDB_COMMISSION_STA_NO_IDENTIFY_QUERY_RESPONSE:
        break;
    case BDB_COMMISSION_STA_BINDING_TABLE_FULL:
        break;
    case BDB_COMMISSION_STA_NO_SCAN_RESPONSE:
        break;
    case BDB_COMMISSION_STA_NOT_PERMITTED:
        break;
    case BDB_COMMISSION_STA_REJOIN_FAILURE:
    case BDB_LOSS_CONNECTIVITY:
        smartPlug_rejoinTimerStart();
        break;
    case BDB_COMMISSION_STA_FORMATION_DONE:
#ifndef ZBHCI_EN
        tl_zbMacChannelSet(DEFAULT_CHANNEL); //set default channel
#endif
        break;
    default:
        break;
    }
}

void zbdemo_bdbIdentifyCb(u8 endpoint, u16 srcAddr, u16 identifyTime)
{
    smartPlug_zclIdentifyCmdHandler(endpoint, srcAddr, identifyTime);
}

#ifdef ZCL_OTA
void smartPlug_otaProcessMsgHandler(u8 evt, u8 status)
{
    if (evt == OTA_EVT_COMPLETE) {
        if (status == ZCL_STA_SUCCESS) {
            ota_mcuReboot();
        } else {
            ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
        }
    }
}
#endif

static s32 smartPlug_softReset(void *arg)
{
    (void)arg;
    SYSTEM_RESET();

    return -1;
}

/*********************************************************************
 * @fn      smartPlug_leaveCnfHandler
 *
 * @brief   Handler for ZDO Leave Confirm message.
 *
 * @param   pRsp - parameter of leave confirm
 *
 * @return  None
 */
void smartPlug_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf)
{
    if (pLeaveCnf->status == SUCCESS) {
        statusLed_blinkStart(3, 200, 200);

        //waiting blink over
        if (!TL_ZB_TIMER_SCHEDULE(smartPlug_softReset, NULL, 2 * 1000)) {
            smartPlug_softReset(NULL);
        }
    }
}

/*********************************************************************
 * @fn      smartPlug_leaveIndHandler
 *
 * @brief   Handler for ZDO leave indication message.
 *
 * @param   pInd - parameter of leave indication
 *
 * @return  None
 */
void smartPlug_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    (void)pLeaveInd;
}

bool smartPlug_panidConflictIndicateHandler(u16 newPanId)
{
    (void)newPanId;
    return FALSE;
}

/*********************************************************************
 * @fn      smartPlug_nwkStatusIndHandler
 *
 * @brief   Handler for NWK status indication message.
 *
 * @param   pInd - parameter of NWK status indication
 *
 * @return  None
 */
void smartPlug_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd)
{
    printf("nwkStatusIndHandler: addr = %x, status = %x\n", pNwkStatusInd->shortAddr, pNwkStatusInd->status);

    if (pNwkStatusInd->status == NWK_COMMAND_STATUS_BAD_KEY_SEQUENCE_NUMBER) {
        if (!ss_securityModeIsDistributed()) {
            zb_rejoinSecModeSet(REJOIN_INSECURITY);
        } else {
            zb_rejoinSecModeSet(REJOIN_SECURITY);
        }
        zb_rejoinReq(zb_apsChannelMaskGet(), g_bdbAttrs.scanDuration);
    }
}

#endif  /* __PROJECT_TL_SMART_PLUG__ */
