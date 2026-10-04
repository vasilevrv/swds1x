#include "app_cfg.h"
#if (__PROJECT_TL_RELAY_2CH__)
#include "tl_common.h"
#include "zcl_include.h"
#include "relay_2ch.h"
#include "app_main.h"

#define ZCL_BASIC_MFG_NAME    {13, 'S', 'a', 'l', 'u', ' ', 'W', 'o', 'r', 'k', 's', 'h', 'o', 'p'}
#define ZCL_BASIC_MODEL_ID    {9, 's', 'w', 'r', 'e', 'l', 'a', 'y', '2', 'x'}
#define ZCL_BASIC_SW_BUILD_ID {10, '2', '0', '2', '6', '1', '0', '0', '4', '0', '4'}

static const u16 inClusters[] = {
    ZCL_CLUSTER_GEN_BASIC, ZCL_CLUSTER_GEN_IDENTIFY, ZCL_CLUSTER_GEN_GROUPS,
    ZCL_CLUSTER_GEN_SCENES, ZCL_CLUSTER_GEN_ON_OFF, RELAY_2CH_CONFIG_CLUSTER_ID,
};
static const u16 outClusters1[] = {ZCL_CLUSTER_OTA, RELAY_2CH_CONFIG_CLUSTER_ID};
static const u16 outClusters2[] = {RELAY_2CH_CONFIG_CLUSTER_ID};
const af_simple_descriptor_t g_relay2SimpleDesc[2] = {
    {HA_PROFILE_ID, HA_DEV_ONOFF_OUTPUT, 1, 1, 0, 6, 2, (u16 *)inClusters, (u16 *)outClusters1},
    {HA_PROFILE_ID, HA_DEV_ONOFF_OUTPUT, 2, 1, 0, 6, 1, (u16 *)inClusters, (u16 *)outClusters2},
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

zcl_identifyAttr_t g_zcl_identifyAttrs[2];
static const zclAttrInfo_t identifyAttrTable0[] = {
    {ZCL_ATTRID_IDENTIFY_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_identifyAttrs[0].identifyTime},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};
static const zclAttrInfo_t identifyAttrTable1[] = {
    {ZCL_ATTRID_IDENTIFY_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_identifyAttrs[1].identifyTime},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

zcl_groupAttr_t g_zcl_groupAttrs[2];
static const zclAttrInfo_t groupAttrTable0[] = {
    {ZCL_ATTRID_GROUP_NAME_SUPPORT, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_zcl_groupAttrs[0].nameSupport},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};
static const zclAttrInfo_t groupAttrTable1[] = {
    {ZCL_ATTRID_GROUP_NAME_SUPPORT, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_zcl_groupAttrs[1].nameSupport},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

zcl_sceneAttr_t g_zcl_sceneAttrs[2];
static const zclAttrInfo_t sceneAttrTable0[] = {
    {ZCL_ATTRID_SCENE_SCENE_COUNT, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs[0].sceneCount},
    {ZCL_ATTRID_SCENE_CURRENT_SCENE, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs[0].currentScene},
    {ZCL_ATTRID_SCENE_CURRENT_GROUP, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_zcl_sceneAttrs[0].currentGroup},
    {ZCL_ATTRID_SCENE_SCENE_VALID, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ, (u8 *)&g_zcl_sceneAttrs[0].sceneValid},
    {ZCL_ATTRID_SCENE_NAME_SUPPORT, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs[0].nameSupport},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};
static const zclAttrInfo_t sceneAttrTable1[] = {
    {ZCL_ATTRID_SCENE_SCENE_COUNT, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs[1].sceneCount},
    {ZCL_ATTRID_SCENE_CURRENT_SCENE, ZCL_DATA_TYPE_UINT8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs[1].currentScene},
    {ZCL_ATTRID_SCENE_CURRENT_GROUP, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&g_zcl_sceneAttrs[1].currentGroup},
    {ZCL_ATTRID_SCENE_SCENE_VALID, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ, (u8 *)&g_zcl_sceneAttrs[1].sceneValid},
    {ZCL_ATTRID_SCENE_NAME_SUPPORT, ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, &g_zcl_sceneAttrs[1].nameSupport},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

zcl_onOffAttr_t g_zcl_onOffAttrs[2] = {
    {.globalSceneControl = TRUE, .startUpOnOff = ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS},
    {.globalSceneControl = TRUE, .startUpOnOff = ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS},
};

static const zclAttrInfo_t onOffAttrTable0[] = {
    {ZCL_ATTRID_ONOFF, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8 *)&g_zcl_onOffAttrs[0].onOff},
    {ZCL_ATTRID_GLOBAL_SCENE_CONTROL, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ, (u8 *)&g_zcl_onOffAttrs[0].globalSceneControl},
    {ZCL_ATTRID_ON_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_onOffAttrs[0].onTime},
    {ZCL_ATTRID_OFF_WAIT_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_onOffAttrs[0].offWaitTime},
    {ZCL_ATTRID_START_UP_ONOFF, ZCL_DATA_TYPE_ENUM8, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, &g_zcl_onOffAttrs[0].startUpOnOff},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};
static const zclAttrInfo_t onOffAttrTable1[] = {
    {ZCL_ATTRID_ONOFF, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8 *)&g_zcl_onOffAttrs[1].onOff},
    {ZCL_ATTRID_GLOBAL_SCENE_CONTROL, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ, (u8 *)&g_zcl_onOffAttrs[1].globalSceneControl},
    {ZCL_ATTRID_ON_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_onOffAttrs[1].onTime},
    {ZCL_ATTRID_OFF_WAIT_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_zcl_onOffAttrs[1].offWaitTime},
    {ZCL_ATTRID_START_UP_ONOFF, ZCL_DATA_TYPE_ENUM8, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, &g_zcl_onOffAttrs[1].startUpOnOff},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

static const zclAttrInfo_t configAttrTable0[] = {
    {RELAY_ATTR_BUTTON_LINKED, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, &g_relayConfig[0].buttonLinked},
    {RELAY_ATTR_DOUBLE_CLICK, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, &g_relayConfig[0].doubleClickEnabled},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};
static const zclAttrInfo_t configAttrTable1[] = {
    {RELAY_ATTR_BUTTON_LINKED, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, &g_relayConfig[1].buttonLinked},
    {RELAY_ATTR_DOUBLE_CLICK, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, &g_relayConfig[1].doubleClickEnabled},
    {ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision},
};

static status_t configCommand(zclIncoming_t *message)
{
    return message->hdr.manufCode == MANUFACTURER_CODE_NONE
        ? ZCL_STA_UNSUP_CLUSTER_COMMAND : ZCL_STA_UNSUP_MANU_CLUSTER_COMMAND;
}
static status_t configRegister(u8 endpoint, u16 manufacturer, u8 count,
                               const zclAttrInfo_t attrs[], cluster_forAppCb_t callback)
{
    return zcl_registerCluster(endpoint, RELAY_2CH_CONFIG_CLUSTER_ID, manufacturer,
                               count, attrs, configCommand, callback);
}
#define ATTR_COUNT(table) (sizeof(table) / sizeof((table)[0]))
const zcl_specClusterInfo_t g_relay2ClusterList[2][6] = {
    {
        {ZCL_CLUSTER_GEN_BASIC, MANUFACTURER_CODE_NONE, ATTR_COUNT(basicAttrTable), basicAttrTable, zcl_basic_register, relay2_basicCb},
        {ZCL_CLUSTER_GEN_IDENTIFY, MANUFACTURER_CODE_NONE, ATTR_COUNT(identifyAttrTable0), identifyAttrTable0, zcl_identify_register, relay2_identifyCb},
        {ZCL_CLUSTER_GEN_GROUPS, MANUFACTURER_CODE_NONE, ATTR_COUNT(groupAttrTable0), groupAttrTable0, zcl_group_register, NULL},
        {ZCL_CLUSTER_GEN_SCENES, MANUFACTURER_CODE_NONE, ATTR_COUNT(sceneAttrTable0), sceneAttrTable0, zcl_scene_register, relay2_sceneCb},
        {ZCL_CLUSTER_GEN_ON_OFF, MANUFACTURER_CODE_NONE, ATTR_COUNT(onOffAttrTable0), onOffAttrTable0, zcl_onOff_register, relay2_onOffCb},
        {RELAY_2CH_CONFIG_CLUSTER_ID, MANUFACTURER_CODE_NONE, ATTR_COUNT(configAttrTable0), configAttrTable0, configRegister, NULL},
    },
    {
        {ZCL_CLUSTER_GEN_BASIC, MANUFACTURER_CODE_NONE, ATTR_COUNT(basicAttrTable), basicAttrTable, zcl_basic_register, relay2_basicCb},
        {ZCL_CLUSTER_GEN_IDENTIFY, MANUFACTURER_CODE_NONE, ATTR_COUNT(identifyAttrTable1), identifyAttrTable1, zcl_identify_register, relay2_identifyCb},
        {ZCL_CLUSTER_GEN_GROUPS, MANUFACTURER_CODE_NONE, ATTR_COUNT(groupAttrTable1), groupAttrTable1, zcl_group_register, NULL},
        {ZCL_CLUSTER_GEN_SCENES, MANUFACTURER_CODE_NONE, ATTR_COUNT(sceneAttrTable1), sceneAttrTable1, zcl_scene_register, relay2_sceneCb},
        {ZCL_CLUSTER_GEN_ON_OFF, MANUFACTURER_CODE_NONE, ATTR_COUNT(onOffAttrTable1), onOffAttrTable1, zcl_onOff_register, relay2_onOffCb},
        {RELAY_2CH_CONFIG_CLUSTER_ID, MANUFACTURER_CODE_NONE, ATTR_COUNT(configAttrTable1), configAttrTable1, configRegister, NULL},
    },
};
u8 g_relay2ClusterCount = 6;
void zcl_relay2AttrsInit(void) { relay2_init(); }
#endif
