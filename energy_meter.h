#pragma once
#include "meter_config.h"
#include "energy_meter_types.h"

#if SMART_PLUG_METER_TYPE == SMART_PLUG_METER_BL0937
#include "bl0937.h"
#define energy_meter_init bl0937_init
#define energy_meter_task bl0937_task
#define energy_meter_is_online bl0937_is_online
#define energy_meter_reset_baseline bl0937_reset_energy_baseline
#else
#include "hlw8032.h"
#define energy_meter_init hlw8032_init
#define energy_meter_task hlw8032_task
#define energy_meter_is_online hlw8032_isOnline
#define energy_meter_reset_baseline hlw8032_resetEnergyBaseline
#endif
