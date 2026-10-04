/********************************************************************************************************
 * @file    app_main.c
 *
 * @brief   Two-channel relay application entry point.
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

#if (__PROJECT_TL_RELAY_2CH__)

/**********************************************************************
 * INCLUDES
 */
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "app_main.h"
#include "relay_control.h"
#include "relay_2ch.h"
#include "board_io.h"
#include "app_ui.h"
#if ZBHCI_EN
#include "zbhci.h"
#endif

/**********************************************************************
 * LOCAL CONSTANTS
 */


/**********************************************************************
 * TYPEDEFS
 */


/**********************************************************************
 * GLOBAL VARIABLES
 */
app_ctx_t gRelayCtx;


#ifdef ZCL_OTA
extern ota_callBack_t relay2_otaCb;

//running code firmware information
ota_preamble_t relay2_otaInfo = {
    .fileVer          = FILE_VERSION,
    .imageType        = IMAGE_TYPE,
    .manufacturerCode = MANUFACTURER_CODE,
};
#endif

//Must declare the application call back function which used by ZDO layer
const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf,                 //start device cnf cb
    NULL,                               //reset cnf cb
    NULL,                               //device announce indication cb
    relay2_leaveIndHandler,          //leave ind cb
    relay2_leaveCnfHandler,          //leave cnf cb
    relay2_panidConflictIndicateHandler,//nwk panid conflict ind cb
    NULL,                               //permit join ind cb
    NULL,                               //nlme sync cnf cb
    NULL,                               //tc join ind cb
    NULL,                               //tc detects that the frame counter is near limit
    NULL,                               //end device negotiate cnf cb
    relay2_nwkStatusIndHandler,      //nwk status ind cb
};

/**
 *  @brief Definition for bdb commissioning setting
 */
bdb_commissionSetting_t g_bdbCommissionSetting = {
    .linkKey.tcLinkKey.keyType          = SS_GLOBAL_LINK_KEY,
    .linkKey.tcLinkKey.key              = (u8 *)tcLinkKeyCentralDefault,         //can use unique link key stored in NV
#if TEST_FOR_CERTIFICATION
    .linkKey.distributeLinkKey.keyType  = CERTIFICATION_KEY,
    .linkKey.distributeLinkKey.key      = (u8 *)linkKeyDistributedCertification, //use linkKeyDistributedCertification before testing
    .linkKey.touchLinkKey.keyType       = CERTIFICATION_KEY,
    .linkKey.touchLinkKey.key           = (u8 *)touchLinkKeyCertification,       //use touchLinkKeyCertification before testing
#else
    .linkKey.distributeLinkKey.keyType  = MASTER_KEY,
    .linkKey.distributeLinkKey.key      = (u8 *)linkKeyDistributedMaster,        //use linkKeyDistributedCertification before testing
    .linkKey.touchLinkKey.keyType       = MASTER_KEY,
    .linkKey.touchLinkKey.key           = (u8 *)touchLinkKeyMaster,              //use touchLinkKeyCertification before testing
#endif
#if TOUCHLINK_SUPPORT
    .touchlinkEnable                    = 1,                                     /* enable touch-link */
#else
    .touchlinkEnable                    = 0,                                     /* disable touch-link */
#endif
    .touchlinkChannel                   = DEFAULT_CHANNEL,                       /* touch-link default operation channel for target */
    .touchlinkLqiThreshold              = 0xA0,                                  /* threshold for touch-link scan req/resp command */
};

/**********************************************************************
 * LOCAL VARIABLES
 */

/**********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
 * @fn      stack_init
 *
 * @brief   This function initialize the ZigBee stack and related profile. If HA/ZLL profile is
 *          enabled in this application, related cluster should be registered here.
 *
 * @param   None
 *
 * @return  None
 */
void stack_init(void)
{
    /* Initialize ZB stack */
    zb_init();

    /* Register stack CB */
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

/*********************************************************************
 * @fn      user_app_init
 *
 * @brief   This function initialize the application(Endpoint) information for this node.
 *
 * @param   None
 *
 * @return  None
 */
void user_app_init(void)
{
    af_nodeDescManuCodeUpdate(MANUFACTURER_CODE);

    /* Initialize ZCL layer */
    /* Register Incoming ZCL Foundation command/response messages */
    zcl_init(relay2_zclProcessIncomingMsg);

    /* Register endPoint */
    for (u8 ch = 0; ch < RELAY_CHANNELS; ch++) {
        af_endpointRegister(ch + 1, (af_simple_descriptor_t *)&g_relay2SimpleDesc[ch], relay2_zclRxHandler, NULL);
    }
    /* Initialize or restore attributes, this must before 'zcl_register()' */
    zcl_relay2AttrsInit();
    zcl_reportingTabInit();

    /* Register ZCL specific cluster information */
    for (u8 ch = 0; ch < RELAY_CHANNELS; ch++) {
        zcl_register(ch + 1, g_relay2ClusterCount, (zcl_specClusterInfo_t *)g_relay2ClusterList[ch]);
    }

#if ZCL_OTA_SUPPORT
    /* Initialize OTA */
    ota_init(OTA_TYPE_CLIENT, (af_simple_descriptor_t *)&g_relay2SimpleDesc[0], &relay2_otaInfo, &relay2_otaCb);
#endif

}

void app_task(void)
{
    app_key_handler();
    if (gRelayCtx.factoryResetPending) {
        return; /* Do not recreate application NV data while the stack resets. */
    }
    relay2_networkTask();
    relay2_task();

    if (BDB_STATE_GET() == BDB_STATE_IDLE) {
        report_handler();
    }
}

static void relay2SysException(void)
{
    SYSTEM_RESET();
}

/*********************************************************************
 * @fn      user_init
 *
 * @brief   User level initialization code.
 *
 * @param   isRetention - if it is waking up with ram retention.
 *
 * @return  None
 */
void user_init(bool isRetention)
{
    (void)isRetention;

    /* Register the system exception handler. */
    sys_exceptHandlerRegister(relay2SysException);

    /* Initialize the status LED and keep the relay inactive during startup. */
    board_io_init();
    led_init();

    relay_init();

    /* Initialize Stack */
    stack_init();

    /* Initialize user application */
    user_app_init();

    app_buttons_init();

    /* Apply the restored relay state. */
    relay_restore();

    /* User's Task */
#if ZBHCI_EN
#if ZB_TEST_ENABLE
    zbhciInit(zbhciProcessIncomingSerialCmd);
#else
    zbhciInit(zbhciCmdHandler);
#endif
    ev_on_poll(EV_POLL_HCI, zbhciTask);
#endif
    ev_on_poll(EV_POLL_IDLE, app_task);

    /* Read the pre-install code from NV */
    if (bdb_preInstallCodeLoad(&gRelayCtx.tcLinkKey.keyType, gRelayCtx.tcLinkKey.key) == RET_OK) {
        g_bdbCommissionSetting.linkKey.tcLinkKey.keyType = gRelayCtx.tcLinkKey.keyType;
        g_bdbCommissionSetting.linkKey.tcLinkKey.key = gRelayCtx.tcLinkKey.key;
    }

    /* Set default reporting configuration */
    u8 reportableChange[2] = {0x00, 0x00};
    for (u8 ch = 0; ch < RELAY_CHANNELS; ch++) {
        bdb_defaultReportingCfg(ch + 1, HA_PROFILE_ID,
                                ZCL_CLUSTER_GEN_ON_OFF, ZCL_ATTRID_ONOFF,
                                0, 120, (u8 *)&reportableChange);
    }
    /* Initialize BDB */
    bdb_init((af_simple_descriptor_t *)&g_relay2SimpleDesc[0], &g_bdbCommissionSetting, &g_zbDemoBdbCb, 1);
}

#endif  /* __PROJECT_TL_RELAY_2CH__ */
