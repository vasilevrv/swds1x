#pragma once

/* GPIO assignment and polarities supplied by the user for this relay board. */
#ifndef RELAY_BOARD_CONFIGURED
#define RELAY_BOARD_CONFIGURED 1
#endif

#define RELAY1_PIN                     GPIO_PC0
#define RELAY1_ACTIVE_LEVEL            1
#define RELAY2_PIN                     GPIO_PD4
#define RELAY2_ACTIVE_LEVEL            1
#define BUTTON1_PIN                    GPIO_PD3
#define BUTTON1_ACTIVE_LEVEL           0
#define BUTTON2_PIN                    GPIO_PD7
#define BUTTON2_ACTIVE_LEVEL           0
#define SERVICE_BUTTON_PIN             GPIO_PC2
#define SERVICE_BUTTON_ACTIVE_LEVEL    0
#define RELAY_LED_PIN                  GPIO_PD2
#define RELAY_LED_ACTIVE_LEVEL         0

/* PC5 selected by the user as unconnected, matching the SDK default.
 * The SDK drives this GPIO high to sample supply voltage for flash writes. */
#define RELAY_VBAT_ADC_PIN              GPIO_PC5
