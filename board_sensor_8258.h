#pragma once
#include "sensor_pins.h"
#define VOLTAGE_DETECT_ADC_PIN SENSOR_VBAT_ADC_PIN
#define LED_POWER SENSOR_LED_PIN
#define LED_PERMIT SENSOR_LED_PIN
#define LED_ON 0
#define LED_OFF 1
#define KB_LINE_HIGH_VALID 0
#define KB_MAP_NORMAL {{1,},}
#define KB_MAP_NUM KB_MAP_NORMAL
#define KB_MAP_FN KB_MAP_NORMAL
#define KB_DRIVE_PINS {0}
#define KB_SCAN_PINS {SENSOR_BUTTON_PIN}
