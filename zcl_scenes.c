#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__)

#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "smart_plug.h"
#include "app_main.h"

static void sceneRecall(zclIncomingAddrInfo_t *address, zcl_sceneEntry_t *scene)
{
    if (scene->extFieldLen > ZCL_MAX_SCENE_EXT_FIELD_SIZE) {
        return;
    }
    u8 *data = scene->extField;
    u8 *end = scene->extField + scene->extFieldLen;

    /* Validate the entire extension before applying any relay changes. */
    u8 *check = data;
    while (check < end) {
        if ((end - check) < 3 || check[2] > (end - check - 3)) {
            return;
        }
        if (BUILD_U16(check[0], check[1]) == ZCL_CLUSTER_GEN_ON_OFF &&
            (check[2] != 1 || check[3] > ZCL_ONOFF_STATUS_ON)) {
            return;
        }
        check += 3 + check[2];
    }

    while ((end - data) >= 3) {
        u16 clusterId = BUILD_U16(data[0], data[1]);
        u8 length = data[2];
        data += 3;

        if (length > (end - data)) {
            return;
        }
        if ((clusterId == ZCL_CLUSTER_GEN_ON_OFF) && (length >= 1) &&
            (data[0] <= ZCL_ONOFF_STATUS_ON)) {
            smartPlug_onOffCb(address, data[0], NULL);
        }
        data += length;
    }
}

static void sceneStore(zcl_sceneEntry_t *scene)
{
    zcl_onOffAttr_t *onOff = zcl_onoffAttrGet();
    scene->extField[0] = LO_UINT16(ZCL_CLUSTER_GEN_ON_OFF);
    scene->extField[1] = HI_UINT16(ZCL_CLUSTER_GEN_ON_OFF);
    scene->extField[2] = 1;
    scene->extField[3] = onOff->onOff;
    scene->extFieldLen = 4;
}

status_t smartPlug_sceneCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload)
{
    if ((address->dstEp != SMART_PLUG_ENDPOINT) ||
        (address->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR)) {
        return ZCL_STA_SUCCESS;
    }

    if (commandId == ZCL_CMD_SCENE_STORE_SCENE) {
        sceneStore((zcl_sceneEntry_t *)payload);
        return ZCL_STA_SUCCESS;
    }
    if (commandId == ZCL_CMD_SCENE_RECALL_SCENE) {
        sceneRecall(address, (zcl_sceneEntry_t *)payload);
        return ZCL_STA_SUCCESS;
    }
    return ZCL_STA_UNSUP_MANU_CLUSTER_COMMAND;
}

#endif
