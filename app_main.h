#pragma once

/* Application context, ZCL attributes and stack callbacks. */

typedef struct {
    u8 keyType;
    u8 key[16];
} app_linkKey_info_t;

typedef struct {
    ev_timer_event_t *timerLedEvt;
    u32 keyPressedTime;
    u8 state;
    bool factoryResetPending;
    u16 ledOnTime;
    u16 ledOffTime;
    u16 times;
    app_linkKey_info_t tcLinkKey;
} app_ctx_t;

typedef struct {
    u8 zclVersion;
    u8 appVersion;
    u8 stackVersion;
    u8 hwVersion;
    u8 manuName[ZCL_BASIC_MAX_LENGTH];
    u8 modelId[ZCL_BASIC_MAX_LENGTH];
    u8 swBuildId[ZCL_BASIC_MAX_LENGTH];
    u8 powerSource;
    u8 deviceEnable;
} zcl_basicAttr_t;

typedef struct {
    u16 identifyTime;
} zcl_identifyAttr_t;

typedef struct {
    u8 nameSupport;
} zcl_groupAttr_t;

typedef struct {
    u8 sceneCount;
    u8 currentScene;
    u8 nameSupport;
    bool sceneValid;
    u16 currentGroup;
} zcl_sceneAttr_t;

typedef struct {
    u16 onTime;
    u16 offWaitTime;
    u8 startUpOnOff;
    bool onOff;
    bool globalSceneControl;
} zcl_onOffAttr_t;

typedef struct _attribute_packed_ {
    u8 onOff;
    u8 startUpOnOff;
} zcl_nv_onOff_t;

extern app_ctx_t gSmartPlugCtx;
extern bdb_commissionSetting_t g_bdbCommissionSetting;
extern bdb_appCb_t g_zbDemoBdbCb;

extern u8 g_smartPlugClusterCount;
extern const zcl_specClusterInfo_t g_smartPlugClusterList[];
extern const af_simple_descriptor_t g_smartPlugSimpleDesc;

extern zcl_basicAttr_t g_zcl_basicAttrs;
extern zcl_identifyAttr_t g_zcl_identifyAttrs;
extern zcl_groupAttr_t g_zcl_groupAttrs;
extern zcl_sceneAttr_t g_zcl_sceneAttrs;
extern zcl_onOffAttr_t g_zcl_onOffAttrs;

#define zcl_sceneAttrGet() (&g_zcl_sceneAttrs)
#define zcl_onoffAttrGet() (&g_zcl_onOffAttrs)

void smartPlug_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 identifyTime);
void smartPlug_zclProcessIncomingMsg(zclIncoming_t *message);
void smartPlug_zclRxHandler(void *data);
bool smartPlug_zclPacketValid(u16 cluster, const u8 *frame, u16 length);
void smartPlug_networkTask(void);

status_t smartPlug_basicCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload);
status_t smartPlug_identifyCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload);
status_t smartPlug_sceneCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload);
status_t smartPlug_onOffCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload);

void smartPlug_otaProcessMsgHandler(u8 evt, u8 status);
void smartPlug_leaveCnfHandler(nlme_leave_cnf_t *confirmation);
void smartPlug_leaveIndHandler(nlme_leave_ind_t *indication);
bool smartPlug_panidConflictIndicateHandler(u16 newPanId);
void smartPlug_nwkStatusIndHandler(zdo_nwk_status_ind_t *indication);

void smartPlug_onOffInit(void);
void smartPlug_onOffUpdate(u8 commandId);

void zcl_smartPlugAttrsInit(void);
nv_sts_t zcl_onOffAttr_save(void);
