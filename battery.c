#include "app_cfg.h"
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "app_main.h"
#include "app_ui.h"
#include "sensor_control.h"
#include "battery.h"
uint8_t gSensorBatteryVoltage=0xFF, gSensorBatteryPercent=0xFF;
static u32 lastBatterySample, lastBatteryBusy, batteryWaitStarted, lastReading;
static u32 readingSum;
static u16 readingMin, readingMax;
static u8 readingCount;
static bool batteryPending, startupPending;
#define BATTERY_INTERVAL_MS 43200000U
#define BATTERY_WINDOW_MS 5000U
#define BATTERY_READING_MS 500U
static bool batteryReportPending, batteryReportAttempted;
static u32 lastBatteryReportAttempt;
bool sensor_batteryCanSleep(void) {return !batteryPending;}
static void batteryReportTry(void)
{
    if (!batteryReportPending || !zb_isDeviceJoinedNwk() || !bdb_isIdle() ||
        !zb_bindingTblSearched(ZCL_CLUSTER_GEN_POWER_CFG, 1)) return;
    u32 now=app_nowMs();
    if (batteryReportAttempted && (u32)(now-lastBatteryReportAttempt)<60000U) return;
    batteryReportAttempted=TRUE;
    lastBatteryReportAttempt=now;
    epInfo_t dst={0};
    dst.txOptions=APS_TX_OPT_ACK_TX;
    dst.profileId=HA_PROFILE_ID;
    dst.dstAddrMode=APS_DSTADDR_EP_NOTPRESETNT;
    struct {
        u8 numAttr;
        zclReport_t attrList[2];
    } report={0};
    report.numAttr=2;
    report.attrList[0].attrID=ZCL_ATTRID_BATTERY_VOLTAGE;
    report.attrList[0].dataType=ZCL_DATA_TYPE_UINT8;
    report.attrList[0].attrData=&gSensorBatteryVoltage;
    report.attrList[1].attrID=ZCL_ATTRID_BATTERY_PERCENTAGE_REMAINING;
    report.attrList[1].dataType=ZCL_DATA_TYPE_UINT8;
    report.attrList[1].attrData=&gSensorBatteryPercent;
    if (zcl_sendReportAttrsCmd(1, &dst, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR,
        ZCL_CLUSTER_GEN_POWER_CFG, (zclReportCmd_t *)&report)==ZCL_STA_SUCCESS) {
        batteryReportPending=FALSE;
        batteryReportAttempted=FALSE;
    }
}
/* A short burst rejects ADC startup noise; separate bursts sample supply
 * recovery over time. Power the ADC down between bursts. */
static u16 batteryReadBurst(void)
{
    drv_adc_init();
    drv_adc_mode_pin_set(DRV_ADC_VBAT_MODE, SENSOR_VBAT_ADC_PIN);
    drv_adc_enable(TRUE);
    WaitUs(200);
    (void)sensor_batteryReadMv(); /* Discard the first conversion after power-up. */
    u16 readings[5];
    for (u8 i=0; i<5; ++i) {
        readings[i]=sensor_batteryReadMv();
        for (u8 j=i; j>0 && readings[j]<readings[j-1]; --j) {
            u16 t=readings[j];readings[j]=readings[j-1];readings[j-1]=t;
        }
    }
    drv_adc_enable(FALSE);
    return readings[2];
}
void sensor_batteryInit(void)
{
    gSensorBatteryVoltage=gSensorBatteryPercent=0xFF;
    batteryPending=batteryReportPending=batteryReportAttempted=FALSE;
    startupPending=TRUE;
    lastBatterySample=app_nowMs()-60000U;
    /* Disable persisted SDK schedules: the application owns reporting. */
    const u16 attributes[]={ZCL_ATTRID_BATTERY_VOLTAGE, ZCL_ATTRID_BATTERY_PERCENTAGE_REMAINING};
    for (u8 i=0; i<2; ++i) {
        reportCfgInfo_t *entry=zcl_reportCfgInfoEntryFind(1, ZCL_CLUSTER_GEN_POWER_CFG, attributes[i]);
        if (entry) entry->maxInterval=0xFFFF;
    }
}
void sensor_batteryTask(void)
{
    u32 now=app_nowMs();
    batteryReportTry();
    if (!batteryPending && (u32)(now-lastBatterySample)<
        (startupPending ? 60000U : BATTERY_INTERVAL_MS)) return;
    bool busy=gSensorCtx.timerLedEvt || sensor_ledFadeActive() || tl_stackBusy() || !bdb_isIdle();
    if (!batteryPending) {
        /* Wait on ordinary device wakes; pairing/LED activity does not keep
         * the device awake just to start a battery measurement. */
        if (busy) return;
        batteryPending=TRUE;
        batteryWaitStarted=lastBatteryBusy=now;
        lastReading=now-BATTERY_READING_MS;
        readingSum=readingCount=0;
        readingMin=0xFFFF;readingMax=0;
    }
    if (busy) lastBatteryBusy=now;
    if ((u32)(now-batteryWaitStarted)<BATTERY_WINDOW_MS && !busy &&
        (u32)(now-lastBatteryBusy)>=200U &&
        (u32)(now-lastReading)>=BATTERY_READING_MS) {
        lastReading=now;
        u16 mv=batteryReadBurst();
        if (mv>=1800 && mv<=3600) {
            readingSum+=mv;++readingCount;
            if (mv<readingMin) readingMin=mv;
            if (mv>readingMax) readingMax=mv;
        }
    }
    if ((u32)(now-batteryWaitStarted)>=BATTERY_WINDOW_MS) {
        lastBatterySample=now;
        batteryPending=FALSE;
        if (readingCount>=5) {
            /* Trim the lowest/highest burst, then average the rest. No old
             * boot reading or previous half-day reading biases this result. */
            u16 mv=(readingSum-readingMin-readingMax+(readingCount-2U)/2U)/(readingCount-2U);
            gSensorBatteryVoltage=(mv+50U)/100U;
            gSensorBatteryPercent=sensor_batteryPercent(mv);
            startupPending=FALSE;
            batteryReportPending=TRUE;
            batteryReportAttempted=FALSE;
        }
        /* Invalid startup: retry after a minute on an existing wake. Invalid
         * periodic window: keep the last good value until the next half-day. */
    }
}
