#pragma once

void relay_init(void);
void relay_restore(void);
void relay_set(bool on);

void statusLed_blinkStart(u16 times, u16 onTimeMs, u16 offTimeMs);
void statusLed_blinkStop(void);
