#pragma once
void statusLed_blinkStart(u16 times, u16 onTimeMs, u16 offTimeMs);
void statusLed_blinkStop(void);
void sensor_ledRestore(void);
void sensor_ledFadeStart(void);
bool sensor_ledFadeActive(void);
void sensor_factoryReset(void);
void sensor_notifyTask(void);
void sensor_markPending(void);
