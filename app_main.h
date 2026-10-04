#pragma once

/* Application context, ZCL attributes and stack callbacks. */

typedef struct {
    u8 keyType;
    u8 key[16];
} app_linkKey_info_t;

typedef struct {
    ev_timer_event_t *timerLedEvt;
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

extern app_ctx_t gRelayCtx;
extern bdb_commissionSetting_t g_bdbCommissionSetting;
extern bdb_appCb_t g_zbDemoBdbCb;

extern u8 g_relay2ClusterCount;
extern const zcl_specClusterInfo_t g_relay2ClusterList[2][6];
extern const af_simple_descriptor_t g_relay2SimpleDesc[2];

extern zcl_basicAttr_t g_zcl_basicAttrs;
extern zcl_identifyAttr_t g_zcl_identifyAttrs[2];
extern zcl_groupAttr_t g_zcl_groupAttrs[2];
extern zcl_sceneAttr_t g_zcl_sceneAttrs[2];
extern zcl_onOffAttr_t g_zcl_onOffAttrs[2];

#define zcl_sceneAttrGet(ch) (&g_zcl_sceneAttrs[(ch)])
#define zcl_onoffAttrGet(ch) (&g_zcl_onOffAttrs[(ch)])

void relay2_identifyStop(void);
void relay2_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 identifyTime);
void relay2_zclProcessIncomingMsg(zclIncoming_t *message);
void relay2_zclRxHandler(void *data);
bool relay2_zclPacketValid(u16 cluster, const u8 *frame, u16 length);
void relay2_networkTask(void);

status_t relay2_basicCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload);
status_t relay2_identifyCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload);
status_t relay2_sceneCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload);
status_t relay2_onOffCb(zclIncomingAddrInfo_t *address, u8 commandId, void *payload);

void relay2_otaProcessMsgHandler(u8 evt, u8 status);
void relay2_leaveCnfHandler(nlme_leave_cnf_t *confirmation);
void relay2_leaveIndHandler(nlme_leave_ind_t *indication);
bool relay2_panidConflictIndicateHandler(u16 newPanId);
void relay2_nwkStatusIndHandler(zdo_nwk_status_ind_t *indication);

void relay2_onOffInit(void);
void relay2_onOffUpdate(u8 channel, u8 commandId);

void zcl_relay2AttrsInit(void);
void relay2_cancelTimedOff(void);
