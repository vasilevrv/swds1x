#include "host_sdk.h"
static u32 app_nowMs(void) {return now_ticks / 16000U;}
#define REJOIN_SECURITY 1
#define BDB_STATE_IDLE 0
typedef struct { s32 (*cb)(void *); } ev_timer_event_t;
static ev_timer_event_t timers[8];
static u8 timer_count;
static bool timer_fail, factory_new=true, joined;
static u32 steering, rejoining;
static u8 bdb_state;
static struct { u8 scanDuration; } g_bdbAttrs;
#define BDB_STATE_GET() bdb_state
static bool zb_isDeviceFactoryNew(void) { return factory_new; }
/* Override the host header's default joined result for network scenarios. */
static bool network_joined(void) { return joined; }
#define zb_isDeviceJoinedNwk() network_joined()
static u16 zb_random(void) { return 0; }
static void bdb_networkSteerStart(void) { steering++; }
static void zb_rejoinSecModeSet(bool mode) { (void)mode; }
static u32 zb_apsChannelMaskGet(void) { return 1; }
static void zb_rejoinReq(u32 channels,u8 duration) { (void)channels; (void)duration; rejoining++; }
static bool ss_securityModeIsDistributed(void) { return false; }
static ev_timer_event_t *schedule(s32 (*cb)(void *),void *arg,u32 period) {
    (void)arg; assert(period && period<=60000);
    if (timer_fail) return NULL;
    assert(timer_count<8); timers[timer_count].cb=cb; return &timers[timer_count++];
}
#define TL_ZB_TIMER_SCHEDULE(cb,arg,period) schedule(cb,arg,period)
#include "network_sources.inc"

int main(void) {
    timer_fail=true; sensor_classicJoinStart(); assert(steerRetry && !sensor_networkCanSleep());
    now_ticks+=1000001U*16; sensor_networkTask(); assert(steerRetry && !sensor_networkCanSleep());
    timer_fail=false; now_ticks+=1000001U*16; sensor_networkTask();
    assert(!steerRetry && steerTimerEvt && sensor_networkCanSleep());
    assert(timers[0].cb(NULL)==-1 && steering==1 && !steerTimerEvt);
    factory_new=false; timer_fail=true; sensor_rejoinTimerStart(); assert(rejoinRetry);
    timer_fail=false; now_ticks+=1000001U*16; sensor_networkTask();
    assert(!rejoinRetry && rejoinBackoffTimerEvt);
    bdb_state=1; assert(timers[1].cb(NULL)==0 && !rejoining);
    bdb_state=0; assert(timers[1].cb(NULL)==0 && rejoining==1);
    joined=true; assert(timers[1].cb(NULL)==-1 && !rejoinBackoffTimerEvt);
    puts("PASS: pairing/rejoin timer allocation retry, zero RNG, busy commissioning, joined stop");
    return 0;
}
