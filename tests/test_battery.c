#include "host_sdk.h"
#include "battery.h"
#define SENSOR_VBAT_ADC_PIN 5
typedef u32 GPIO_PinTypeDef;
#define DRV_ADC_VBAT_MODE 1
static u32 nowMs;
static u32 app_nowMs(void) {return nowMs;}
static struct {void *timerLedEvt;} gSensorCtx;
static bool fading,radioBusy,bdbBusy,adcEnabled;
static bool sensor_ledFadeActive(void) {return fading;}
static bool tl_stackBusy(void) {return radioBusy;}
static bool bdb_isIdle(void) {return !bdbBusy;}
static u16 samples[6];
static unsigned adcIndex,adcInitializations;
static void drv_adc_init(void) {++adcInitializations;adcIndex=0;}
static void drv_adc_mode_pin_set(u8 mode,u32 pin) {assert(mode==1 && pin==5);}
static void drv_adc_enable(bool en) {adcEnabled=en;}
static void WaitUs(u32 us) {assert(us>=200);}
uint16_t sensor_batteryReadMv(void) {assert(adcEnabled && adcIndex<6);return samples[adcIndex++];}
#define ZCL_CLUSTER_GEN_POWER_CFG 1
#define ZCL_ATTRID_BATTERY_VOLTAGE 0x20
#define ZCL_ATTRID_BATTERY_PERCENTAGE_REMAINING 0x21
#define ZCL_DATA_TYPE_UINT8 0x20
#define ZCL_DATA_TYPE_UINT16 0x21
#define ZCL_DATA_TYPE_OCTET_STR 0x41
#define ZCL_DATA_TYPE_INT8 0x28
#define ZCL_FRAME_SERVER_CLIENT_DIR 1
#define APS_TX_OPT_ACK_TX 1
#define HA_PROFILE_ID 0x104
#define APS_DSTADDR_EP_NOTPRESETNT 0
#define ZCL_STA_SUCCESS 0
static bool bound;
static unsigned reports;
static int sendStatus;
typedef struct {u8 txOptions;u16 profileId;u8 dstAddrMode;} epInfo_t;
typedef struct {u16 attrID;u8 dataType;u8 *attrData;} zclReport_t;
typedef struct {u8 numAttr;zclReport_t attrList[];} zclReportCmd_t;
typedef struct {u16 maxInterval;} reportCfgInfo_t;
static reportCfgInfo_t oldReports[2];
static reportCfgInfo_t *zcl_reportCfgInfoEntryFind(u8 ep,u16 cluster,u16 attr) {
    assert(ep==1 && cluster==1);
    return &oldReports[attr==0x20 ? 0 : 1];
}
static bool zb_bindingTblSearched(u16 cluster,u8 ep) {assert(cluster==1 && ep==1);return bound;}
static int zcl_sendReportAttrsCmd(u8 ep,epInfo_t *dst,bool noRsp,u8 direction,u16 cluster,zclReportCmd_t *report) {
    assert(ep==1 && dst->profileId==HA_PROFILE_ID && dst->txOptions==APS_TX_OPT_ACK_TX);
    assert(noRsp && direction==1 && cluster==1 && report->numAttr==2);
    assert(report->attrList[0].attrID==0x20 && report->attrList[0].dataType==0x20);
    assert(report->attrList[1].attrID==0x21 && report->attrList[1].dataType==0x20);
    assert(*report->attrList[0].attrData==gSensorBatteryVoltage);
    assert(*report->attrList[1].attrData==gSensorBatteryPercent);
    ++reports;return sendStatus;
}
#include "battery_sources.inc"
static void readings(u16 mv) {for(unsigned i=0;i<6;++i)samples[i]=mv; samples[0]=1800;}
static void finishWindow(u16 mv) {
    readings(mv);
    for(unsigned i=0;i<50;++i) {nowMs+=100;sensor_batteryTask();assert(!adcEnabled);}
    assert(sensor_batteryCanSleep());
    sensor_batteryTask();
}
int main(void) {
    assert(sensor_batteryPercent(2200)==0 && sensor_batteryPercent(3100)==200);
    assert(sensor_batteryPercent(2850)==115 && sensor_batteryPercent(3000)==180);
    for(unsigned mv=1;mv<=65535;++mv) assert(sensor_batteryPercent(mv)>=sensor_batteryPercent(mv-1));
    sensor_batteryInit();assert(adcInitializations==0 && gSensorBatteryVoltage==255);
    assert(oldReports[0].maxInterval==0xFFFF && oldReports[1].maxInterval==0xFFFF);
    bound=true;bdbBusy=true;nowMs=5000;sensor_batteryTask();
    assert(sensor_batteryCanSleep() && adcInitializations==0 && reports==0);
    bdbBusy=false;sensor_batteryTask();assert(!sensor_batteryCanSleep());
    nowMs+=199;sensor_batteryTask();assert(adcInitializations==0);
    nowMs++;readings(3012);sensor_batteryTask();assert(adcInitializations==1 && !adcEnabled);
    // Supply recovery plus one high outlier: trim extremes, average eight.
    for(unsigned i=1;i<10;++i) {
        nowMs+=500;readings(i==1 ? 3500 : 3300);sensor_batteryTask();
    }
    nowMs+=300;sensor_batteryTask();assert(sensor_batteryCanSleep());
    assert(readingCount==10 && gSensorBatteryVoltage==33 && gSensorBatteryPercent==200);
    sensor_batteryTask();assert(reports==1);sensor_batteryTask();assert(reports==1);
    unsigned prior=adcInitializations;
    nowMs+=3600000;sensor_batteryTask();assert(adcInitializations==prior);
    nowMs=lastBatterySample+BATTERY_INTERVAL_MS-1;sensor_batteryTask();assert(adcInitializations==prior);
    nowMs++;sensor_batteryTask();finishWindow(3012);
    assert(adcInitializations==prior+10 && reports==2 && gSensorBatteryVoltage==30 && gSensorBatteryPercent==182);
    // Invalid periodic samples preserve last good data and wait twelve hours.
    nowMs+=BATTERY_INTERVAL_MS;sensor_batteryTask();finishWindow(0);
    assert(reports==2 && gSensorBatteryVoltage==30);
    prior=adcInitializations;nowMs+=60000;sensor_batteryTask();assert(adcInitializations==prior);
    // Radio/LED activity suppresses ADC bursts and cannot extend the window.
    nowMs+=BATTERY_INTERVAL_MS;sensor_batteryTask();radioBusy=true;finishWindow(3300);
    radioBusy=false;assert(adcInitializations==prior && reports==2);
    fading=true;nowMs+=BATTERY_INTERVAL_MS;sensor_batteryTask();assert(sensor_batteryCanSleep());
    fading=false;sensor_batteryTask();finishWindow(2800);assert(gSensorBatteryPercent==90 && reports==3);
    // Clock wrap and a new cold boot discard previous measurements.
    nowMs=UINT32_MAX-1000U;sensor_batteryInit();sensor_batteryTask();finishWindow(2900);
    assert(gSensorBatteryPercent==140 && reports==4);
    // Invalid startup stays unknown; retry on an existing wake after a minute.
    sensor_batteryInit();sensor_batteryTask();finishWindow(0);assert(gSensorBatteryVoltage==255);
    prior=adcInitializations;nowMs+=59999;sensor_batteryTask();assert(adcInitializations==prior);
    nowMs++;sensor_batteryTask();sendStatus=1;finishWindow(3100);
    assert(sensor_batteryCanSleep());prior=reports;
    nowMs+=59999;sensor_batteryTask();assert(reports==prior);
    sendStatus=0;nowMs++;sensor_batteryTask();assert(reports==prior+1);
    sensor_batteryTask();assert(reports==prior+1);
    puts("PASS: five-second quiet sampling, recovery/outlier rejection, ADC shutdown, startup/12-hour reports, invalid/busy windows, retry and clock wrap");
}
