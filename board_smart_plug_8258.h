#pragma once

#include "meter_config.h"

/*
 * TLSR8258 pin assignment traced by the user. ADC PC5 and metering
 * calibration still require confirmation. Both app and bootloader use this file.
 */
/* Relay: initialize inactive before enabling the GPIO output. */
#define SMART_PLUG_RELAY_PIN            GPIO_PB5 /* IC pin 15 */
#define SMART_PLUG_RELAY_ACTIVE_LEVEL   1
#define PB5_FUNC                        AS_GPIO
#define PB5_DATA_OUT                    (!SMART_PLUG_RELAY_ACTIVE_LEVEL)
#define PB5_OUTPUT_ENABLE               1
#define PB5_INPUT_ENABLE                0
#define PULL_WAKEUP_SRC_PB5             PM_PIN_PULLDOWN_100K

/* Single status LED. */
#define SMART_PLUG_LED_PIN              GPIO_PD2 /* IC pin 31 */
#define SMART_PLUG_LED_ACTIVE_LEVEL     0
#define PD2_FUNC                        AS_GPIO
#define PD2_DATA_OUT                    (!SMART_PLUG_LED_ACTIVE_LEVEL)
#define PD2_OUTPUT_ENABLE               1
#define PD2_INPUT_ENABLE                0

/* Local push button, active low with an internal pull-up. */
#define SMART_PLUG_BUTTON_PIN           GPIO_PC4 /* IC pin 24 */
#define SMART_PLUG_BUTTON_ACTIVE_LEVEL  0
#define PC4_FUNC                        AS_GPIO
#define PC4_OUTPUT_ENABLE               0
#define PC4_INPUT_ENABLE                1
#define PULL_WAKEUP_SRC_PC4             (SMART_PLUG_BUTTON_ACTIVE_LEVEL ? PM_PIN_PULLDOWN_100K : PM_PIN_PULLUP_10K)
#define KB_LINE_HIGH_VALID             SMART_PLUG_BUTTON_ACTIVE_LEVEL

/* Meter pin 6 -> PC3: BL0937 CF, or HLW8032 UART TX (4800 8E1). */
#define BL0937_CF_PIN                   GPIO_PC3
#define BL0937_CF1_PIN                  GPIO_PC2
#define BL0937_SEL_PIN                  GPIO_PB4
#define HLW8032_UART_RX_PIN              UART_RX_PC3
#define PC3_FUNC                        AS_GPIO
#define PC3_OUTPUT_ENABLE               0
#define PC3_INPUT_ENABLE                1
#define PULL_WAKEUP_SRC_PC3             PM_PIN_UP_DOWN_FLOAT

/* Meter pin 8 -> PB4: BL0937 SEL output; HLW8032 RX is reserved/unused.
 * PB4 is not a hardware UART TX pin on 8258. Leave it high impedance;
 * receiving metering data does not require transmitting to HLW8032. */
#define HLW8032_RESERVED_RX_PIN         GPIO_PB4
#define HLW8032_UART_TX_PIN             UART_TX_NONE_PIN
#define PB4_FUNC                        AS_GPIO
#define PB4_DATA_OUT                    0
#define PB4_OUTPUT_ENABLE               (SMART_PLUG_METER_TYPE == SMART_PLUG_METER_BL0937)
#define PB4_INPUT_ENABLE                0
#define PULL_WAKEUP_SRC_PB4             PM_PIN_UP_DOWN_FLOAT

/* Component-derived coefficients; still require calibration against a reference.
 * VP series resistance 1.991 MOhm, VP-to-GND 0.998 kOhm = 998 Ohm.
 * voltage coefficient = 1991000 / (998 * 1000) = 1.99498998,
 * rounded to milli = 1995. IP/IN 1 kOhm resistors are input filters. */
#define HLW8032_VOLTAGE_COEFFICIENT_MILLI 1995UL
/* Confirmed shunt marking R001 = 0.001 Ohm:
 * current coefficient = 1 / (Rshunt * 1000) = 1.000. Nominal, not calibrated. */
#define HLW8032_CURRENT_COEFFICIENT_MILLI 1000UL

/* Conservative upper bound for detecting counter discontinuities, not a trip limit. */
#define HLW8032_MAX_POWER_MW             25000000UL

/* SDK flash undervoltage protection needs an externally unconnected ADC pin
 * even when application voltage detection is disabled. PC5 is not exposed on
 * ZT2S; the user chose to retain it for a first bench test. Internal routing
 * and supply-voltage sampling are not confirmed by the module pinout.
 * adc_vbat_pin_init() drives this GPIO high at runtime to sample MCU supply. */
#define VOLTAGE_DETECT_ADC_PIN           GPIO_PC5
#define PC5_FUNC                         AS_GPIO
#define PC5_OUTPUT_ENABLE                0
#define PC5_INPUT_ENABLE                 1
#define PULL_WAKEUP_SRC_PC5              PM_PIN_UP_DOWN_FLOAT

/* Meter pin 7 -> PC2: BL0937 CF1; HLW8032 PF reserved for a future cross-check.
 * Lifetime energy currently uses UART PF REG, not GPIO pulse interrupts. */
#define HLW8032_PF_PIN                   GPIO_PC2
#define PC2_FUNC                        AS_GPIO
#define PC2_OUTPUT_ENABLE               0
#define PC2_INPUT_ENABLE                1
#define PULL_WAKEUP_SRC_PC2             PM_PIN_UP_DOWN_FLOAT

/* Names consumed by the Telink keyboard and debug infrastructure. */
#define BUTTON1                         SMART_PLUG_BUTTON_PIN
#define LED_POWER                       SMART_PLUG_LED_PIN
#define LED_PERMIT                      SMART_PLUG_LED_PIN
#define LED_ON                          SMART_PLUG_LED_ACTIVE_LEVEL
#define LED_OFF                         (!SMART_PLUG_LED_ACTIVE_LEVEL)

enum {
    VK_SW1 = 0x01,
};

#define KB_MAP_NORMAL           { {VK_SW1,}, }
#define KB_MAP_NUM              KB_MAP_NORMAL
#define KB_MAP_FN               KB_MAP_NORMAL
#define KB_DRIVE_PINS           {0}
#define KB_SCAN_PINS            {BUTTON1}
