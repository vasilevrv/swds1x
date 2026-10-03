#pragma once

#define HLW8032_FRAME_SIZE 24

#include "energy_meter_types.h"
/* Compatibility for the retained HLW8032 driver and its existing host checks. */
typedef energy_meter_sample_t hlw8032_sample_t;

void hlw8032_init(void);
void hlw8032_task(void);
bool hlw8032_isOnline(void);
void hlw8032_resetEnergyBaseline(void);
