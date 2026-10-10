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

extern app_ctx_t gSensorCtx;
extern bdb_commissionSetting_t g_bdbCommissionSetting;
extern bdb_appCb_t g_zbDemoBdbCb;
extern const af_simple_descriptor_t sensor_simpleDesc;
extern const zcl_specClusterInfo_t sensor_clusterList[4];
extern zcl_basicAttr_t g_zcl_basicAttrs;
extern zcl_identifyAttr_t g_zcl_identifyAttrs;
typedef struct {
    u8 zoneState;
    u16 zoneType;
    u16 zoneStatus;
    addrExt_t iasCieAddr;
    u8 zoneId;
} zcl_iasZoneAttr_t;
extern zcl_iasZoneAttr_t g_zcl_iasZoneAttrs;
void sensor_attrsInit(void);
nv_sts_t sensor_iasSave(void);
void sensor_iasTask(void);
void sensor_zclProcessIncomingMsg(zclIncoming_t *message);
void sensor_zclRxHandler(void *data);
bool sensor_zclPacketValid(u16 cluster, const u8 *frame, u16 length);
void sensor_networkTask(void);
bool sensor_networkCanSleep(void);
void sensor_iasInit(void);
void sensor_identifyStop(void);
void sensor_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 time);
status_t sensor_basicCb(zclIncomingAddrInfo_t *, u8, void *);
status_t sensor_identifyCb(zclIncomingAddrInfo_t *, u8, void *);
status_t sensor_iasCb(zclIncomingAddrInfo_t *, u8, void *);
void sensor_otaProcessMsgHandler(u8 evt, u8 status);
void sensor_leaveCnfHandler(nlme_leave_cnf_t *);
void sensor_leaveIndHandler(nlme_leave_ind_t *);
bool sensor_panidConflictIndicateHandler(u16);
void sensor_nwkStatusIndHandler(zdo_nwk_status_ind_t *);
