#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__)

#include "tl_common.h"
#include "zcl_include.h"
#include "smart_plug.h"
#include "app_main.h"

#define ZCL_BASIC_MFG_NAME    {13, 'S', 'a', 'l', 'u', ' ', 'W', 'o', 'r', 'k', 's', 'h', 'o', 'p'}
#define ZCL_BASIC_MODEL_ID    {8, 'S', 'W', 'P', 'L', 'U', 'G', '1', 'x'}
#define ZCL_BASIC_SW_BUILD_ID {10, '2', '0', '2', '6', '1', '0', '0', '1', '0', '4'}

static const u16 smartPlugInClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_GEN_GROUPS,
    ZCL_CLUSTER_GEN_SCENES,
    ZCL_CLUSTER_GEN_ON_OFF,
    ZCL_CLUSTER_SE_METERING,
    ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
    SMART_PLUG_CONFIG_CLUSTER_ID,
};

static const u16 smartPlugOutClusterList[] = {
    ZCL_CLUSTER_OTA,
};

#define SMART_PLUG_IN_CLUSTER_NUM  (sizeof(smartPlugInClusterList) / sizeof(smartPlugInClusterList[0]))
#define SMART_PLUG_OUT_CLUSTER_NUM (sizeof(smartPlugOutClusterList) / sizeof(smartPlugOutClusterList[0]))

const af_simple_descriptor_t g_smartPlugSimpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SMART_PLUG,
    SMART_PLUG_ENDPOINT,
    1,
    0,
    SMART_PLUG_IN_CLUSTER_NUM,
    SMART_PLUG_OUT_CLUSTER_NUM,
    (u16 *)smartPlugInClusterList,
    (u16 *)smartPlugOutClusterList,
};

zcl_basicAttr_t g_zcl_basicAttrs = {
    .zclVersion = 0x03,
    .appVersion = 0x00,
    .stackVersion = 0x02,
    .hwVersion = 0x00,
    .manuName = ZCL_BASIC_MFG_NAME,
    .modelId = ZCL_BASIC_MODEL_ID,
    .powerSource = POWER_SOURCE_MAINS_1_PHASE,
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

zcl_groupAttr_t g_zcl_groupAttrs;
static const zclAttrInfo_t groupAttrTable[] = {
    {ZCL_ATTRID_GROUP_NAME_SUPPORT, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_zcl_groupAttrs.nameSupport},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

zcl_sceneAttr_t g_zcl_sceneAttrs;
static const zclAttrInfo_t sceneAttrTable[] = {
    {ZCL_ATTRID_SCENE_SCENE_COUNT, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs.sceneCount},
    {ZCL_ATTRID_SCENE_CURRENT_SCENE, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs.currentScene},
    {ZCL_ATTRID_SCENE_CURRENT_GROUP, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_zcl_sceneAttrs.currentGroup},
    {ZCL_ATTRID_SCENE_SCENE_VALID, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ, (u8 *)&g_zcl_sceneAttrs.sceneValid},
    {ZCL_ATTRID_SCENE_NAME_SUPPORT, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs.nameSupport},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

zcl_onOffAttr_t g_zcl_onOffAttrs = {
    .globalSceneControl = TRUE,
    .startUpOnOff = ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS,
};

static const zclAttrInfo_t onOffAttrTable[] = {
    {ZCL_ATTRID_ONOFF, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8 *)&g_zcl_onOffAttrs.onOff},
    {ZCL_ATTRID_GLOBAL_SCENE_CONTROL, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ, (u8 *)&g_zcl_onOffAttrs.globalSceneControl},
    {ZCL_ATTRID_ON_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_onOffAttrs.onTime},
    {ZCL_ATTRID_OFF_WAIT_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_onOffAttrs.offWaitTime},
    {ZCL_ATTRID_START_UP_ONOFF, ZCL_DATA_TYPE_ENUM8, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, &g_zcl_onOffAttrs.startUpOnOff},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

/* Telemetry uses the millisecond scheduler; SDK reporting is reserved for On/Off. */
static const zclAttrInfo_t electricalAttrTable[] = {
    {ZCL_ATTRID_MEASUREMENT_TYPE, ZCL_DATA_TYPE_BITMAP32, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.measurementType},
    {ZCL_ATTRID_RMS_VOLTAGE, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.rmsVoltage},
    {ZCL_ATTRID_RMS_CURRENT, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.rmsCurrent},
    {ZCL_ATTRID_ACTIVE_POWER, ZCL_DATA_TYPE_INT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.activePower},
    {ZCL_ATTRID_APPARENT_POWER, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.apparentPower},
    {ZCL_ATTRID_POWER_FACTOR, ZCL_DATA_TYPE_INT8, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.powerFactor},
    {ZCL_ATTRID_AC_VOLTAGE_MULTIPLIER, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.voltageMultiplier},
    {ZCL_ATTRID_AC_VOLTAGE_DIVISOR, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.voltageDivisor},
    {ZCL_ATTRID_AC_CURRENT_MULTIPLIER, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.currentMultiplier},
    {ZCL_ATTRID_AC_CURRENT_DIVISOR, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.currentDivisor},
    {ZCL_ATTRID_AC_POWER_MULTIPLIER, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.powerMultiplier},
    {ZCL_ATTRID_AC_POWER_DIVISOR, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_smartPlugElectricalAttrs.powerDivisor},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

static const zclAttrInfo_t meteringAttrTable[] = {
    {ZCL_ATTRID_STATUS, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_smartPlugMeteringAttrs.status},
    {ZCL_ATTRID_CURRENT_SUMMATION_DELIVERD, ZCL_DATA_TYPE_UINT48, ACCESS_CONTROL_READ, g_smartPlugMeteringAttrs.currentSummationDelivered},
    {ZCL_ATTRID_UNIT_OF_MEASURE, ZCL_DATA_TYPE_ENUM8, ACCESS_CONTROL_READ, &g_smartPlugMeteringAttrs.unitOfMeasure},
    {ZCL_ATTRID_MULTIPLIER, ZCL_DATA_TYPE_UINT24, ACCESS_CONTROL_READ, g_smartPlugMeteringAttrs.multiplier},
    {ZCL_ATTRID_DIVISOR, ZCL_DATA_TYPE_UINT24, ACCESS_CONTROL_READ, g_smartPlugMeteringAttrs.divisor},
    {ZCL_ATTRID_SUMMATION_FORMATTING, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_smartPlugMeteringAttrs.summationFormatting},
    {ZCL_ATTRID_DEMAND_FORMATTING, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_smartPlugMeteringAttrs.demandFormatting},
    {ZCL_ATTRID_METERING_DEVICE_TYPE, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_smartPlugMeteringAttrs.meteringDeviceType},
    {ZCL_ATTRID_INSTANTANEOUS_DEMAND, ZCL_DATA_TYPE_INT24, ACCESS_CONTROL_READ, g_smartPlugMeteringAttrs.instantaneousDemand},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

static const zclAttrInfo_t configAttrTable[] = {
    {SMART_PLUG_ATTR_TELEMETRY_INTERVAL_MS, ZCL_DATA_TYPE_UINT32, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_smartPlugTelemetryIntervalMs},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

static status_t smartPlugConfigCommand(zclIncoming_t *message)
{
    /* SDK dispatch calls cmdHandlerFunc without checking for NULL. */
    return message->hdr.manufCode == MANUFACTURER_CODE_NONE
        ? ZCL_STA_UNSUP_CLUSTER_COMMAND : ZCL_STA_UNSUP_MANU_CLUSTER_COMMAND;
}

static status_t smartPlugConfigRegister(u8 endpoint, u16 manufacturerCode, u8 attrCount,
                                        const zclAttrInfo_t attrTable[], cluster_forAppCb_t callback)
{
    return zcl_registerCluster(endpoint, SMART_PLUG_CONFIG_CLUSTER_ID, manufacturerCode,
                               attrCount, attrTable, smartPlugConfigCommand, callback);
}

#define ATTR_COUNT(table) (sizeof(table) / sizeof((table)[0]))

const zcl_specClusterInfo_t g_smartPlugClusterList[] = {
    {ZCL_CLUSTER_GEN_BASIC, MANUFACTURER_CODE_NONE, ATTR_COUNT(basicAttrTable), basicAttrTable, zcl_basic_register, smartPlug_basicCb},
    {ZCL_CLUSTER_GEN_IDENTIFY, MANUFACTURER_CODE_NONE, ATTR_COUNT(identifyAttrTable), identifyAttrTable, zcl_identify_register, smartPlug_identifyCb},
    {ZCL_CLUSTER_GEN_GROUPS, MANUFACTURER_CODE_NONE, ATTR_COUNT(groupAttrTable), groupAttrTable, zcl_group_register, NULL},
    {ZCL_CLUSTER_GEN_SCENES, MANUFACTURER_CODE_NONE, ATTR_COUNT(sceneAttrTable), sceneAttrTable, zcl_scene_register, smartPlug_sceneCb},
    {ZCL_CLUSTER_GEN_ON_OFF, MANUFACTURER_CODE_NONE, ATTR_COUNT(onOffAttrTable), onOffAttrTable, zcl_onOff_register, smartPlug_onOffCb},
    {ZCL_CLUSTER_SE_METERING, MANUFACTURER_CODE_NONE, ATTR_COUNT(meteringAttrTable), meteringAttrTable, zcl_metering_register, NULL},
    {ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT, MANUFACTURER_CODE_NONE, ATTR_COUNT(electricalAttrTable), electricalAttrTable, zcl_electricalMeasure_register, NULL},
    {SMART_PLUG_CONFIG_CLUSTER_ID, MANUFACTURER_CODE_NONE, ATTR_COUNT(configAttrTable), configAttrTable, smartPlugConfigRegister, NULL},
};

u8 g_smartPlugClusterCount = ATTR_COUNT(g_smartPlugClusterList);

nv_sts_t zcl_onOffAttr_save(void)
{
#if NV_ENABLE
    zcl_nv_onOff_t stored;
    bool save = FALSE;
    nv_sts_t status = nv_flashReadNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_ON_OFF,
                                      sizeof(stored), (u8 *)&stored);

    if (status == NV_SUCC) {
        save = (stored.onOff != g_zcl_onOffAttrs.onOff) ||
               (stored.startUpOnOff != g_zcl_onOffAttrs.startUpOnOff);
    } else if (status == NV_ITEM_NOT_FOUND) {
        save = TRUE;
    }

    if (save) {
        stored.onOff = g_zcl_onOffAttrs.onOff;
        stored.startUpOnOff = g_zcl_onOffAttrs.startUpOnOff;
        status = nv_flashWriteNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_ON_OFF,
                                  sizeof(stored), (u8 *)&stored);
    }
    return status;
#else
    return NV_ENABLE_PROTECT_ERROR;
#endif
}

static void zcl_onOffAttr_restore(void)
{
#if NV_ENABLE
    zcl_nv_onOff_t stored;
    if (nv_flashReadNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_ON_OFF,
                       sizeof(stored), (u8 *)&stored) == NV_SUCC) {
        if ((stored.onOff <= ZCL_ONOFF_STATUS_ON) &&
            ((stored.startUpOnOff <= ZCL_START_UP_ONOFF_SET_ONOFF_TOGGLE) ||
             (stored.startUpOnOff == ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS))) {
            g_zcl_onOffAttrs.onOff = stored.onOff;
            g_zcl_onOffAttrs.startUpOnOff = stored.startUpOnOff;
        }
    }
#endif
}

void zcl_smartPlugAttrsInit(void)
{
    zcl_onOffAttr_restore();
}

#endif
