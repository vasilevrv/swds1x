#pragma once
#include "relay_pins.h"

#if !RELAY_BOARD_CONFIGURED
#error "Relay GPIOs are not configured. Fill relay_pins.h with confirmed pins and active levels."
#endif
#ifndef RELAY_VBAT_ADC_PIN
#error "Confirm a free floating ADC GPIO and define RELAY_VBAT_ADC_PIN in relay_pins.h."
#endif

/* Generic SDK GPIO defaults keep outputs disabled until board_io_init().
 * Application and bootloader share this configuration and initialization. */
#define VOLTAGE_DETECT_ADC_PIN RELAY_VBAT_ADC_PIN
#define LED_POWER             RELAY_LED_PIN
#define LED_PERMIT            RELAY_LED_PIN
#define LED_ON                RELAY_LED_ACTIVE_LEVEL
#define LED_OFF               (!RELAY_LED_ACTIVE_LEVEL)

/* The SDK keyboard module is unused, but compiled in its common source set. */
#define KB_LINE_HIGH_VALID    SERVICE_BUTTON_ACTIVE_LEVEL
#define KB_MAP_NORMAL         { {1,}, }
#define KB_MAP_NUM            KB_MAP_NORMAL
#define KB_MAP_FN             KB_MAP_NORMAL
#define KB_DRIVE_PINS         {0}
#define KB_SCAN_PINS          {SERVICE_BUTTON_PIN}
