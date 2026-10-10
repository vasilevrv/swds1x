#pragma once
#include <stdint.h>
#include <stdbool.h>
uint8_t sensor_batteryPercent(uint16_t millivolts);
uint16_t sensor_batteryReadMv(void);
extern uint8_t gSensorBatteryVoltage, gSensorBatteryPercent;
void sensor_batteryInit(void);
void sensor_batteryTask(void);
bool sensor_batteryCanSleep(void);
