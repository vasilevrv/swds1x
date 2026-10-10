/********************************************************************************************************
 * @file    sampleSensorEpCfg.c
 *
 * @brief   This is the source file for sampleSensorEpCfg
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
#include "tl_common.h"
#include "zcl_include.h"
#include "app_main.h"
#include "battery.h"
#define ZCL_BASIC_MFG_NAME {13,'S','a','l','u',' ','W','o','r','k','s','h','o','p'}
#define ZCL_BASIC_MODEL_ID {6,'s','w','d','s','1','x'}
#define ZCL_BASIC_SW_BUILD_ID {10,'2','0','2','6','1','0','1','0','0','6'}
static const u16 inClusters[] = {ZCL_CLUSTER_GEN_BASIC, ZCL_CLUSTER_GEN_IDENTIFY, ZCL_CLUSTER_GEN_POWER_CFG, ZCL_CLUSTER_SS_IAS_ZONE};
static const u16 outClusters[] = {ZCL_CLUSTER_OTA};
const af_simple_descriptor_t sensor_simpleDesc = {
    HA_PROFILE_ID, HA_DEV_IAS_ZONE, 1, 1, 0, 4, 1, (u16 *)inClusters, (u16 *)outClusters
};
zcl_basicAttr_t g_zcl_basicAttrs = {
    .zclVersion = 0x03,
    .appVersion = 0x00,
    .stackVersion = 0x02,
    .hwVersion = 0x00,
    .manuName = ZCL_BASIC_MFG_NAME,
    .modelId = ZCL_BASIC_MODEL_ID,
    .powerSource = POWER_SOURCE_BATTERY,
    .swBuildId = ZCL_BASIC_SW_BUILD_ID,
    .deviceEnable = TRUE,
};

static const zclAttrInfo_t basicAttrTable[] = {
    {ZCL_ATTRID_BASIC_ZCL_VER, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_basicAttrs.zclVersion},
    {ZCL_ATTRID_BASIC_APP_VER, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_basicAttrs.appVersion},
    {ZCL_ATTRID_BASIC_STACK_VER, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_basicAttrs.stackVersion},
    {ZCL_ATTRID_BASIC_HW_VER, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_basicAttrs.hwVersion},
    {ZCL_ATTRID_BASIC_MFR_NAME, ZCL_DATA_TYPE_CHAR_STR, ACCESS_CONTROL_READ, g_zcl_basicAttrs.manuName},
    {ZCL_ATTRID_BASIC_MODEL_ID, ZCL_DATA_TYPE_CHAR_STR, ACCESS_CONTROL_READ, g_zcl_basicAttrs.modelId},
    {ZCL_ATTRID_BASIC_POWER_SOURCE, ZCL_DATA_TYPE_ENUM8, ACCESS_CONTROL_READ, &g_zcl_basicAttrs.powerSource},
    {ZCL_ATTRID_BASIC_DEV_ENABLED, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, &g_zcl_basicAttrs.deviceEnable},
    {ZCL_ATTRID_BASIC_SW_BUILD_ID, ZCL_DATA_TYPE_CHAR_STR, ACCESS_CONTROL_READ, g_zcl_basicAttrs.swBuildId},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

zcl_identifyAttr_t g_zcl_identifyAttrs;
static const zclAttrInfo_t identifyAttrTable[] = {
    {ZCL_ATTRID_IDENTIFY_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_identifyAttrs.identifyTime},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};
static const zclAttrInfo_t batteryAttrTable[] = {
    {ZCL_ATTRID_BATTERY_VOLTAGE, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, &gSensorBatteryVoltage},
    {ZCL_ATTRID_BATTERY_PERCENTAGE_REMAINING, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, &gSensorBatteryPercent},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};
zcl_iasZoneAttr_t g_zcl_iasZoneAttrs = {
    .zoneState  = ZONE_STATE_NOT_ENROLLED,
    .zoneType   = ZONE_TYPE_CONTACT_SWITCH,
    .zoneStatus = 0x00,
    .iasCieAddr = {0x00},
    .zoneId     = ZCL_ZONE_ID_INVALID,
};

const zclAttrInfo_t iasZone_attrTbl[] = {
    { ZCL_ATTRID_ZONE_STATE,              ZCL_DATA_TYPE_ENUM8,     ACCESS_CONTROL_READ,                        (u8*)&g_zcl_iasZoneAttrs.zoneState },
    { ZCL_ATTRID_ZONE_TYPE,               ZCL_DATA_TYPE_ENUM16,    ACCESS_CONTROL_READ,                        (u8*)&g_zcl_iasZoneAttrs.zoneType },
    { ZCL_ATTRID_ZONE_STATUS,             ZCL_DATA_TYPE_BITMAP16,  ACCESS_CONTROL_READ,                        (u8*)&g_zcl_iasZoneAttrs.zoneStatus },
    { ZCL_ATTRID_IAS_CIE_ADDR,            ZCL_DATA_TYPE_IEEE_ADDR, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)g_zcl_iasZoneAttrs.iasCieAddr },
    { ZCL_ATTRID_ZONE_ID,                 ZCL_DATA_TYPE_UINT8,     ACCESS_CONTROL_READ,                        (u8*)&g_zcl_iasZoneAttrs.zoneId},

    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,    ACCESS_CONTROL_READ,                        (u8*)&zcl_attr_global_clusterRevision},
};


#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
const zcl_specClusterInfo_t sensor_clusterList[4] = {
    {ZCL_CLUSTER_GEN_BASIC, MANUFACTURER_CODE_NONE, COUNT(basicAttrTable), basicAttrTable, zcl_basic_register, sensor_basicCb},
    {ZCL_CLUSTER_GEN_IDENTIFY, MANUFACTURER_CODE_NONE, COUNT(identifyAttrTable), identifyAttrTable, zcl_identify_register, sensor_identifyCb},
    {ZCL_CLUSTER_GEN_POWER_CFG, MANUFACTURER_CODE_NONE, COUNT(batteryAttrTable), batteryAttrTable, zcl_powerCfg_register, NULL},
    {ZCL_CLUSTER_SS_IAS_ZONE, MANUFACTURER_CODE_NONE, COUNT(iasZone_attrTbl), iasZone_attrTbl, zcl_iasZone_register, sensor_iasCb},
};
typedef struct {
    addrExt_t iasCieAddr;
    u8 zoneId;
    u8 zoneState;
} zcl_nv_iasZone_t;
nv_sts_t sensor_iasSave(void)
{
    nv_sts_t st = NV_SUCC;

#ifdef ZCL_IAS_ZONE
#if NV_ENABLE
    bool needSave = FALSE;
    zcl_nv_iasZone_t zcl_nv_iasZone;

    st = nv_flashReadNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_IAS_ZONE, sizeof(zcl_nv_iasZone_t), (u8 *)&zcl_nv_iasZone);
    if (st == NV_SUCC) {
        if (!ZB_IEEE_ADDR_CMP(zcl_nv_iasZone.iasCieAddr, g_zcl_iasZoneAttrs.iasCieAddr) ||
           (zcl_nv_iasZone.zoneId != g_zcl_iasZoneAttrs.zoneId) ||
           (zcl_nv_iasZone.zoneState != g_zcl_iasZoneAttrs.zoneState)) {
            needSave = TRUE;
        }
    } else if(st == NV_ITEM_NOT_FOUND) {
        needSave = TRUE;
    }

    if (needSave) {
        ZB_IEEE_ADDR_COPY(zcl_nv_iasZone.iasCieAddr, g_zcl_iasZoneAttrs.iasCieAddr);
        zcl_nv_iasZone.zoneId = g_zcl_iasZoneAttrs.zoneId;
        zcl_nv_iasZone.zoneState = g_zcl_iasZoneAttrs.zoneState;

        st = nv_flashWriteNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_IAS_ZONE, sizeof(zcl_nv_iasZone_t), (u8 *)&zcl_nv_iasZone);
    }
#else
    st = NV_ENABLE_PROTECT_ERROR;
#endif
#endif

    return st;
}

/*********************************************************************
 * @fn      sensor_iasRestore
 *
 * @brief
 *
 * @param   None
 *
 * @return
 */
nv_sts_t sensor_iasRestore(void)
{
    nv_sts_t st = NV_SUCC;

#ifdef ZCL_IAS_ZONE
#if NV_ENABLE
    zcl_nv_iasZone_t zcl_nv_iasZone;

    st = nv_flashReadNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_IAS_ZONE, sizeof(zcl_nv_iasZone_t), (u8 *)&zcl_nv_iasZone);
    if (st == NV_SUCC) {
        ZB_IEEE_ADDR_COPY(g_zcl_iasZoneAttrs.iasCieAddr, zcl_nv_iasZone.iasCieAddr);
        g_zcl_iasZoneAttrs.zoneId = zcl_nv_iasZone.zoneId;
        g_zcl_iasZoneAttrs.zoneState = zcl_nv_iasZone.zoneState;
    }
#else
    st = NV_ENABLE_PROTECT_ERROR;
#endif
#endif

    return st;
}

void sensor_attrsInit(void)
{
    sensor_iasRestore();
    sensor_iasInit();
}
