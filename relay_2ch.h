#pragma once

#define RELAY_CHANNELS                2
#define RELAY_2CH_CONFIG_CLUSTER_ID   0xFC00
#define RELAY_ATTR_BUTTON_LINKED      0x0000
#define RELAY_ATTR_DOUBLE_CLICK       0x0001
#define RELAY_CMD_BUTTON_EVENT        0x00

typedef struct {
    u8 buttonLinked;
    u8 doubleClickEnabled;
} relay_config_t;
extern relay_config_t g_relayConfig[RELAY_CHANNELS];

/* -1 for unsupported endpoints, otherwise channel index 0 or 1. */
int relay2_channelFromEndpoint(u8 endpoint);
void relay2_init(void);
void relay2_task(void);
void relay2_saveState(void);
void relay2_configChanged(u8 channel);
void relay2_onRelayStateChanged(u8 channel);
void relay2_buttonEvent(u8 channel, u8 event);
void relay2_factoryReset(void);
void relay2_onOffTimerSync(u8 channel);
