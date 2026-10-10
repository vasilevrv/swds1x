#include "host_sdk.h"
#define ZBHCI_EN 0
#define EV_POLL_IDLE 2
typedef struct {u8 unused;} af_simple_descriptor_t;
static af_simple_descriptor_t sensor_simpleDesc;
static struct {struct {struct {u8 keyType;u8 *key;} tcLinkKey;} linkKey;} g_bdbCommissionSetting;
static u8 g_zbDemoBdbCb;
static struct {struct {u8 keyType,key[16];} tcLinkKey;} gSensorCtx;
#define RET_OK 0
static unsigned gpioInit,ledRestore,phyRestore,clockResume,stackInit,attrsInit,buttonsInit,pollRegister,bdbInit;
static unsigned batteryInit;
static void sensor_batteryInit(void) {++batteryInit;}
static bool deepWake;
static u8 repowerArg;
static void board_io_init(void) {++gpioInit;}
static void sensor_ledRestore(void) {++ledRestore;}
static void mac_phyReconfig(void) {++phyRestore;}
static void app_clockResume(void) {++clockResume;}
static void sensorSysException(void) {}
static void sys_exceptHandlerRegister(void (*cb)(void)) {assert(cb==sensorSysException);}
static void led_init(void) {}
static void stack_init(void) {++stackInit;}
static void user_app_init(void) {++attrsInit;}
static void app_buttons_init(void) {++buttonsInit;}
static void app_task(void) {}
static void ev_on_poll(u8 id,void (*cb)(void)) {assert(id==EV_POLL_IDLE && cb==app_task);++pollRegister;}
static u8 bdb_preInstallCodeLoad(u8 *type,u8 *key) {(void)type;(void)key;return 1;}
static bool drv_pm_deepSleep_flag_get(void) {return deepWake;}
static void bdb_init(af_simple_descriptor_t *desc,void *setting,void *callbacks,u8 repower) {
    assert(desc==&sensor_simpleDesc && setting==&g_bdbCommissionSetting && callbacks==&g_zbDemoBdbCb);
    ++bdbInit;repowerArg=repower;
}
#include "startup_sources.inc"
int main(void) {
    user_init(false);
    assert(gpioInit==1 && stackInit==1 && attrsInit==1 && buttonsInit==1 && pollRegister==1 && bdbInit==1 && repowerArg==1);
    for(u8 i=0;i<20;++i) user_init(true);
    assert(gpioInit==21 && ledRestore==20 && phyRestore==20 && clockResume==20);
    assert(batteryInit==1);
    assert(stackInit==1 && attrsInit==1 && buttonsInit==1 && pollRegister==1 && bdbInit==1);
    deepWake=true;user_init(false);
    assert(bdbInit==2 && repowerArg==0 && stackInit==2 && batteryInit==2);
    puts("PASS: cold boot, twenty retention wakes without stack/input reset, deep-wake BDB repower flag");
}
