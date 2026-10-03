/********************************************************************************************************
 * @file    app_ui.c
 *
 * @brief   This is the source file for app_ui
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
#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__)

/**********************************************************************
 * INCLUDES
 */
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "app_main.h"
#include "plug_control.h"
#include "smart_plug.h"
#include "app_ui.h"


/**********************************************************************
 * LOCAL CONSTANTS
 */


/**********************************************************************
 * TYPEDEFS
 */


/**********************************************************************
 * GLOBAL VARIABLES
 */


/**********************************************************************
 * LOCAL FUNCTIONS
 */
void led_on(u32 pin)
{
    drv_gpio_write(pin, LED_ON);
}

void led_off(u32 pin)
{
    drv_gpio_write(pin, LED_OFF);
}

void led_init(void)
{
    led_off(SMART_PLUG_LED_PIN);
}

static s32 factoryResetTimerCb(void *arg)
{
    (void)arg;
    smartPlug_factoryReset();
    zb_factoryReset();
    return -1;
}

static void buttonKeepPressed(u8 keyCode)
{
    if (keyCode == VK_SW1) {
        printf("Local factory reset...\n");
        smartPlug_onOffUpdate(ZCL_CMD_ONOFF_OFF);
        gSmartPlugCtx.factoryResetPending = TRUE;
        statusLed_blinkStart(3, 100, 100);
        if (!TL_ZB_TIMER_SCHEDULE(factoryResetTimerCb, NULL, 700)) {
            factoryResetTimerCb(NULL);
        }
    }
}

static void buttonShortPressed(u8 keyCode)
{
    switch (keyCode) {
        case VK_SW1:
            {
                zcl_onOffAttr_t *pCurOnOffAttr = zcl_onoffAttrGet();
                if (pCurOnOffAttr) {
                    smartPlug_onOffUpdate(pCurOnOffAttr->onOff
                                            ? ZCL_ONOFF_STATUS_OFF
                                            : ZCL_ONOFF_STATUS_ON);
                }
            }
            break;
        default:
            break;
    }
}

static void keyScan_keyReleasedCB(u8 keyCode)
{
    if (gSmartPlugCtx.state == APP_KEY_SHORT_PRESSED) {
        buttonShortPressed(keyCode);
    }
}

static void keyScan_keyPressedCB(u8 keyCode)
{
    if (keyCode == VK_SW1) {
        gSmartPlugCtx.keyPressedTime = clock_time();
    }
}

void app_key_handler(void)
{
    if (gSmartPlugCtx.factoryResetPending) {
        return;
    }
    static u8 valid_keyCode = 0xff;
    if (gSmartPlugCtx.state == APP_KEY_PRESSED) {
        if (clock_time_exceed(gSmartPlugCtx.keyPressedTime, 10 * 1000 * 1000)) {
            buttonKeepPressed(valid_keyCode);
            gSmartPlugCtx.state = APP_KEY_KEEP_PRESSED;
        }
    }

    if (kb_scan_key(0, 1)) {
        printf("keyScan: cnt = %d, key = %d\n", kb_event.cnt, kb_event.keycode[0]);
        if (kb_event.cnt) {
            valid_keyCode = kb_event.keycode[0];
            keyScan_keyPressedCB(valid_keyCode);
            gSmartPlugCtx.state = APP_KEY_PRESSED;
        } else {
            keyScan_keyReleasedCB(valid_keyCode);
            valid_keyCode = 0xff;
            gSmartPlugCtx.state = APP_KEY_RELEASED;
        }
    }
}

#endif  /* __PROJECT_TL_SMART_PLUG__ */
