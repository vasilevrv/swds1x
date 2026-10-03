#pragma once

/* Local board configuration for the SDK bootloader; SDK sources stay unchanged. */
#define CLOCK_SYS_CLOCK_HZ       48000000
#define PM_ENABLE                0
#define RF_ENABLE                0
#define PA_ENABLE                0
#define GSUART_PRINTF_MODE       0
#define UART_PRINTF_MODE         0
/* Meter pins are reserved. Do not enable the SDK's UART update protocol. */
#define MODULE_UART_ENABLE       0
#define VOLTAGE_DETECT_ENABLE    0
#define FLASH_PROTECT_ENABLE     1
#define MODULE_WATCHDOG_ENABLE   0

#include "version_cfg.h"
#include "../board_smart_plug_8258.h"

typedef enum {
    EV_POLL_UART_PROC,
    EV_POLL_KEY_PRESS,
    EV_POLL_MAX,
} ev_poll_e;
