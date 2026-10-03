#pragma once

typedef struct {
    u32 voltageMv;
    u32 currentMa;
    u32 activePowerMw;
    u32 apparentPowerMva;
    u8 powerFactor;
    u64 energyDeltaMicroWh;
} energy_meter_sample_t;
