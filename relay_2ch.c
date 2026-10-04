#include "app_cfg.h"
#if (__PROJECT_TL_RELAY_2CH__)
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "app_main.h"
#include "relay_control.h"
#include "relay_2ch.h"
#include "button_engine.h"

#define NV_ITEM_RELAY_STATE  0x71
#define RELAY_NV_MAGIC      0x32594C52UL
#define RELAY_NV_VERSION    1

typedef struct _attribute_packed_ {
    u32 magic;
    u8 version;
    u8 onOff[2], startUpOnOff[2], buttonLinked[2], doubleClickEnabled[2];
} relay_nv_t;

relay_config_t g_relayConfig[2] = {{1, 0}, {1, 0}};
static bool nvDirty;
static bool retryPending;
static u32 lastAttempt;
static relay_nv_t saved;
static bool haveSaved;

int relay2_channelFromEndpoint(u8 endpoint)
{
    return endpoint >= 1 && endpoint <= 2 ? endpoint - 1 : -1;
}

static bool remembersState(u8 channel)
{
    u8 startup = g_zcl_onOffAttrs[channel].startUpOnOff;
    return startup == 2 || startup == ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS;
}

static relay_nv_t snapshot(void)
{
    relay_nv_t state;
    memset(&state, 0, sizeof(state));
    state.magic = RELAY_NV_MAGIC;
    state.version = RELAY_NV_VERSION;
    for (u8 ch = 0; ch < 2; ch++) {
        state.startUpOnOff[ch] = g_zcl_onOffAttrs[ch].startUpOnOff;
        /* Fixed startup modes do not depend on the current relay state.
         * Canonicalize them even when another channel/settings trigger a save. */
        state.onOff[ch] = remembersState(ch) ? g_zcl_onOffAttrs[ch].onOff :
                         (state.startUpOnOff[ch] == 1);
        state.buttonLinked[ch] = g_relayConfig[ch].buttonLinked;
        state.doubleClickEnabled[ch] = g_relayConfig[ch].doubleClickEnabled;
    }
    return state;
}

void relay2_saveState(void)
{
    if (gRelayCtx.factoryResetPending) return;
    relay_nv_t state = snapshot();
    if (haveSaved && !memcmp(&saved, &state, sizeof(state))) {
        nvDirty = FALSE;
        retryPending = FALSE;
        return;
    }
    nvDirty = TRUE;
    if (retryPending && !clock_time_exceed(lastAttempt, 5000000UL)) return;
    lastAttempt = clock_time();
#if NV_ENABLE
    if (nv_flashWriteNew(1, NV_MODULE_APP, NV_ITEM_RELAY_STATE, sizeof(state), (u8 *)&state) == NV_SUCC) {
        saved = state;
        haveSaved = TRUE;
        nvDirty = retryPending = FALSE;
    } else {
        retryPending = TRUE;
    }
#endif
}

void relay2_init(void)
{
    nvDirty = retryPending = haveSaved = FALSE;
    for (u8 ch = 0; ch < 2; ch++) {
        g_relayConfig[ch] = (relay_config_t){1, 0};
        g_zcl_onOffAttrs[ch].onOff = FALSE;
        g_zcl_onOffAttrs[ch].startUpOnOff = ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS;
    }
#if NV_ENABLE
    relay_nv_t state;
    if (nv_flashReadNew(1, NV_MODULE_APP, NV_ITEM_RELAY_STATE, sizeof(state), (u8 *)&state) != NV_SUCC ||
        state.magic != RELAY_NV_MAGIC || state.version != RELAY_NV_VERSION) return;
    for (u8 ch = 0; ch < 2; ch++) {
        if (state.onOff[ch] > 1 || state.buttonLinked[ch] > 1 || state.doubleClickEnabled[ch] > 1 ||
            (state.startUpOnOff[ch] > 2 && state.startUpOnOff[ch] != 0xFF)) return;
    }
    for (u8 ch = 0; ch < 2; ch++) {
        g_zcl_onOffAttrs[ch].onOff = state.onOff[ch];
        g_zcl_onOffAttrs[ch].startUpOnOff = state.startUpOnOff[ch];
        g_relayConfig[ch] = (relay_config_t){state.buttonLinked[ch], state.doubleClickEnabled[ch]};
    }
    saved = state;
    haveSaved = TRUE;
#endif
}

void relay2_task(void)
{
    if (nvDirty) relay2_saveState();
}

void relay2_configChanged(u8 channel)
{
    if (channel < 2) relay2_saveState();
}

void relay2_onRelayStateChanged(u8 channel)
{
    if (channel < RELAY_CHANNELS && remembersState(channel)) relay2_saveState();
    /* Do not override network/Identify/reset indication with click feedback. */
}

void relay2_buttonEvent(u8 channel, u8 event)
{
    if (channel >= 2 || event > BUTTON_HOLD || gRelayCtx.factoryResetPending) return;
    if (event == BUTTON_SINGLE && g_relayConfig[channel].buttonLinked) {
        relay2_onOffUpdate(channel, ZCL_CMD_ONOFF_TOGGLE);
    }
    if (!zb_isDeviceJoinedNwk()) return;
    epInfo_t dst;
    TL_SETSTRUCTCONTENT(dst, 0);
    dst.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dst.txOptions = APS_TX_OPT_ACK_TX;
    dst.dstAddr.shortAddr = 0x0000;
    dst.dstEp = 1;
    dst.profileId = HA_PROFILE_ID;
    /* Every event gets a new ZCL sequence number, including identical clicks. */
    zcl_sendCmd(channel + 1, &dst, RELAY_2CH_CONFIG_CLUSTER_ID, RELAY_CMD_BUTTON_EVENT,
                TRUE, ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, MANUFACTURER_CODE_NONE,
                ZCL_SEQ_NUM, 1, &event);
}

static s32 factoryResetCb(void *arg)
{
    (void)arg;
    /* SDK factory reset clears APP NV as well as network/ZCL records. */
    zb_factoryReset();
    return -1;
}

void relay2_factoryReset(void)
{
    if (gRelayCtx.factoryResetPending) return;
    gRelayCtx.factoryResetPending = TRUE;
    relay2_cancelTimedOff();
    relay2_identifyStop();
    for (u8 ch = 0; ch < 2; ch++) relay2_onOffUpdate(ch, ZCL_CMD_ONOFF_OFF);
    statusLed_blinkStart(3, 100, 100);
    if (!TL_ZB_TIMER_SCHEDULE(factoryResetCb, NULL, 700)) factoryResetCb(NULL);
}
#endif
