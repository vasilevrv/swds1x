/********************************************************************************************************
 * @file    zcl_on_off.c
 *
 * @brief   Relay commands for the On/Off cluster.
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
#include "app_main.h"
#include "plug_control.h"
#include "smart_plug.h"

/**********************************************************************
 * LOCAL CONSTANTS
 */
#define ZCL_ONOFF_TIMER_INTERVAL        100 //the timer interval to change the offWaitTime/onTime attribute of the ONOFF cluster

/**********************************************************************
 * LOCAL VARIABLES
 */
static ev_timer_event_t *onWithTimedOffTimerEvt = NULL;

/**********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
 * @fn      smartPlug_onOffInit
 *
 * @brief
 *
 * @param   None
 *
 * @return  None
 */
void smartPlug_onOffInit(void)
{
    zcl_onOffAttr_t *pOnOff = zcl_onoffAttrGet();
    /* zcl_onOff_register() has already applied StartUpOnOff exactly once. */
    relay_set(pOnOff->onOff);
    zcl_onOffAttr_save();
}

/*********************************************************************
 * @fn      smartPlug_onOffUpdate
 *
 * @brief
 *
 * @param   ZCL_CMD_ONOFF_ON / ZCL_ONOFF_STATUS_OFF / ZCL_CMD_ONOFF_TOGGLE
 *
 * @return  None
 */
void smartPlug_onOffUpdate(u8 cmd)
{
    zcl_onOffAttr_t *pOnOff = zcl_onoffAttrGet();
    bool previous = pOnOff->onOff;
    bool onOff = ZCL_ONOFF_STATUS_ON;

    if (gSmartPlugCtx.factoryResetPending && (cmd != ZCL_CMD_ONOFF_OFF)) {
        return;
    }

    if (cmd == ZCL_CMD_ONOFF_ON) {
        onOff = ZCL_ONOFF_STATUS_ON;
    } else if (cmd == ZCL_CMD_ONOFF_OFF) {
        onOff = ZCL_ONOFF_STATUS_OFF;
    } else if (cmd == ZCL_CMD_ONOFF_TOGGLE) {
        onOff = (pOnOff->onOff == ZCL_ONOFF_STATUS_ON) ? ZCL_ONOFF_STATUS_OFF
                                                       : ZCL_ONOFF_STATUS_ON;
    } else {
        return;
    }

    //update attributes
    if (onOff == ZCL_ONOFF_STATUS_ON) {
        pOnOff->globalSceneControl = TRUE;
        pOnOff->onOff = ZCL_ONOFF_STATUS_ON;
        if (pOnOff->onTime == 0) {
            pOnOff->offWaitTime = 0;
        }
    } else {
        pOnOff->onOff = ZCL_ONOFF_STATUS_OFF;
        pOnOff->onTime = 0;
    }

#ifdef ZCL_SCENE
    zcl_sceneAttr_t *pScene = zcl_sceneAttrGet();
    pScene->sceneValid = 0;
#endif

    relay_set(pOnOff->onOff);
    if (previous != pOnOff->onOff) {
        zcl_onOffAttr_save();
        smartPlug_onRelayStateChanged(pOnOff->onOff);
    }
}

/*********************************************************************
 * @fn      smartPlug_onWithTimedOffTimerCb
 *
 * @brief   timer event to process the ON_WITH_TIMED_OFF command
 *
 * @param   arg
 *
 * @return  0: timer continue on; -1: timer will be canceled
 */
static s32 smartPlug_onWithTimedOffTimerCb(void *arg)
{
    (void)arg;
    zcl_onOffAttr_t *pOnOff = zcl_onoffAttrGet();

    if ((pOnOff->onOff == ZCL_ONOFF_STATUS_ON) && pOnOff->onTime &&
        (pOnOff->onTime != 0xFFFF)) {
        pOnOff->onTime--;
        if (pOnOff->onTime <= 0) {
            smartPlug_onOffUpdate(ZCL_CMD_ONOFF_OFF);
        }
    } else if ((pOnOff->onOff == ZCL_ONOFF_STATUS_OFF) && pOnOff->offWaitTime &&
               (pOnOff->offWaitTime != 0xFFFF)) {
        pOnOff->offWaitTime--;
        if (pOnOff->offWaitTime <= 0) {
            onWithTimedOffTimerEvt = NULL;
            return -1;
        }
    }

    if (pOnOff->onTime || pOnOff->offWaitTime) {
        return 0;
    } else {
        onWithTimedOffTimerEvt = NULL;
        return -1;
    }
}

/*********************************************************************
 * @fn      smartPlug_onWithTimedOffTimerStart
 *
 * @brief   start the onWithTimedOff timer
 *
 * @param
 *
 * @return
 */
static void smartPlug_onWithTimedOffTimerStart(void)
{
    if (onWithTimedOffTimerEvt) {
        TL_ZB_TIMER_CANCEL(&onWithTimedOffTimerEvt);
    }
    onWithTimedOffTimerEvt = TL_ZB_TIMER_SCHEDULE(smartPlug_onWithTimedOffTimerCb, NULL, ZCL_ONOFF_TIMER_INTERVAL);
    if (!onWithTimedOffTimerEvt) {
        smartPlug_onOffUpdate(ZCL_CMD_ONOFF_OFF);
        zcl_onoffAttrGet()->offWaitTime = 0;
    }
}

/*********************************************************************
 * @fn      smartPlug_onWithTimedOffProcess
 *
 * @brief
 *
 * @param   cmd
 *
 * @return  None
 */
static void smartPlug_onWithTimedOffProcess(zcl_onoff_onWithTimeOffCmd_t *cmd)
{
    zcl_onOffAttr_t *pOnOff = zcl_onoffAttrGet();

    if (cmd->onOffCtrl.bits.acceptOnlyWhenOn && (pOnOff->onOff == ZCL_ONOFF_STATUS_OFF)) {
        return;
    }

    if (pOnOff->offWaitTime && (pOnOff->onOff == ZCL_ONOFF_STATUS_OFF)) {
        pOnOff->offWaitTime = min2(pOnOff->offWaitTime, cmd->offWaitTime);
    } else {
        pOnOff->onTime = max2(pOnOff->onTime, cmd->onTime);
        pOnOff->offWaitTime = cmd->offWaitTime;
        smartPlug_onOffUpdate(ZCL_CMD_ONOFF_ON);
    }

    if ((pOnOff->onTime && pOnOff->onTime < 0xFFFF) ||
        (pOnOff->offWaitTime && pOnOff->offWaitTime < 0xFFFF)) {
            smartPlug_onWithTimedOffTimerStart();
    }
}

/*********************************************************************
 * @fn      smartPlug_offWithEffectProcess
 *
 * @brief
 *
 * @param   cmd
 *
 * @return  None
 */
static void smartPlug_offWithEffectProcess(zcl_onoff_offWithEffectCmd_t *cmd)
{
    zcl_onOffAttr_t *pOnOff = zcl_onoffAttrGet();
    (void)cmd;
    pOnOff->globalSceneControl = FALSE;

    smartPlug_onOffUpdate(ZCL_CMD_ONOFF_OFF);
}

/*********************************************************************
 * @fn      smartPlug_onWithRecallGlobalSceneProcess
 *
 * @brief
 *
 * @param   cmd
 *
 * @return  None
 */
static void smartPlug_onWithRecallGlobalSceneProcess(void)
{
    zcl_onOffAttr_t *pOnOff = zcl_onoffAttrGet();
    pOnOff->globalSceneControl = TRUE;
    smartPlug_onOffUpdate(ZCL_CMD_ONOFF_ON);
}

/*********************************************************************
 * @fn      smartPlug_onOffCb
 *
 * @brief   Handler for ZCL ONOFF command. This function will set ONOFF attribute first.
 *
 * @param	pAddrInfo
 * @param   cmdId - onoff cluster command id
 * @param	cmdPayload
 *
 * @return  status_t
 */
status_t smartPlug_onOffCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    status_t status = ZCL_STA_SUCCESS;

    if ((pAddrInfo->dstEp == SMART_PLUG_ENDPOINT) &&
        (pAddrInfo->dirCluster == ZCL_FRAME_CLIENT_SERVER_DIR)) {
        switch (cmdId) {
        case ZCL_CMD_ONOFF_ON:
        case ZCL_CMD_ONOFF_OFF:
        case ZCL_CMD_ONOFF_TOGGLE:
            smartPlug_onOffUpdate(cmdId);
            break;
        case ZCL_CMD_OFF_WITH_EFFECT:
            smartPlug_offWithEffectProcess((zcl_onoff_offWithEffectCmd_t *)cmdPayload);
            break;
        case ZCL_CMD_ON_WITH_RECALL_GLOBAL_SCENE:
            smartPlug_onWithRecallGlobalSceneProcess();
            break;
        case ZCL_CMD_ON_WITH_TIMED_OFF:
            smartPlug_onWithTimedOffProcess((zcl_onoff_onWithTimeOffCmd_t *)cmdPayload);
            break;
        default:
            status = ZCL_STA_UNSUP_CLUSTER_COMMAND;
            break;
        }
    }

    return status;
}

#endif  /* __PROJECT_TL_SMART_PLUG__ */
