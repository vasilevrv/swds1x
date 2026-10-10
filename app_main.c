/********************************************************************************************************
 * @file    app_main.c
 *
 * @brief   Door/window sensor application entry point.
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

#if (__PROJECT_TL_SWDS1X__)

/**********************************************************************
 * INCLUDES
 */
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "app_main.h"
#include "sensor_control.h"
#include "battery.h"
#include "sensor_engine.h"
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
app_ctx_t gSensorCtx;

#ifdef ZCL_OTA
extern ota_callBack_t sensor_otaCb;

//running code firmware information
ota_preamble_t sensor_otaInfo = {
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
    sensor_leaveIndHandler,          //leave ind cb
    sensor_leaveCnfHandler,          //leave cnf cb
    sensor_panidConflictIndicateHandler,//nwk panid conflict ind cb
    NULL,                               //permit join ind cb
    NULL,                               //nlme sync cnf cb
    NULL,                               //tc join ind cb
    NULL,                               //tc detects that the frame counter is near limit
    NULL,                               //end device negotiate cnf cb
    sensor_nwkStatusIndHandler,      //nwk status ind cb
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
    zcl_init(sensor_zclProcessIncomingMsg);

    af_powerDescPowerModeUpdate(POWER_MODE_RECEIVER_COMES_PERIODICALLY);
    af_endpointRegister(1, (af_simple_descriptor_t *)&sensor_simpleDesc, sensor_zclRxHandler, NULL);
    sensor_attrsInit();
    zcl_reportingTabInit();
    zcl_register(1, 4, (zcl_specClusterInfo_t *)sensor_clusterList);
#if ZCL_OTA_SUPPORT
    /* Initialize OTA */
    ota_init(OTA_TYPE_CLIENT, (af_simple_descriptor_t *)&sensor_simpleDesc, &sensor_otaInfo, &sensor_otaCb);
#endif

}

void app_task(void)
{
    app_key_handler();
    if (gSensorCtx.factoryResetPending) return;
    sensor_batteryTask();
    sensor_networkTask();
    sensor_iasTask();
    sensor_notifyTask();
    if (BDB_STATE_GET() == BDB_STATE_IDLE) report_handler();
    app_sleep();
}

static void sensorSysException(void)
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
    board_io_init();
    if (isRetention) {
        sensor_ledRestore();
        app_clockResume();
        mac_phyReconfig();
        return;
    }

    /* Register the system exception handler. */
    sys_exceptHandlerRegister(sensorSysException);

    /* Initialize the status LED and keep the LED inactive during startup. */
    led_init();
    /* Initialize Stack */
    stack_init();

    /* Initialize user application */
    user_app_init();

    app_buttons_init();
    sensor_batteryInit();


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
    if (bdb_preInstallCodeLoad(&gSensorCtx.tcLinkKey.keyType, gSensorCtx.tcLinkKey.key) == RET_OK) {
        g_bdbCommissionSetting.linkKey.tcLinkKey.keyType = gSensorCtx.tcLinkKey.keyType;
        g_bdbCommissionSetting.linkKey.tcLinkKey.key = gSensorCtx.tcLinkKey.key;
    }

    /* Initialize BDB */
    u8 repower = drv_pm_deepSleep_flag_get() ? 0 : 1;
    bdb_init((af_simple_descriptor_t *)&sensor_simpleDesc, &g_bdbCommissionSetting, &g_zbDemoBdbCb, repower);
}

#endif  /* __PROJECT_TL_SWDS1X__ */
