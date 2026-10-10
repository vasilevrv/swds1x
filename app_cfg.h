/********************************************************************************************************
 * @file    app_cfg.h
 *
 * @brief   This is the header file for app_cfg
 *
 * @author  Zigbee Group
 * @date    2021
 *
 * @par     Copyright (c) 2021, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Licensed under the Apache License, Version 2.0 (the "License");
 *          you may not use this file except in compliance with the License.
 *          You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 *          Unless required by applicable law or agreed to in writing, software
 *          distributed under the License is distributed on an "AS IS" BASIS,
 *          WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *          See the License for the specific language governing permissions and
 *          limitations under the License.
 *
 *******************************************************************************************************/
#pragma once

#ifndef __PROJECT_TL_SWDS1X__
#define __PROJECT_TL_SWDS1X__ 0
#endif

/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
extern "C" {
#endif

#define ZB_TEST_ENABLE                          0

/**********************************************************************
 * App configuration
 */
#define TEST_FOR_CERTIFICATION                  0

/* Radio */
#define PM_ENABLE 1
#define RF_ENABLE                               1
#define PA_ENABLE                               0

/* Debug mode */
#define GSUART_PRINTF_MODE                      0//GPIO simulate UART
#define UART_PRINTF_MODE                        0//Hardware UART
//#define USB_PRINTF_MODE                       0

/* HCI interface */
#define	ZBHCI_UART                              0
#define ZBHCI_USB_CDC                           0
#define ZBHCI_USB_HID                           0
#if ZB_TEST_ENABLE
#define ZBHCI_SWIRE_MODE                        1
#endif

/* BDB */
#define TOUCHLINK_SUPPORT                       0
#define FIND_AND_BIND_SUPPORT                   0

/* Voltage detect module */
/* If VOLTAGE_DETECT_ENABLE is set,
 * 1) if MCU_CORE_826x is defined, the DRV_ADC_VBAT_MODE mode is used by default,
 * and there is no need to configure the detection IO port;
 * 2) if MCU_CORE_8258 or MCU_CORE_8278 is defined, the DRV_ADC_VBAT_MODE mode is used by default,
 * we need to configure the detection IO port, and the IO must be in a floating state.
 * 3) if MCU_CORE_B91 is defined, the DRV_ADC_BASE_MODE mode is used by default,
 * we need to configure the detection IO port, and the IO must be connected to the target under test,
 * such as VCC.
 */
#define VOLTAGE_DETECT_ENABLE                   0

/* Flash protect module */
/* Only the firmware area will be locked, the NV data area will not be locked.
 * For details, please refer to drv_flash.c file.
 */
#define FLASH_PROTECT_ENABLE                    1

/* Watch dog module */
#define MODULE_WATCHDOG_ENABLE                  0

/* UART module */
/* No metering UART or UART update protocol. */
#define MODULE_UART_ENABLE 0

/* Console module */
#if (GSUART_PRINTF_MODE || UART_PRINTF_MODE || USB_PRINTF_MODE)
#define CONSOLE_ENABLE                          1
#endif

/* USB module */
#if (ZBHCI_USB_PRINT || ZBHCI_USB_CDC || ZBHCI_USB_HID || USB_PRINTF_MODE)
#define MODULE_USB_ENABLE                       1
#endif

#if (ZBHCI_USB_PRINT || ZBHCI_USB_CDC || ZBHCI_USB_HID || ZBHCI_UART || ZBHCI_SWIRE_MODE)
#define ZBHCI_EN                                1
#endif

/**********************************************************************
 * Board definitions
 */
/* Board ID */
#define BOARD_SENSOR_8258                   18

/* Board define */
#if defined(MCU_CORE_8258)
    #define BOARD                               BOARD_SENSOR_8258
    #define CLOCK_SYS_CLOCK_HZ                  48000000
#else
    #error "swds1x supports TLSR8258 only"
#endif

/**********************************************************************
 * Version configuration
 */
#include "version_cfg.h"

/**********************************************************************
 * Board configuration
 */
#if (BOARD == BOARD_SENSOR_8258)
    #include "board_sensor_8258.h"
#endif

/**********************************************************************
 * Stack configuration
 */
#include "stack_cfg.h"

/**********************************************************************
 * ZCL cluster configuration
 */
#define ZCL_ON_OFF_SUPPORT 0
#define ZCL_GROUP_SUPPORT 0
#define ZCL_SCENE_SUPPORT 0
#define ZCL_ELECTRICAL_MEASUREMENT_SUPPORT      0
#define ZCL_METERING_SUPPORT                    0
#define ZCL_IAS_ZONE_SUPPORT 1
#define ZCL_POLL_CTRL_SUPPORT 0
#define ZCL_POWER_CFG_SUPPORT 1
#define ZCL_OTA_SUPPORT                         1
#define ZCL_GP_SUPPORT                          0
#define ZCL_WWAH_SUPPORT                        0
#if TOUCHLINK_SUPPORT
#define ZCL_ZLL_COMMISSIONING_SUPPORT           1
#endif
/**********************************************************************
 * BLE configuration
 */
#if BLE_CONCURRENT_MODE
#include "app_ble_cfg.h"
#endif

/**********************************************************************
 * EV configuration
 */
typedef enum {
    EV_POLL_ED_DETECT,
    EV_POLL_HCI,
    EV_POLL_IDLE,
    EV_POLL_MAX,
} ev_poll_e;

/* Disable C linkage for C++ Compilers: */
#if defined(__cplusplus)
}
#endif
