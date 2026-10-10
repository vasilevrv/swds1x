#include "host_sdk.h"
static u32 app_nowMs(void) {return now_ticks / 16000U;}
#define ZCL_FRAME_CLIENT_SERVER_DIR 0
#define ZCL_CMD_WRITE 2
#define ZCL_CMD_WRITE_UNDIVIDED 3
#define ZCL_CMD_WRITE_NO_RSP 5
#define ZCL_CLUSTER_SS_IAS_ZONE 0x500
#define ZCL_CLUSTER_GEN_IDENTIFY 3
#define ZCL_ATTRID_IAS_CIE_ADDR 0x10
#define ZCL_ATTRID_IDENTIFY_TIME 0
#define ZCL_CMD_ZONE_ENROLL_RSP 0
#define ZONE_TYPE_CONTACT_SWITCH 0x15
#define ZONE_STATE_NOT_ENROLLED 0
#define ZONE_STATE_ENROLLED 1
#define ZCL_ZONE_ID_INVALID 255
#define ZONE_ENROLL_SUCC 0
#define ZCL_STA_SUCCESS 0
#define ZCL_STA_FAILURE 1
#define ZCL_STA_UNSUP_CLUSTER_COMMAND 129
#define SUCCESS 0
#define NV_SUCC 0
#define NV_ITEM_NOT_FOUND 1
#define NV_ENABLE 1
#define ZCL_IAS_ZONE 1
#define NV_MODULE_ZCL 4
#define NV_ITEM_ZCL_IAS_ZONE 10
#define MANUFACTURER_CODE 0x1141
#define HA_PROFILE_ID 0x104
#define APS_SHORT_DSTADDR_WITHEP 2
#define ZDO_ADDR_REQ_SINGLE_RESP 0
#define NWK_BROADCAST_ROUTER_COORDINATOR 0xfffc
typedef u8 status_t;
typedef u8 nv_sts_t;
typedef u8 addrExt_t[8];
#define ZB_IEEE_ADDR_COPY(a,b) memcpy(a,b,8)
#define ZB_IEEE_ADDR_CMP(a,b) (memcmp(a,b,8)==0)
static bool emptyAddr(const u8 *a,u8 value) {for(u8 i=0;i<8;++i) if(a[i]!=value)return false;return true;}
#define ZB_IEEE_ADDR_IS_ZERO(a) emptyAddr(a,0)
#define ZB_IEEE_ADDR_IS_INVALID(a) emptyAddr(a,255)
static struct {bool factoryResetPending;} gSensorCtx;
static struct {u16 identifyTime;} g_zcl_identifyAttrs;
static struct {u8 zoneState;u16 zoneType,zoneStatus;addrExt_t iasCieAddr;u8 zoneId;} g_zcl_iasZoneAttrs;
typedef struct {u8 dstEp,srcEp,dirCluster;u16 srcAddr;} zclIncomingAddrInfo_t;
typedef struct {struct {u8 dst_ep,src_ep;u16 cluster_id,src_short_addr;} indInfo;} apsdeDataInd_t;
typedef struct {u16 attrID;} zclWriteRec_t;
typedef struct {u8 numAttr;zclWriteRec_t attrList[2];} zclWriteCmd_t;
typedef struct {struct {u8 cmd;struct {struct {u8 dir;} bf;} frmCtrl;} hdr;void *msg,*attrCmd;} zclIncoming_t;
typedef struct {u16 profileId;u8 dstAddrMode,dstEp;union {u16 shortAddr;} dstAddr;} epInfo_t;
typedef struct {u16 zoneType,manufacturerCode;} zoneEnrollReq_t;
typedef struct {u8 code,zoneId;} zoneEnrollRsp_t;
static u8 record[10];static bool present,writeFail,readFail;
static unsigned writes,notifies,enrolls;
static nv_sts_t nv_flashReadNew(u8 a,u8 b,u8 c,u32 n,u8 *out) {(void)a;(void)b;(void)c;assert(n==10);if(readFail)return 2;if(!present)return NV_ITEM_NOT_FOUND;memcpy(out,record,n);return 0;}
static nv_sts_t nv_flashWriteNew(u8 a,u8 b,u8 c,u32 n,u8 *in) {(void)a;(void)b;(void)c;assert(n==10);if(writeFail)return 2;memcpy(record,in,n);present=true;++writes;return 0;}
static void sensor_markPending(void) {++notifies;}
static void sensor_zclIdentifyCmdHandler(u8 ep,u16 addr,u16 time) {(void)ep;(void)addr;(void)time;}
typedef struct {u8 status;addrExt_t ieee_addr_remote;u16 nwk_addr_remote;} zdo_nwk_addr_resp_t;
typedef struct {u8 *zpdu;u8 status,length;} zdo_zdpDataInd_t;
typedef struct {addrExt_t ieee_addr_interest;u8 req_type,start_index;} zdo_nwk_addr_req_t;
static unsigned addressRequests;
static bool lookupMissing;
static void (*addressCallback)(void *);
static u8 zb_zdoNwkAddrReq(u16 dest,zdo_nwk_addr_req_t *request,u8 *sequence,void (*cb)(void *)) {
    (void)sequence;assert(dest==0xfffc && request->req_type==0);
    assert(ZB_IEEE_ADDR_CMP(request->ieee_addr_interest,g_zcl_iasZoneAttrs.iasCieAddr));
    ++addressRequests;addressCallback=cb;return 0;
}
static u8 zb_getNwkAddrByExtAddr(const u8 *ieee,u16 *addr) {(void)ieee;*addr=0;return lookupMissing ? 255 : 0;}
static bool spoof;
static u8 zb_getExtAddrByNwkAddr(u16 addr,u8 *ieee) {(void)addr;memset(ieee,spoof?2:1,8);return 0;}
static void zcl_iasZone_enrollReqCmd(u8 ep,epInfo_t *dst,bool disable,zoneEnrollReq_t *req) {assert(ep==1 && dst->dstEp==1 && disable && req->zoneType==0x15);++enrolls;}
#include "ias_sources.inc"
static void step(u32 ms) {now_ticks+=ms*16000U;sensor_iasTask();}
int main(void) {
    memset(g_zcl_iasZoneAttrs.iasCieAddr,1,8);
    g_zcl_iasZoneAttrs.zoneId=255;
    apsdeDataInd_t ind={{1,1,0x500,0}};
    zclWriteCmd_t attrs={1,{{0x10}}};
    zclIncoming_t msg={0};msg.hdr.cmd=2;msg.msg=&ind;msg.attrCmd=&attrs;
    sensor_zclProcessIncomingMsg(&msg);assert(iasDirty && enrollNeeded);
    writeFail=true;step(1001);assert(iasDirty && writes==0 && enrolls==1);
    writeFail=false;step(1001);assert(!iasDirty && writes==1);
    zclIncomingAddrInfo_t address={1,1,0,0};zoneEnrollRsp_t response={0,23};
    spoof=true;assert(sensor_iasCb(&address,0,&response)==ZCL_STA_FAILURE && !notifies);
    spoof=false;assert(sensor_iasCb(&address,0,&response)==0 && notifies==1);
    step(1001);assert(writes==2 && !iasDirty);
    g_zcl_iasZoneAttrs.zoneStatus=1;step(1001);assert(writes==2); // no writes per contact
    assert(sensor_iasSave()==0 && writes==2); // unchanged configuration is not written
    memset(&g_zcl_iasZoneAttrs,0,sizeof(g_zcl_iasZoneAttrs));
    assert(sensor_iasRestore()==0 && g_zcl_iasZoneAttrs.zoneState==1 && g_zcl_iasZoneAttrs.zoneId==23);
    assert(g_zcl_iasZoneAttrs.iasCieAddr[0]==1 && record[8]==23 && record[9]==1);
    sensor_iasInit();sensor_zclProcessIncomingMsg(&msg);
    assert(g_zcl_iasZoneAttrs.zoneState==1);
    memset(g_zcl_iasZoneAttrs.iasCieAddr,3,8);
    sensor_zclProcessIncomingMsg(&msg);assert(g_zcl_iasZoneAttrs.zoneState==0 && g_zcl_iasZoneAttrs.zoneId==255);
    readFail=true;step(1001);assert(iasDirty);readFail=false;step(1001);assert(!iasDirty && writes==3);
    // Recover a CIE whose short address was lost across a power interruption.
    enrollNeeded=false;lookupMissing=true;unsigned before=enrolls;step(1001);
    assert(addressRequests==1 && enrolls==before);
    zdo_nwk_addr_resp_t answer={0};memset(answer.ieee_addr_remote,4,8);
    zdo_zdpDataInd_t reply={(u8 *)&answer,0,12};
    addressCallback(&reply);assert(!enrollNeeded);
    memset(answer.ieee_addr_remote,3,8);reply.length=2;
    addressCallback(&reply);assert(!enrollNeeded);
    reply.length=12;addressCallback(&reply);assert(enrollNeeded);
    step(1001);assert(enrolls==before+1);
    gSensorCtx.factoryResetPending=true;unsigned old=enrolls;
    // app_task gates IAS work during reset; incoming writes are independently gated.
    iasDirty=false;sensor_zclProcessIncomingMsg(&msg);assert(!iasDirty && enrolls==old);
    puts("PASS: IAS CIE write/enroll, source validation, NV layout/restore/retry, unchanged writes and contact write suppression");
}
